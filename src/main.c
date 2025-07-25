/* ----------------------------------------------------------------------------
 *         SAM Software Package License
 * ----------------------------------------------------------------------------
 * Copyright (c) 2011-2014, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following condition is met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

/**
 * --------------------
 * SAM-BA Implementation on SAMD21 and SAMD51
 * --------------------
 * Requirements to use SAM-BA :
 *
 * Supported communication interfaces (SAMD21):
 * --------------------
 *
 * SERCOM5 : RX:PB23 TX:PB22
 * Baudrate : 115200 8N1
 *
 * USB : D-:PA24 D+:PA25
 *
 * Pins Usage
 * --------------------
 * The following pins are used by the program :
 * PA25 : input/output
 * PA24 : input/output
 * PB23 : input
 * PB22 : output
 * PA15 : input
 *
 * The application board shall avoid driving the PA25,PA24,PB23,PB22 and PA15
 * signals
 * while the boot program is running (after a POR for example)
 *
 * Clock system
 * --------------------
 * CPU clock source (GCLK_GEN_0) - 48MHz DFLL oscillator (DFLL48M)
 * SERCOM5 core GCLK source (GCLK_ID_SERCOM5_CORE) - GCLK_GEN_0 (i.e., FDLL48M)
 * USB GCLK source (GCLK_ID_USB) - GCLK_GEN_0 (i.e., DFLL in CRM or open loop mode)
 *
 * Crystalless mode:
 * GCLK Generator 0 source (GCLK_GEN_0) - 48MHz DFLL in Clock Recovery mode
 * (DFLL48M)
 *
 * Crytal mode:
 * GCLK Generator 0 source (GCLK_GEN_0) - 48MHz DFLL in closed loop mode
 * GCLK Generator 1 source (GCLK_GEN_1) - 32 kHz external cyrstal (XOSC32K)
 * DFLL48M Refence clock (GCLK_DFLL48M_REF) - GCLK_GEN_1 (i.e., XOSC32K)
 *
 * Memory Mapping
 * --------------------
 * SAM-BA code will be located at 0x0 and executed before any applicative code.
 *
 * Applications compiled to be executed along with the bootloader will start at
 * 0x2000 (samd21) or 0x4000 (samd51)
 *
 */

#include "uf2.h"

static void check_start_application(void);

static volatile bool main_b_cdc_enable = false;
extern int8_t led_tick_step;

#if defined(SAMD21)
    #define RESET_CONTROLLER PM
#elif defined(SAMD51)
    #define RESET_CONTROLLER RSTC
#endif

/**
 * \brief Check the application startup condition
 *
 */
static void check_start_application(void) {
    uint32_t app_start_address;

    /* Load the Reset Handler address of the application */
    app_start_address = *(uint32_t *)(APP_START_ADDRESS + 4);

    /**
     * Test reset vector of application @APP_START_ADDRESS+4
     * Sanity check on the Reset_Handler address
     */
    if (app_start_address < APP_START_ADDRESS || app_start_address > FLASH_SIZE) {
        /* Stay in bootloader */
        return;
    }

#if USE_SINGLE_RESET
    if (SINGLE_RESET()) {
        if (RESET_CONTROLLER->RCAUSE.bit.POR || *DBL_TAP_PTR != DBL_TAP_MAGIC_QUICK_BOOT) {
            // the second tap on reset will go into app
            *DBL_TAP_PTR = DBL_TAP_MAGIC_QUICK_BOOT;
            // this will be cleared after successful USB enumeration
            // this is around 1.5s
            resetHorizon = timerHigh + 50;
            return;
        }
    }
#endif

    if (RESET_CONTROLLER->RCAUSE.bit.POR) {
        *DBL_TAP_PTR = 0;
    }
    else if (*DBL_TAP_PTR == DBL_TAP_MAGIC) {
        *DBL_TAP_PTR = 0;
        return; // stay in bootloader
    }
    else {
        if (*DBL_TAP_PTR != DBL_TAP_MAGIC_QUICK_BOOT) {
            *DBL_TAP_PTR = DBL_TAP_MAGIC;
            delay(500);
        }
        *DBL_TAP_PTR = 0;
    }

    LED_MSC_OFF();

#if defined(BOARD_RGBLED_CLOCK_PIN)
    // This won't work for neopixel, because we're running at 1MHz or thereabouts...
    RGBLED_set_color(COLOR_LEAVE);
#endif

    /* Rebase the Stack Pointer */
    __set_MSP(*(uint32_t *)APP_START_ADDRESS);

    /* Rebase the vector table base address */
    SCB->VTOR = ((uint32_t)APP_START_ADDRESS & SCB_VTOR_TBLOFF_Msk);

    /* Jump to application Reset Handler in the application */
    asm("bx %0" ::"r"(app_start_address));
}

