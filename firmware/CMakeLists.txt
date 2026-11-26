cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET esp32s3 CACHE STRING "ESP-IDF target")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(coffee_pot_alarm)

esptool_py_flash_to_partition(
    flash model_a "${CMAKE_CURRENT_SOURCE_DIR}/models/lenet_quantized.tflite"
)
