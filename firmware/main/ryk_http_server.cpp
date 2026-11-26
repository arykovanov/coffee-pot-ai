#include "ryk_http_server.h"
#include "ryk_model.h"

#include "freertos/FreeRTOS.h"
#include <esp_check.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>
#include <sstream>


static const char* TAG = "HTTP";
static const size_t MAX_BODY_SIZE = 16384;
const int ASYNC_WORKER_TASK_STACK_SIZE = 2560;
const int ASYNC_WORKER_TASK_PRIORITY = 5;


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
  }

  esp_err_t HttpServer::Start()
  {
    RequestQueue = xQueueCreate(QueueSize, sizeof(httpd_req_t*));
    if (RequestQueue == nullptr)
    {
      ESP_LOGE(TAG, "Failed to create HTTP request queue");
      return ESP_FAIL;
    }

    const auto result = xTaskCreate(
      &WorkerTask,
      "HttpServer",
      ASYNC_WORKER_TASK_STACK_SIZE,
      this,
      ASYNC_WORKER_TASK_PRIORITY,
      &WorkerTaskHandle
    );

    if (result != pdPASS)
      return ESP_FAIL;


    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    esp_err_t err = ESP_OK;

    err = httpd_start(&hd, &config);
    if (err != ESP_OK)
      ESP_LOGE(TAG, "Failed to start HTTP server.");


    return err;
  }

  esp_err_t HttpServer::ProcessRequestFunc(httpd_req_t* req)
  {
    HttpServer* self = reinterpret_cast<HttpServer*>(req->user_ctx);

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

    vTaskDelete(nullptr);
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


esp_err_t Ryk::StartHttpServer(Model& model)
{
  httpd_handle_t hd = nullptr;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  esp_err_t err = ESP_OK;

  err = httpd_start(&hd, &config);
  if (err != ESP_OK)
    ESP_LOGE(TAG, "Failed to start HTTP server.");

  if (err != ESP_OK && hd != nullptr)
    httpd_stop(hd);

  return err;
}
