#pragma once

#include <esp_err.h>
#include <esp_http_server.h>
#include <nlohmann/json.hpp>

#include <functional>
#include <string_view>

namespace Ryk
{
  class Model;

  class Request
  {
  public:
    esp_err_t ReceiveJson(nlohmann::json& body);
    esp_err_t SendJson(const nlohmann::json& body);

    esp_err_t ReceiveBody(std::string& buf, size_t mazSize);

    esp_err_t SendText(const char* body);

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
    ~HttpServer();

    // Start listening of a port and handling requests
    esp_err_t Start();

    esp_err_t AddHandler(http_method method, const char* path, HttpHandler&& handler);

    esp_err_t AddCommonHeader(std::string_view header, std::string_view value);

  private:
    static esp_err_t ProcessRequestFunc(httpd_req_t* req);
    esp_err_t ProcessRequest(httpd_req_t* req);
    esp_err_t SetCommonHeaders(httpd_req_t* req);
    static void WorkerTask(void *p);

  private:
    httpd_handle_t hd = nullptr;
    std::map<std::string, HttpHandler> Handlers;
    std::map<std::string, std::string> Headers;

    QueueHandle_t RequestQueue = nullptr;
    const size_t QueueSize = 3;
    TaskHandle_t WorkerTaskHandle;
  };

  esp_err_t StartHttpServer(Model& model);
}
