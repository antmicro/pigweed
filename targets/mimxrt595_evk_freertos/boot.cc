// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_boot/boot.h"

#include "board.h"
#include "clock_config.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "pw_boot_cortex_m/boot.h"
#include "pw_preprocessor/compiler.h"
#include "pw_sys_io_mcuxpresso/init.h"
#include "FreeRTOS.h"
#include "pw_system/init.h"
#include "task.h"
#include "fsl_clock.h"
#include "fsl_debug_console.h"
#include "controller_hci_uart.h"
#include "fsl_power.h"
#include "usb_host_config.h"
#include "usb_phy.h"
#include "usb_host.h"

#if PW_MALLOC_ACTIVE
#include "pw_malloc/malloc.h"
#endif  // PW_MALLOC_ACTIVE

#define CONTROLLER_ID            kUSB_ControllerIp3516Hs0

void pw_boot_PreStaticMemoryInit() {
  // Call CMSIS SystemInit code.
  SystemInit();
}

void pw_boot_PreStaticConstructorInit() {
#if PW_MALLOC_ACTIVE
  pw_MallocInit(&pw_boot_heap_low_addr, &pw_boot_heap_high_addr);
#endif  // PW_MALLOC_ACTIVE
}

#if (((defined(CONFIG_BT_SMP)) && (CONFIG_BT_SMP)))
#include "ksdk_mbedtls.h"
#endif /* CONFIG_BT_SMP */
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#if defined(__GIC_PRIO_BITS)
#define USB_HOST_INTERRUPT_PRIORITY (25U)
#elif defined(__NVIC_PRIO_BITS) && (__NVIC_PRIO_BITS >= 3)
#define USB_HOST_INTERRUPT_PRIORITY (6U)
#else
#define USB_HOST_INTERRUPT_PRIORITY (3U)
#endif
/*work as debug console with M.2, work as bt usart with non-M.2*/
#define APP_DEBUG_UART_TYPE     kSerialPort_Uart
#define APP_DEBUG_UART_BASEADDR (uint32_t) USART12
#define APP_DEBUG_UART_INSTANCE 12U
#define APP_DEBUG_UART_CLK_FREQ CLOCK_GetFlexcommClkFreq(12)
#define APP_DEBUG_UART_FRG_CLK \
    (const clock_frg_clk_config_t){12U, clock_frg_clk_config_t::kCLOCK_FrgPllDiv, 255U, 0U} /*!< Select FRG0 mux as frg_pll */
#define APP_DEBUG_UART_CLK_ATTACH kFRG_to_FLEXCOMM12
#define APP_UART_IRQ_HANDLER      FLEXCOMM12_IRQHandler
#define APP_UART_IRQ              FLEXCOMM12_IRQn
#define APP_DEBUG_UART_BAUDRATE   115200

int controller_hci_uart_get_configuration(controller_hci_uart_config_t *config)
{
    if (NULL == config)
    {
        return -1;
    }
    config->clockSrc        = BOARD_BT_UART_CLK_FREQ;
    config->defaultBaudrate = 115200u;
    config->runningBaudrate = BOARD_BT_UART_BAUDRATE;
    config->instance        = BOARD_BT_UART_INSTANCE;
    config->enableRxRTS     = 1u;
    config->enableTxCTS     = 1u;
    return 0;
}

