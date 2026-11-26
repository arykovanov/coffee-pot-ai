/* QEMU bootloader startup.

   Copyright (c) 2026 Real Time Logic

   This software may only be used by the terms and conditions
   stipulated in the corresponding license agreement under which the
   software has been supplied. All use of this code is subject to the
   terms and conditions of the included License Agreement.
*/

#include "Qemu.h"
#include <soc/spi_mem_reg.h>

/* ESP-IDF's bootloader links this symbol to retain the weak hook below. */
void bootloader_hooks_include(void)
{
}

void bootloader_after_init(void)
{
   if(IsInQemu())
   {
      /* Match the hardware's disabled CS1 reset state. Otherwise bootloader
         flash writes also reach QEMU's uninitialized Octal PSRAM model. */
      REG_SET_BIT(SPI_MEM_MISC_REG(1), SPI_MEM_CS1_DIS);
   }
}
