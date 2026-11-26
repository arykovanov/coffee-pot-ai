# Skip unsupported ADC startup calibration only when QEMU is detected.
# Compile a build copy so real hardware retains normal calibration.
idf_component_get_property(adc_dir esp_adc COMPONENT_DIR)
idf_component_get_property(adc_lib esp_adc COMPONENT_LIB)
set(original "${adc_dir}/adc_common.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${original}")
file(READ "${original}" code)
set(constructor [=[static __attribute__((constructor)) void adc_hw_calibration(void)
{]=])
string(FIND "${code}" "${constructor}" at)
if(at LESS 0)
    message(FATAL_ERROR "ADC startup changed; review PatchQemu.cmake")
endif()
string(REPLACE "${constructor}" "${constructor}
    if (IsInQemu()) {
        return;
    }" code "${code}")
string(REPLACE [=[#include "sdkconfig.h"]=]
    [=[#include "sdkconfig.h"
#include "Qemu.h"]=] code "${code}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qemu")
set(patched "${CMAKE_CURRENT_BINARY_DIR}/qemu/adc_common.c")
file(WRITE "${patched}" "${code}")
get_target_property(sources ${adc_lib} SOURCES)
list(REMOVE_ITEM sources "adc_common.c" "${original}")
set_property(TARGET ${adc_lib} PROPERTY SOURCES "${sources}")
target_sources(${adc_lib} PRIVATE "${patched}")
target_include_directories(${adc_lib} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
