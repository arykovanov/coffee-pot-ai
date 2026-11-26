idf_component_register(
    SRCS
        "ryk_main.cpp"
        "ryk_network.cpp"
        "ryk_http_server.cpp"
        "ryk_model.cpp"
        "ryk_model_data.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_http_server
        esp_timer
        esp_wifi
        esp_eth
        esp_event
        esp_netif
        esp_partition
        esp_adc
        nvs_flash
        network_provisioning
        bt
)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++20)

if(IDF_TARGET STREQUAL "esp32s3")
    include("${CMAKE_CURRENT_LIST_DIR}/PatchQemu.cmake")
endif()
