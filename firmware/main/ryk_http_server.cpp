#include "ryk_http_server.h"

#include "freertos/FreeRTOS.h"
#include <esp_check.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>


static const char* TAG = "HTTP";
static const size_t MAX_BODY_SIZE = 16384;
const int ASYNC_WORKER_TASK_STACK_SIZE = 2560;
const int ASYNC_WORKER_TASK_PRIORITY = 5;

namespace
{
  const char* GetErrorStatus(httpd_err_code_t error)
  {
    switch (error)
    {
      case HTTPD_501_METHOD_NOT_IMPLEMENTED: return "501 Not Implemented";
      case HTTPD_505_VERSION_NOT_SUPPORTED: return "505 HTTP Version Not Supported";
      case HTTPD_400_BAD_REQUEST: return "400 Bad Request";
      case HTTPD_401_UNAUTHORIZED: return "401 Unauthorized";
      case HTTPD_403_FORBIDDEN: return "403 Forbidden";
      case HTTPD_404_NOT_FOUND: return "404 Not Found";
      case HTTPD_405_METHOD_NOT_ALLOWED: return "405 Method Not Allowed";
      case HTTPD_408_REQ_TIMEOUT: return "408 Request Timeout";
      case HTTPD_414_URI_TOO_LONG: return "414 URI Too Long";
      case HTTPD_411_LENGTH_REQUIRED: return "411 Length Required";
      case HTTPD_413_CONTENT_TOO_LARGE: return "413 Content Too Large";
      case HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE: return "431 Request Header Fields Too Large";
      case HTTPD_500_INTERNAL_SERVER_ERROR:
      default: return "500 Internal Server Error";
    }
  }

  // Balance every producer callback, including error and rejection paths.
  class SubmissionGuard
  {
  public:
    explicit SubmissionGuard(std::atomic<std::uint32_t>& counter)
      : Counter(counter)
    {
      Counter.fetch_add(1);
    }

    ~SubmissionGuard()
    {
      Counter.fetch_sub(1);
    }

    SubmissionGuard(const SubmissionGuard&) = delete;
    SubmissionGuard& operator=(const SubmissionGuard&) = delete;

  private:
    std::atomic<std::uint32_t>& Counter;
  };
}


static esp_err_t GetContentLength(httpd_req_t* req, size_t* contentLength)
{
  if (req == nullptr || contentLength == nullptr)
    return ESP_ERR_INVALID_ARG;

  const size_t headerLen = httpd_req_get_hdr_value_len(req, "Content-Length");
  if (headerLen == 0)
    return ESP_ERR_INVALID_SIZE;

  std::string headerValue(headerLen + 1, '\0');
  esp_err_t err = httpd_req_get_hdr_value_str(req, "Content-Length", &headerValue[0], headerValue.size());
  ESP_RETURN_ON_ERROR(err, TAG, "%i - Failed to get 'Content-Length'", err);

  size_t bodySize = 0;
  const char* end = headerValue.data() + headerLen;
  const auto result = std::from_chars(headerValue.data(), end, bodySize);
  if (result.ec != std::errc{} || result.ptr != end)
    return ESP_ERR_INVALID_SIZE;

  *contentLength = bodySize;
  return ESP_OK;
}

static esp_err_t SendBody(httpd_req_t* req, const uint8_t* data, size_t dataSize)
{
  if (req == nullptr || (data == nullptr && dataSize != 0))
    return ESP_ERR_INVALID_ARG;

  if (dataSize > static_cast<size_t>(std::numeric_limits<ssize_t>::max()))
    return ESP_ERR_INVALID_SIZE;

  return httpd_resp_send(req, reinterpret_cast<const char*>(data), static_cast<ssize_t>(dataSize));
}

namespace Ryk
{
  esp_err_t Request::ReceiveJson(nlohmann::json& body)
  {
    body = nullptr;
    const size_t headerLen = httpd_req_get_hdr_value_len(RawRequest, "Content-Type");
    if (headerLen == 0)
      return ESP_ERR_INVALID_ARG;

    std::string contentType(headerLen + 1, '\0');
    const esp_err_t headerErr = httpd_req_get_hdr_value_str(
        RawRequest, "Content-Type", contentType.data(), contentType.size());
    if (headerErr != ESP_OK)
      return headerErr;
    contentType.resize(headerLen);

    // Media types are case-insensitive; parameters do not change the type.
    auto mediaType = std::string_view(contentType).substr(0, contentType.find(';'));
    const auto first = mediaType.find_first_not_of(" \t");
    if (first == std::string_view::npos)
      return ESP_ERR_INVALID_ARG;
    mediaType.remove_prefix(first);
    mediaType = mediaType.substr(0, mediaType.find_last_not_of(" \t") + 1);

    constexpr std::string_view expectedType = HTTPD_TYPE_JSON;
    if (mediaType.size() != expectedType.size()
        || !std::equal(mediaType.begin(), mediaType.end(), expectedType.begin(),
                       [](unsigned char actual, char expected)
                       { return std::tolower(actual) == expected; }))
      return ESP_ERR_INVALID_ARG;

    std::string payload;
    const esp_err_t err = ReceiveBody(payload, MAX_BODY_SIZE);
    if (err != ESP_OK)
      return err;

    // The parser treats a raw NUL as end-of-input, but JSON forbids raw NULs.
    if (payload.find('\0') != std::string::npos)
      return ESP_ERR_INVALID_ARG;

    auto parsed = nlohmann::json::parse(payload, nullptr, false);
    if (parsed.is_discarded())
      return ESP_ERR_INVALID_ARG;

    body.swap(parsed);
    return ESP_OK;
  }

