/* QEMU runtime detection.

   Copyright (c) 2026 Real Time Logic

   This software may only be used by the terms and conditions
   stipulated in the corresponding license agreement under which the
   software has been supplied. All use of this code is subject to the
   terms and conditions of the included License Agreement.
*/
#pragma once

#include "sdkconfig.h"
#include <stdbool.h>
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
#include <soc/soc.h>
#include <soc/syscon_reg.h>
#endif

/* Espressif QEMU exposes its signature in the reserved word before DATE.
   This MMIO read is also safe before app_main, during ADC constructors. */
static inline bool IsInQemu(void)
{
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
   return REG_READ(SYSCON_DATE_REG - 4) == 0x51454d55U;
#else
   /* No supported QEMU machine for Xedge32's other target, ESP32-P4. */
   return false;
#endif
}
