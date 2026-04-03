/**
 * @file main.c
 * @brief RAM Function Demo - dsPIC33CK512MP606
 *
 * Build with MPLAB X + XC16. Use the custom linker script
 * p33CK512MP606_ram.gld supplied in this project.
 *
 * Expected UART output (115200-8N1):
 *   [BOOT] RAM functions init done
 *   [CHECK] led_blink_from_ram  @ 0x000A00 -> IN  RAM  <-- address will vary
 *   [CHECK] led_blink_from_flash@ 0x001234 -> IN  FLASH
 *   [DEMO] Blinking LED from RAM  (3x)
 *   [DEMO] Blinking LED from FLASH(3x)
 *   [CAN]  Waiting for function image...
 */

/* ---- Compiler / device includes ---------------------------------------- */
#include <xc.h>
#include <stdint.h>
#include <stdio.h>      /* printf -> redirected to UART via _mon_putc */
#include "ram_demo.h"

/* ========================================================================
 * Configuration Bits  (adjust to your oscillator / board)
 * ======================================================================== */
// FSEC
#pragma config BWRP  = OFF        /* Boot segment write protect           */
#pragma config BSS   = DISABLED   /* Boot segment code protect            */
#pragma config BSEN  = OFF
#pragma config GWRP  = OFF
#pragma config GSS   = DISABLED
#pragma config CWRP  = OFF
#pragma config CSS   = DISABLED
#pragma config AIVTDIS = OFF

// FOSCSEL
#pragma config FNOSC = FRC        /* Start on internal FRC (8 MHz)        */
#pragma config IESO  = OFF

// FOSC
#pragma config POSCMD = NONE      /* No primary oscillator                */
#pragma config OSCIOFNC = ON
#pragma config FCKSM = CSECMD

// FWDT
#pragma config WDTPOST = PS32768
#pragma config WDTPRE  = PR128
#pragma config WDTEN   = OFF      /* WDT disabled for demo                */
#pragma config WINDIS  = OFF
#pragma config WDTWIN  = WIN25

// FICD
#pragma config ICS = PGD2
#pragma config JTAGEN = OFF

// FDEVOPT
#pragma config ALTI2C1 = OFF
#pragma config ALTI2C2 = OFF
#pragma config ALTI2C3 = OFF
#pragma config SMBEN   = SMBUS
#pragma config SPI2PIN  = PPS

/* ========================================================================
 * Hardware definitions  (LED on RB8 – change to match your board)
 * ======================================================================== */
#define LED_LAT_REG   LATB
#define LED_TRIS_REG  TRISB
#define LED_PIN_MASK  (1u << 8u)   /* RB8 */

/* ========================================================================
 * Clock setup: FRC + PLL -> Fosc = 200 MHz, Fcy = 100 MHz
 * ======================================================================== */
static void clock_init(void)
{
    /* Fvco = Fin * (FBDIV / PRDIV)
     * Fosc = Fvco / POSTDIV1 / POSTDIV2
     * FRC = 8 MHz, PRDIV=1, FBDIV=100, POST1=2, POST2=2 -> Fosc=200 MHz */
    PLLFBD  = 98u;        /* FBDIV = PLLFBD + 2 = 100               */
    CLKDIVbits.PLLPRE  = 1u;   /* PRDIV = 1                         */
    PLLDIVbits.POST1DIV = 2u;  /* POSTDIV1 = 2                      */
    PLLDIVbits.POST2DIV = 2u;  /* POSTDIV2 = 2  -> Fcy = 100 MHz    */

    /* Switch to FRC+PLL */
    __builtin_write_OSCCONH(0x01);          /* new osc = FRCPLL     */
    __builtin_write_OSCCONL(OSCCON | 0x01); /* initiate switch      */
    while (OSCCONbits.OSWEN == 1u);         /* wait for switch      */
    while (OSCCONbits.LOCK  == 0u);         /* wait for PLL lock    */
}

/* ========================================================================
 * UART1 setup for printf output (TX = RD11 via PPS – adjust to your board)
 * ======================================================================== */
