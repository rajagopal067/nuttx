/************************************************************************************
 * configs/hdk/muc/src/stm32_boot.c
 *
 *   Copyright (C) 2015 Motorola Mobility, LLC. All rights reserved.
 *   Copyright (C) 2011-2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ************************************************************************************/

#include <debug.h>
#include <errno.h>

#include <nuttx/config.h>
#include <nuttx/device.h>
#include <nuttx/device_battery.h>
#include <nuttx/device_hid.h>
#include <nuttx/device_ptp.h>
#include <nuttx/device_raw.h>
#include <nuttx/device_table.h>
#include <nuttx/util.h>

#include <arch/board/board.h>
#include <arch/board/mods.h>

#include "up_arch.h"
#include "hdk.h"

#include <nuttx/gpio/stm32_gpio_chip.h>

struct board_gpio_cfg_s
{
  uint8_t  pin;
  uint32_t cfgset;
};

static const struct board_gpio_cfg_s board_gpio_cfgs[] =
{
  { GPIO_MODS_SL_BPLUS_EN,   (GPIO_INPUT|GPIO_FLOAT) },
  { GPIO_MODS_CHG_PG_N,      (GPIO_INPUT|GPIO_FLOAT) },
};

#ifdef CONFIG_DEVICE_CORE
static struct device devices[] = {
#ifdef CONFIG_HID_BUTTONS
    {
        .type = DEVICE_TYPE_HID_HW,
        .name = "hid_buttons",
        .desc = "Generic HID buttons",
        .id   = 0,
    },
#endif
#ifdef CONFIG_MODS_RAW
    {
        .type = DEVICE_TYPE_RAW_HW,
        .name = "mods_raw",
        .desc = "Reference Raw Interface",
        .id   = 0,
    },
#endif
#ifdef CONFIG_PTP_DEVICE
    {
        .type = DEVICE_TYPE_PTP_HW,
        .name = "ptp_device",
        .desc = "Power transfer protocol",
        .id   = 0,
    },
#endif
#ifdef CONFIG_MAX17050_DEVICE
    {
        .type = DEVICE_TYPE_BATTERY_DEVICE,
        .name = "max17050_battery",
        .desc = "MAX17050 Battery Driver",
        .id   = 0,
    },
#endif
};

static struct device_table muc_device_table = {
    .device = devices,
    .device_count = ARRAY_SIZE(devices),
};
#endif

/* Map pin number to cfgset used by the STM32 GPIO framework */
int map_pin_nr_to_cfgset(uint8_t pin, uint32_t *cfgset)
{
  int i;

  if (pin > STM32_NGPIO)
    return -EINVAL;

  // 16 pins per port
  *cfgset = ((pin / 16) << GPIO_PORT_SHIFT) |
            ((pin % 16) << GPIO_PIN_SHIFT);

  for (i = 0; i < ARRAY_SIZE(board_gpio_cfgs); ++i)
    {
      if (board_gpio_cfgs[i].pin == pin)
        {
          // Additional pin configuration found
          *cfgset |= board_gpio_cfgs[i].cfgset;
        }
    }

  return 0;
}

/************************************************************************************
 * Name: stm32_boardinitialize
 *
 * Description:
 *   All STM32 architectures must provide the following entry point.  This entry point
 *   is called early in the intitialization -- after all memory has been configured
 *   and mapped but before any devices have been initialized.
 *
 ************************************************************************************/

void stm32_boardinitialize(void)
{
  uint32_t regval;

  /* VDDIO2 is valid on this board */

  regval  = getreg32(STM32_PWR_CR2);
  regval |= PWR_CR2_IOSV;
  putreg32(regval, STM32_PWR_CR2);

#ifdef CONFIG_STM32_SPI_DMA
  /* set up DMA Channels for SPI */
  regval  = DMA1_CSELR_CHAN2_SPI1_RX;
  regval |= DMA1_CSELR_CHAN3_SPI1_TX;
  regval |= DMA1_CSELR_CHAN4_SPI2_RX;
  regval |= DMA1_CSELR_CHAN5_SPI2_TX;
  putreg32(regval, STM32_DMA1_CSELR);
#endif

#ifdef CONFIG_MODS_DIET
  mods_init();
#endif

  stm32_configgpio(GPIO_APBE_SPIBOOT_N);
  stm32_configgpio(GPIO_APBE_BOOTRET);
  stm32_configgpio(GPIO_APBE_PWR_EN);
  stm32_configgpio(GPIO_APBE_RST_N);
}

/****************************************************************************
 * Name: board_initialize
 *
 * Description:
 *   If CONFIG_BOARD_INITIALIZE is selected, then an additional
 *   initialization call will be performed in the boot-up sequence to a
 *   function called board_initialize().  board_initialize() will be
 *   called immediately after up_initialize() is called and just before the
 *   initial application is started.  This additional initialization phase
 *   may be used, for example, to initialize board-specific device drivers.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARD_INITIALIZE
void board_initialize(void)
{
  /* Perform board-specific initialization */

  (void)stm32_bringup();

#if defined(CONFIG_GPIO_CHIP_STM32)
  stm32_gpio_init();
#endif

#ifdef CONFIG_DEVICE_CORE
  device_table_register(&muc_device_table);

#ifdef CONFIG_HID_BUTTONS
  extern struct device_driver hid_buttons_driver;
  device_register_driver(&hid_buttons_driver);
#endif
#ifdef CONFIG_MODS_RAW
  extern struct device_driver mods_raw_driver;
  device_register_driver(&mods_raw_driver);
#endif
#ifdef CONFIG_PTP_DEVICE
  extern struct device_driver ptp_driver;
  device_register_driver(&ptp_driver);
#endif
#ifdef CONFIG_MAX17050_DEVICE
  extern struct device_driver batt_driver;
  device_register_driver(&batt_driver);
#endif

#endif
}
#endif