void USB_HostClockInit(void)
{
    uint8_t usbClockDiv = 1;
    uint32_t usbClockFreq;
    usb_phy_config_struct_t phyConfig = {
        BOARD_USB_PHY_D_CAL,
        BOARD_USB_PHY_TXCAL45DP,
        BOARD_USB_PHY_TXCAL45DM,
    };

    /* Make sure USDHC ram buffer and usb1 phy has power up */
    POWER_DisablePD(kPDRUNCFG_APD_USBHS_SRAM);
    POWER_DisablePD(kPDRUNCFG_PPD_USBHS_SRAM);
    POWER_ApplyPD();

    RESET_PeripheralReset(kUSBHS_PHY_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSBHS_DEVICE_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSBHS_HOST_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kUSBHS_SRAM_RST_SHIFT_RSTn);

    /* enable usb ip clock */
    CLOCK_EnableUsbHs0HostClock(kOSC_CLK_to_USB_CLK, usbClockDiv);
    /* save usb ip clock freq*/
    usbClockFreq = g_xtalFreq / usbClockDiv;
    CLOCK_SetClkDiv(kCLOCK_DivPfc1Clk, 4);
    /* enable usb ram clock */
    CLOCK_EnableClock(kCLOCK_UsbhsSram);
    /* enable USB PHY PLL clock, the phy bus clock (480MHz) source is same with USB IP */
    CLOCK_EnableUsbHs0PhyPllClock(kOSC_CLK_to_USB_CLK, usbClockFreq);

    /* USB PHY initialization */
    USB_EhciPhyInit(CONTROLLER_ID, BOARD_XTAL_SYS_CLK_HZ, &phyConfig);

#if ((defined FSL_FEATURE_USBHSH_USB_RAM) && (FSL_FEATURE_USBHSH_USB_RAM > 0U))
    for (int i = 0; i < (FSL_FEATURE_USBHSH_USB_RAM >> 2); i++)
    {
        ((uint32_t *)FSL_FEATURE_USBHSH_USB_RAM_BASE_ADDRESS)[i] = 0U;
    }
#endif

    /* enable usb1 device clock */
    CLOCK_EnableClock(kCLOCK_UsbhsDevice);
    USBHSH->PORTMODE &= ~USBHSH_PORTMODE_DEV_ENABLE_MASK;
    /*  Wait until dev_needclk de-asserts */
    while (SYSCTL0->USB0CLKSTAT & SYSCTL0_USB0CLKSTAT_DEV_NEED_CLKST_MASK)
    {
        __ASM("nop");
    }
    /* disable usb1 device clock */
    CLOCK_DisableClock(kCLOCK_UsbhsDevice);
}
void USB_HostIsrEnable(void)
{
    uint8_t irqNumber;

    uint8_t usbHOSTEhciIrq[] = USBHSH_IRQS;
    irqNumber                = usbHOSTEhciIrq[CONTROLLER_ID - kUSB_ControllerIp3516Hs0];
    /* USB_HOST_CONFIG_EHCI */

    /* Install isr, set priority, and enable IRQ. */
    NVIC_SetPriority((IRQn_Type)irqNumber, USB_HOST_INTERRUPT_PRIORITY);

    EnableIRQ((IRQn_Type)irqNumber);
}

void APP_InitAppDebugConsole(void)
{
    uint32_t uartClkSrcFreq;

    /* attach FRG0 clock to FLEXCOMM12 (debug console) */
    auto debug_uart_cfg = APP_DEBUG_UART_FRG_CLK;
    CLOCK_SetFRGClock(&debug_uart_cfg);
    CLOCK_AttachClk(APP_DEBUG_UART_CLK_ATTACH);

    uartClkSrcFreq = APP_DEBUG_UART_CLK_FREQ;

    DbgConsole_Init(APP_DEBUG_UART_INSTANCE, APP_DEBUG_UART_BAUDRATE, APP_DEBUG_UART_TYPE, uartClkSrcFreq);
}

void pw_boot_PreMainInit() {
  pw::system::Init();

  RESET_ClearPeripheralReset(kHSGPIO0_RST_SHIFT_RSTn);
  RESET_ClearPeripheralReset(kHSGPIO3_RST_SHIFT_RSTn);
  RESET_ClearPeripheralReset(kHSGPIO4_RST_SHIFT_RSTn);

  BOARD_InitBootPins();
  BOARD_InitBootClocks();

  APP_InitAppDebugConsole();
  /* attach FRG0 clock to FLEXCOMM0 */
  auto bt_uart_cfg = BOARD_BT_UART_FRG_CLK;
  CLOCK_SetFRGClock(&bt_uart_cfg);
  CLOCK_AttachClk(BOARD_BT_UART_CLK_ATTACH);

  BOARD_SetFlexspiClock(FLEXSPI0, 2U, 4U);

  BOARD_InitPeripherals();
  pw_sys_io_mcuxpresso_Init();

  vTaskStartScheduler();
}

// This `main()` stub prevents another main function from being linked since
// this target deliberately doesn't run `main()`.
extern "C" int main() { return 0; }

PW_NO_RETURN void pw_boot_PostMain() {
  // In case main() returns, just sit here until the device is reset.
  while (true) {
  }
  PW_UNREACHABLE;
}