  esp_err_t Request::SendJson(const nlohmann::json& body)
  {
    return ESP_OK;
  }

  esp_err_t Request::SendText(const char* body)
  {
    return httpd_resp_send(RawRequest, body, HTTPD_RESP_USE_STRLEN);
  }

  esp_err_t Request::SendError(httpd_err_code_t error, const char* body)
  {
    return httpd_resp_send_err(RawRequest, error, body);
  }

  esp_err_t Request::SendError(httpd_err_code_t error, const nlohmann::json& body)
  {
    if (body.is_discarded())
      return ESP_ERR_INVALID_ARG;

    // Replace invalid UTF-8 rather than throwing in this exception-free build.
    const std::string payload = body.dump(-1, ' ', false,
                                         nlohmann::json::error_handler_t::replace);
    esp_err_t err = httpd_resp_set_status(RawRequest, GetErrorStatus(error));
    if (err != ESP_OK)
      return err;

    err = httpd_resp_set_type(RawRequest, HTTPD_TYPE_JSON);
    if (err != ESP_OK)
      return err;

    return httpd_resp_send(RawRequest, payload.data(), payload.size());
  }

  esp_err_t Request::ReceiveBody(std::string& body, size_t maxSize)
  {
    body.clear();
    size_t contentLength = 0;
    esp_err_t err = GetContentLength(RawRequest, &contentLength);
    ESP_RETURN_ON_ERROR(err, TAG, "%i - Failed to get content length", err);

    if (contentLength > maxSize)
      return ESP_ERR_INVALID_SIZE;

    body.resize(contentLength);
    size_t receivedSize = 0;
    while (receivedSize < contentLength)
    {
      const size_t readSize = std::min(contentLength - receivedSize,
                                    static_cast<size_t>(std::numeric_limits<int>::max()));
      const int received = httpd_req_recv(RawRequest, reinterpret_cast<char*>(body.data() + receivedSize), readSize);
      if (received <= 0)
      {
        body.clear();
        return received == HTTPD_SOCK_ERR_TIMEOUT ? ESP_ERR_TIMEOUT : ESP_FAIL;
      }

      receivedSize += static_cast<size_t>(received);
    }

    return ESP_OK;
  }

  HttpServer::~HttpServer()
  {
    // Returning after a failed stop would leave callbacks using a dead object.
    ESP_ERROR_CHECK(Stop());
  }

  esp_err_t HttpServer::Stop()
  {
    if (WorkerTaskHandle != nullptr && WorkerTaskHandle == xTaskGetCurrentTaskHandle())
      return ESP_ERR_INVALID_STATE;

    Stopping.store(true);
    // Callbacks admitted before the transition must finish enqueueing first.
    // Later callbacks reject requests without touching the work queue.
    while (ActiveSubmissions.load() != 0)
      vTaskDelay(1);

    if (WorkerTaskHandle != nullptr)
    {
      if (RequestQueue == nullptr)
        return ESP_ERR_INVALID_STATE;

      const esp_err_t err = StopWorkerTask();
      if (err != ESP_OK)
        return err;
    }

    if (hd != nullptr)
    {
      // Async completion needs HTTP server/session state, so stop HTTP last.
      const esp_err_t err = httpd_stop(hd);
      if (err != ESP_OK)
        return err;
      hd = nullptr;
    }

    if (RequestQueue != nullptr)
    {
      vQueueDelete(RequestQueue);
      RequestQueue = nullptr;
    }

    Handlers.clear();
    Headers.clear();
    return ESP_OK;
  }

