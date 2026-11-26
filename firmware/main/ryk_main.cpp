#include "ryk_model.h"
#include "ryk_http_server.h"
#include "ryk_network.h"

#include <esp_check.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <nvs_flash.h>

static const char* TAG = "Ryk";

class NVSHolder
{
public:
  esp_err_t Init()
  {
    return nvs_flash_init();
  }

  ~NVSHolder()
  {
    nvs_flash_deinit();
  }
};

static Ryk::Model model;
static NVSHolder nvs;


static esp_err_t HelloHandler(Ryk::Request& req)
{
  return req.SendText("Hello");
}

static esp_err_t BuzzerHandler(Ryk::Request& req)
{
  ESP_LOGI(TAG, "BUZZZZZZZZZZZZ");
  req.SendText("");
  return ESP_OK;
}

static esp_err_t PostModelFunc(Ryk::Request& req)
{
  std::string body;
  esp_err_t err = req.ReceiveBody(body, 1024*1024);
  if (err == ESP_ERR_INVALID_SIZE)
  {
    return req.SendError(HTTPD_400_BAD_REQUEST, "Invalid 'Content-Length'");
  }

  ESP_RETURN_ON_ERROR(err, TAG, "%i - Failed to receive body", err);

  err = model.UpdateModel(body.data(), body.size());
  if (err == ESP_ERR_INVALID_ARG)
    return req.SendError(HTTPD_400_BAD_REQUEST, "Invalid input image");
  else if (err != ESP_OK)
    return req.SendError(HTTPD_500_INTERNAL_SERVER_ERROR, "Internal Error");


  return req.SendText("");
}

Ryk::HttpServer Server;

extern "C"
void app_main(void)
{
  esp_err_t err = ESP_OK;

  err = nvs.Init();
  ESP_RETURN_VOID_ON_ERROR(err, TAG, "%i - Failed to init NVS", err);

  err = model.Init();
  ESP_RETURN_VOID_ON_ERROR(err, TAG, "Failed to init model");

  err = Ryk::StartNetwork();
  ESP_RETURN_VOID_ON_ERROR(err, TAG, "Failed to start network");

  Server.Start();
  Server.AddCommonHeader("Access-Control-Allow-Origin", "*");
  Server.AddHandler(http_method::HTTP_GET, "/", HelloHandler);
  Server.AddHandler(http_method::HTTP_POST, "/buzz", BuzzerHandler);
}