static void uart_init(void)
{
    /* PPS: map U1TX to RP75 (RD11) */
    __builtin_write_RPCON(0x0000);   /* unlock PPS */
    RPOR9bits.RP75R = 0x01;          /* U1TX = RP75 */
    __builtin_write_RPCON(0x0800);   /* lock PPS   */

    TRISDbits.TRISD11 = 0u;          /* TX pin output */

    U1MODEbits.UARTEN = 0u;
    U1BRG  = (uint16_t)((100000000UL / (16UL * 115200UL)) - 1UL); /* BRG for 100 MHz Fcy */
    U1MODEbits.BRGH   = 0u;
    U1MODEbits.PDSEL  = 0u;  /* 8-bit, no parity */
    U1MODEbits.STSEL  = 0u;  /* 1 stop bit       */
    U1STAbits.UTXEN   = 1u;
    U1MODEbits.UARTEN = 1u;
}

/* Redirect printf to UART1 */
void _mon_putc(char c)
{
    while (U1STAbits.UTXBF == 1u);  /* wait if TX buffer full */
    U1TXREG = (uint16_t)(uint8_t)c;
}

/* ========================================================================
 * main
 * ======================================================================== */
int main(void)
{
    /* 1. Hardware init */
    clock_init();
    uart_init();

    /* LED pin as output, start OFF */
    LED_TRIS_REG &= (uint16_t)(~LED_PIN_MASK);
    LED_LAT_REG  &= (uint16_t)(~LED_PIN_MASK);

    /* ------------------------------------------------------------------
     * 2. Copy .ramfunc section from Flash -> RAM
     *
     *    This MUST happen before calling any RAM_FUNC function.
     *    The CRT (_dinit) only handles initialized data (.data).
     *    Our .ramfunc section has its own copy routine here.
     * ------------------------------------------------------------------ */
    init_ram_functions();
    printf("[BOOT] RAM functions init done\r\n");

    /* ------------------------------------------------------------------
     * 3. Verify execution addresses at runtime
     *
     *    Taking the address of a function returns its RUN address.
     *    After init_ram_functions() the RAM_FUNC functions point to RAM.
     * ------------------------------------------------------------------ */
    void *ram_fn_addr   = (void *)led_blink_from_ram;
    void *flash_fn_addr = (void *)led_blink_from_flash;

    printf("[CHECK] led_blink_from_ram   @ 0x%06lX -> %s\r\n",
           (unsigned long)ram_fn_addr,
           is_in_ram(ram_fn_addr) ? "IN  RAM  <-- executing from SRAM" : "IN  FLASH (ERROR: copy failed?)");

    printf("[CHECK] led_blink_from_flash @ 0x%06lX -> %s\r\n",
           (unsigned long)flash_fn_addr,
           is_in_ram(flash_fn_addr) ? "IN  RAM  (unexpected)" : "IN  FLASH <-- executing from Flash");

    /* ------------------------------------------------------------------
     * 4. Run both versions so you can observe them with a debugger
     *
     *    In MPLAB X debugger: set a breakpoint inside led_blink_from_ram.
     *    The "PC" register shown in the "CPU Registers" window will be
     *    in the range 0x000800..0x00D7FF (RAM), NOT 0x000200..0x055FFF.
     * ------------------------------------------------------------------ */
    printf("[DEMO] Blinking LED from RAM   (3x)\r\n");
    led_blink_from_ram(&LED_LAT_REG, LED_PIN_MASK, 3u);

    printf("[DEMO] Blinking LED from FLASH (3x)\r\n");
    led_blink_from_flash(&LED_LAT_REG, LED_PIN_MASK, 3u);

    /* ------------------------------------------------------------------
     * 5. CAN dynamic-loading stub
     *
     *    In your real application:
     *      a) Receive function image frames over CAN (8 bytes each).
     *      b) Call can_load_function_chunk() for every frame.
     *      c) When all frames received, call can_execute_loaded_function().
     *
     *    The function image in the hex file is just raw instruction bytes.
     *    Compile the "payload" function separately with:
     *      -mno-eds-warn -fno-short-double
     *    and link it at the BASE address of _can_func_pool so all
     *    internal branches are correct (position-dependent code), OR
     *    compile with -mreorder-functions and relocatable output then
     *    do runtime relocation in can_execute_loaded_function().
     * ------------------------------------------------------------------ */
    printf("[CAN]  Ready to receive function image via CAN.\r\n");
    printf("[CAN]  Call can_load_function_chunk() + can_execute_loaded_function().\r\n");

    while (1)
    {
        /* Main application loop */
        __asm__ volatile ("nop");
    }

    return 0;
}
