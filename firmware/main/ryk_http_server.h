#pragma once

#include <esp_err.h>
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace Ryk
{
  class Model;

  class Request
  {
  public:
    // Require application/json (case-insensitive, parameters allowed), then
    // read up to 16 KiB and parse any JSON value; output is null on failure.
    // Missing/wrong Content-Type or invalid JSON returns ESP_ERR_INVALID_ARG;
    // header/body-read errors are propagated.
    esp_err_t ReceiveJson(nlohmann::json& body);
    esp_err_t SendJson(const nlohmann::json& body);

    esp_err_t ReceiveBody(std::string& buf, size_t mazSize);

    esp_err_t SendText(const char* body);

    // Send an HTTP error response and return its transmission result.
    // A null text body selects ESP-IDF's default message for the error.
    // The JSON overload uses application/json and replaces invalid UTF-8.
    esp_err_t SendError(httpd_err_code_t error, const char* body);
    esp_err_t SendError(httpd_err_code_t error, const nlohmann::json& body);

    private:
    Request(httpd_req_t* req)
      : RawRequest(req)
    {
    }

    friend class HttpServer;

  private:
    httpd_req_t* RawRequest = nullptr;
  };

  using HttpHandler = std::function<esp_err_t(Request&)>;
  class HttpServer
  {
  public:
    HttpServer() = default;
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&) = delete;
    HttpServer& operator=(HttpServer&&) = delete;

    // Start listening of a port and handling requests
    esp_err_t Start();

    // Gracefully finish accepted requests and release the server and queue.
    // Call Start/Stop and registration methods serially from an owner task.
    // Stop must not run on the HTTP server task or its request worker.
    // Successful Stop clears handlers/headers and may be called repeatedly.
    esp_err_t Stop();

    esp_err_t AddHandler(http_method method, const char* path, HttpHandler&& handler);

    esp_err_t AddCommonHeader(std::string_view header, std::string_view value);

  private:
    static esp_err_t ProcessRequestFunc(httpd_req_t* req);
    esp_err_t ProcessRequest(httpd_req_t* req);
    esp_err_t SetCommonHeaders(httpd_req_t* req);
    static void WorkerTask(void *p);
    esp_err_t StopWorkerTask();

  private:
    httpd_handle_t hd = nullptr;
    std::map<std::string, HttpHandler> Handlers;
    std::map<std::string, std::string> Headers;
    // Sequential consistency orders admission against the shutdown transition.
    std::atomic<bool> Stopping{true};
    std::atomic<std::uint32_t> ActiveSubmissions{0};
    std::atomic<bool> WorkerStopped{true};

    QueueHandle_t RequestQueue = nullptr;
    const size_t QueueSize = 3;
    TaskHandle_t WorkerTaskHandle = nullptr;
  };
}
