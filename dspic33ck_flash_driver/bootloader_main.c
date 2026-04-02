/**
 * bootloader_main.c
 *
 * Minimal bootloader entry point showing how to initialise the RAM function
 * copy and call the flash driver.
 *
 * Startup sequence
 * ----------------
 *   1. Hardware reset vector jumps to __reset (CRT0 provided by XC16).
 *   2. CRT0 copies .data from flash to RAM, zeroes .bss, then calls main().
 *   3. main() calls flash_driver_init_ramfunc() to copy .ramfunc to RAM.
 *   4. From this point all flash_* functions execute from RAM.
 *
 * NOTE: CRT0 runs BEFORE main().  If you need flash functions during static
 * initialisation (C++ constructors, etc.) you would need a custom CRT0 that
 * calls flash_driver_init_ramfunc() before running constructors.
 */

#include <xc.h>
#include <stdint.h>
#include "flash_driver.h"

/* -------------------------------------------------------------------------
 * Configuration bit settings for dsPIC33CK512MP606
 * Adjust to match your oscillator/WDT/ICD requirements.
 * ---------------------------------------------------------------------- */
// FICD
#pragma config ICS   = PGD2       /* PGC2/PGD2 for ICSP */
#pragma config JTAGEN = OFF

// FPOR
#pragma config BOREN  = ON
#pragma config ALTI2C1 = OFF

// FWDT
#pragma config WDTPOST = PS32768
#pragma config WDTPRE  = PR128
#pragma config WDTEN   = OFF       /* WDT disabled in software */

// FOSC
#pragma config PLLKEN  = ON
#pragma config OSCIOFNC = OFF
#pragma config POSCMD  = NONE     /* No primary oscillator */

// FOSCSEL
#pragma config FNOSC   = FRC       /* Start on internal FRC */
#pragma config IESO    = OFF

// FGS
#pragma config GWRP    = OFF
#pragma config GCP     = OFF

/* -------------------------------------------------------------------------
 * Example: erase one application page then write two instructions
 * ---------------------------------------------------------------------- */
static void example_flash_usage(void)
{
    FlashStatus st;

    /* 1. Erase a page at the beginning of the application region */
    st = flash_erase_page(APPLICATION_FLASH_START);
    if (st != FLASH_OK) {
        /* handle error */
        return;
    }

    /* 2. Write a double-word (2 instructions) at the same address.
     *    Here we write two NOP instructions (opcode 0x000000).
     *    Encode each instruction as uint32_t: bits[15:0]=low word,
     *    bits[23:16]=phantom byte, bits[31:24]=0 (unused).           */
    uint32_t nop_instr = 0x00000000ul;   /* NOP opcode                 */
    st = flash_write_double_word(APPLICATION_FLASH_START, nop_instr, nop_instr);
    if (st != FLASH_OK) {
        /* handle error */
        return;
    }

    /* 3. Read back and verify */
    uint32_t read_val = 0;
    st = flash_read_word(APPLICATION_FLASH_START, &read_val);
    if (st != FLASH_OK || read_val != nop_instr) {
        /* verify failed */
        return;
    }
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */
int main(void)
{
    /*
     * Step 1: copy .ramfunc section from flash image to RAM.
     * This must be the first thing called before any flash_* function.
     */
    flash_driver_init_ramfunc();

    /*
     * Step 2: your bootloader logic (receive firmware, validate, program).
     * Shown here with a small example.
     */
    example_flash_usage();

    /*
     * Step 3: jump to application.
     * Disable interrupts, load the application reset vector, and jump.
     */
    /* Example – adjust APPLICATION_FLASH_START to your app's reset vector */
    void (*app_reset)(void) = (void (*)(void))APPLICATION_FLASH_START;
    app_reset();

    /* Should never reach here */
    while (1) { }
    return 0;
}