extern char _etext;
extern char _end;

/**
 *  \brief  SAM-BA Main loop.
 *  \return Unused (ANSI-C compatibility).
 */
int main(void) {
    // if VTOR is set, we're not running in bootloader mode; halt
    if (SCB->VTOR)
        while (1) {
        }

#if defined(SAMD21)
    // Check for voltage too low, and set up brownout protection.
    // Code largely taken from https://blog.thea.codes/sam-d21-brown-out-detector/ (thanks!)

    // Disable the brown-out detector during configuration, otherwise
    // it might misbehave and reset the microcontroller.
    SYSCTRL->BOD33.bit.ENABLE = 0;
    while (!SYSCTRL->PCLKSR.bit.B33SRDY) {};

    // Configure the brown-out detector so that the program can use it to watch the power supply voltage.
    SYSCTRL->BOD33.reg = (
        // This sets the minimum voltage level to about 2.9V. See datasheet table 37-21.
        // Voltage threshold is about 1.5V + LEVEL * 34mV. See "Electrical Characteristics" in datasheet.
        // 39 is about 2.8V, and is a standard measured value in the datasheet.
        // External flash chips usually require at least 2.7V.
        SYSCTRL_BOD33_LEVEL(39) |
        // Since the program is waiting for the voltage to rise,
        // don't reset the microcontroller if the voltage is too low.
        SYSCTRL_BOD33_ACTION_NONE |
        // Enable hysteresis to better deal with noisy power supplies and voltage transients.
        SYSCTRL_BOD33_HYST);

    // Enable the brown-out detector and then wait for the voltage level to settle.
    SYSCTRL->BOD33.bit.ENABLE = 1;
    while (!SYSCTRL->PCLKSR.bit.BOD33RDY) {}

    // BOD33DET is set when the voltage is *too low*, so wait for it to be cleared.
    while (SYSCTRL->PCLKSR.bit.BOD33DET) {}

    // Let the brown-out detector automatically reset the microcontroller if the voltage drops too low.
    SYSCTRL->BOD33.bit.ENABLE = 0;
    while (!SYSCTRL->PCLKSR.bit.B33SRDY) {};

    SYSCTRL->BOD33.reg |= SYSCTRL_BOD33_ACTION_RESET;
    SYSCTRL->BOD33.bit.ENABLE = 1;

    // If fuses have been reset to all ones, the watchdog ALWAYS-ON is
    // set, so we can't turn off the watchdog.  Set the fuse to a
    // reasonable value and reset. This is a mini version of the fuse
    // reset code in selfmain.c.
    if (((uint32_t *)NVMCTRL_AUX0_ADDRESS)[0] == 0xffffffff) {
        // Clear any error flags.
        NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;
        // Turn off cache and put in manual mode.
        NVMCTRL->CTRLB.reg = NVMCTRL->CTRLB.reg | NVMCTRL_CTRLB_CACHEDIS | NVMCTRL_CTRLB_MANW;
        // Set address to write.
        NVMCTRL->ADDR.reg = NVMCTRL_AUX0_ADDRESS / 2;
        // Erase auxiliary row.
        NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_EAR;
	while (!(NVMCTRL->INTFLAG.bit.READY)) {}
        // Clear page buffer.
        NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_PBC;
	while (!(NVMCTRL->INTFLAG.bit.READY)) {}
        // Reasonable fuse values, including 8k BOOTPROT.
        ((uint32_t *)NVMCTRL_AUX0_ADDRESS)[0] = 0xD8E0C7FA;
        ((uint32_t *)NVMCTRL_AUX0_ADDRESS)[1] = 0xFFFFFC5D;
        // Write the fuses
	NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_WAP;
	while (!(NVMCTRL->INTFLAG.bit.READY)) {}
        resetIntoBootloader();
    }

    // Disable the watchdog, in case the application set it.
    WDT->CTRL.reg = 0;
    while(WDT->STATUS.bit.SYNCBUSY) {}

#elif defined(SAMD51)
    // Disable the watchdog, in case the application set it.
    WDT->CTRLA.reg = 0;
    while(WDT->SYNCBUSY.reg) {}

    // Enable 2.7V brownout detection. The default fuse value is 1.7
    // Set brownout detection to ~2.7V. Default from factory is 1.7V,
    // which is too low for proper operation of external SPI flash chips (they are 2.7-3.6V).
    // Also without this higher level, the SAMD51 will write zeros to flash intermittently.
    // Disable while changing level.

    SUPC->BOD33.bit.ENABLE = 0;
    while (!SUPC->STATUS.bit.B33SRDY) {}  // Wait for BOD33 to synchronize.
    SUPC->BOD33.bit.LEVEL = 200;  // 2.7V: 1.5V + LEVEL * 6mV.
    // Don't reset right now.
    SUPC->BOD33.bit.ACTION = SUPC_BOD33_ACTION_NONE_Val;
    SUPC->BOD33.bit.ENABLE = 1; // enable brown-out detection

    // Wait for BOD33 peripheral to be ready.
    while (!SUPC->STATUS.bit.BOD33RDY) {}

    // Wait for voltage to rise above BOD33 value.
    while (SUPC->STATUS.bit.BOD33DET) {}

    // If we are starting from a power-on or a brownout,
    // wait for the voltage to stabilize. Don't do this on an
    // external reset because it interferes with the timing of double-click.
    // "BODVDD" means BOD33.
    if (RSTC->RCAUSE.bit.POR || RSTC->RCAUSE.bit.BODVDD) {
        do {
            // Check again in 100ms.
            delay(100);
        } while (SUPC->STATUS.bit.BOD33DET);
    }

    // Now enable reset if voltage falls below minimum.
    SUPC->BOD33.bit.ENABLE = 0;
    while (!SUPC->STATUS.bit.B33SRDY) {}  // Wait for BOD33 to synchronize.
    SUPC->BOD33.bit.ACTION = SUPC_BOD33_ACTION_RESET_Val;
    SUPC->BOD33.bit.ENABLE = 1;
#endif

#if USB_VID == 0x239a && USB_PID == 0x0013     // Adafruit Metro M0
    // Delay a bit so SWD programmer can have time to attach.
    delay(15);
#endif
    led_init();

    logmsg("Start");
    assert((uint32_t)&_etext < APP_START_ADDRESS);
    // bossac writes at 0x20005000
    assert(!USE_MONITOR || (uint32_t)&_end < 0x20005000);

    assert(8 << NVMCTRL->PARAM.bit.PSZ == FLASH_PAGE_SIZE);
    assert(FLASH_PAGE_SIZE * NVMCTRL->PARAM.bit.NVMP == FLASH_SIZE);

    /* Jump in application if condition is satisfied */
    check_start_application();

    /* We have determined we should stay in the monitor. */
    /* System initialization */
    system_init();

    __DMB();
    __enable_irq();

#if USE_UART
    /* UART is enabled in all cases */
    usart_open();
#endif

    logmsg("Before main loop");

    usb_init();

    // not enumerated yet
    RGBLED_set_color(COLOR_START);
    led_tick_step = 10;

    /* Wait for a complete enum on usb or a '#' char on serial line */
while (1) {
    // Check if USB is ready
    if (USB_Ok()) {
        if (!main_b_cdc_enable) {
            RGBLED_set_color(COLOR_USB);
            led_tick_step = 1;
            main_b_cdc_enable = true;
            
            #ifdef BOARD_SCREEN
            // Initialize screen after USB enumeration
            screen_init();
            screen_show_usb_status(true);
            delay(500);
            draw_drag();
            #endif
        }
    }

#if USE_MONITOR
    // Check if a USB enumeration has succeeded
    if (main_b_cdc_enable) {
        logmsg("entering monitor loop");
        // SAM-BA on USB loop
        while (1) {
            sam_ba_monitor_run();
        }
    }
    
    #if USE_UART
    /* Check if a '#' has been received */
    if (!main_b_cdc_enable && usart_sharp_received()) {
        RGBLED_set_color(COLOR_UART);
        sam_ba_monitor_init(SAM_BA_INTERFACE_USART);
        /* SAM-BA on UART loop */
        while (1) {
            sam_ba_monitor_run();
        }
    }
    #endif
#else
    // When USE_MONITOR is not defined, we need to process MSC
    if (main_b_cdc_enable) {
        process_msc();
    }
#endif

    // Small delay for timing when USB not enumerated
    if (!main_b_cdc_enable) {
        for (int i = 1; i < 256; ++i) {
            asm("nop");
        }
    }
} // End of main while loop
}