  esp_err_t HttpServer::Start()
  {
    if (hd != nullptr || RequestQueue != nullptr || WorkerTaskHandle != nullptr)
      return ESP_ERR_INVALID_STATE;

    RequestQueue = xQueueCreate(QueueSize, sizeof(httpd_req_t*));
    if (RequestQueue == nullptr)
    {
      ESP_LOGE(TAG, "Failed to create HTTP request queue");
      return ESP_ERR_NO_MEM;
    }

    WorkerStopped.store(false);
    const auto result = xTaskCreate(
        &WorkerTask,
        "HttpServer",
        ASYNC_WORKER_TASK_STACK_SIZE,
        this,
        ASYNC_WORKER_TASK_PRIORITY,
        &WorkerTaskHandle
      );
    if (result != pdPASS)
    {
      WorkerStopped.store(true);
      const esp_err_t cleanupErr = Stop();
      return cleanupErr == ESP_OK ? ESP_ERR_NO_MEM : cleanupErr;
    }

    Stopping.store(false);
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    esp_err_t err = ESP_OK;
    err = httpd_start(&hd, &config);
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "Failed to start HTTP server.");
      const esp_err_t cleanupErr = Stop();
      if (cleanupErr != ESP_OK)
        return cleanupErr;
    }

    return err;
  }

  esp_err_t HttpServer::StopWorkerTask()
  {
    // FIFO ordering puts the marker after all admitted requests.
    httpd_req_t* req = nullptr;
    if (xQueueSend(RequestQueue, &req, portMAX_DELAY) != pdPASS)
      return ESP_FAIL;

    while (!WorkerStopped.load())
      vTaskDelay(1);

    WorkerTaskHandle = nullptr;
    return ESP_OK;
  }

  void HttpServer::WorkerTask(void *p)
  {
    HttpServer* self = reinterpret_cast<HttpServer*>(p);
    while (true)
    {
      httpd_req_t* req = nullptr;
      if (xQueueReceive(self->RequestQueue, &req, portMAX_DELAY) == pdPASS)
      {
        if (req == nullptr)
          break;

        esp_err_t err = self->ProcessRequest(req);
        if (err != ESP_OK)
          ESP_LOGE(TAG, "Failed to process request");

        if (const esp_err_t e = httpd_req_async_handler_complete(req))
          ESP_LOGE(TAG, "Failed to complete async request: %i", e);
      }
    }

    // Last access to self: the owner may now destroy the server and queue.
    self->WorkerStopped.store(true);
    vTaskDelete(nullptr);
  }

  esp_err_t HttpServer::ProcessRequestFunc(httpd_req_t* req)
  {
    HttpServer* self = reinterpret_cast<HttpServer*>(req->user_ctx);

    SubmissionGuard submission(self->ActiveSubmissions);
    if (self->Stopping.load())
    {
      esp_err_t err = self->SetCommonHeaders(req);
      if (err == ESP_OK)
        err = httpd_resp_set_status(req, "503 Service Unavailable");
      if (err == ESP_OK)
        err = httpd_resp_sendstr(req, "Server is shutting down.");
      return err;
    }

    httpd_req_t* copy = NULL;
    esp_err_t err = httpd_req_async_handler_begin(req, &copy);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to create async request");

    if (xQueueSend(self->RequestQueue, &copy, pdMS_TO_TICKS(100)) != pdPASS)
    {
      ESP_LOGE(TAG, "Failed to post async request to queue");
      if (const esp_err_t err = httpd_req_async_handler_complete(copy))
        ESP_LOGE(TAG, "Async cleanup failed: %s", esp_err_to_name(err));

      err = self->SetCommonHeaders(req);
      if (err == ESP_OK)
        err = httpd_resp_set_status(req, "503 Service unavailable");

      if (err == ESP_OK)
        err = httpd_resp_sendstr(req, "Server busy. try again later");

      return err;
    }

    return ESP_OK;
  }

  esp_err_t HttpServer::ProcessRequest(httpd_req_t* req)
  {
    Request r(req);
    HttpHandler handler = Handlers.at(req->uri);
    SetCommonHeaders(req);
    esp_err_t err = handler(r);
    return err;
  }

  esp_err_t HttpServer::AddHandler(http_method method, const char* path, HttpHandler&& handler)
  {
    auto mapIt = Handlers.emplace(path, handler);
    if (!mapIt.second)
      return ESP_ERR_INVALID_ARG;

    const httpd_uri_t h =
    {
      .uri = mapIt.first->first.c_str(),
      .method = method,
      .handler = ProcessRequestFunc,
      .user_ctx = this
    };

    esp_err_t err = httpd_register_uri_handler(hd, &h);
    if (err != ESP_OK)
      ESP_LOGE(TAG, "Failed to register handler GET '%s'", h.uri);

    return err;
  }

  esp_err_t HttpServer::AddCommonHeader(std::string_view header, std::string_view value)
  {
    if (!Headers.emplace(header, value).second)
      return ESP_ERR_INVALID_ARG;

    return ESP_OK;
  }

  esp_err_t HttpServer::SetCommonHeaders(httpd_req_t* req)
  {
    for (const auto& hdrIt : Headers)
    {
      const char* hdr = hdrIt.first.c_str();
      const char* value = hdrIt.second.c_str();
      const esp_err_t err = httpd_resp_set_hdr(req, hdr, value);
      ESP_RETURN_ON_ERROR(err, TAG, "Failed to set header '%s'", hdr);
    }

    return ESP_OK;
  }

} // namespace Ryk
