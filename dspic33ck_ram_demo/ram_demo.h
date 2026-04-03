/**
 * @file ram_demo.h
 * @brief RAM Function Execution Demo - dsPIC33CK512MP606
 *
 * Demonstrates how to declare, copy, and verify that a function
 * is executing from Data RAM instead of Program Flash.
 *
 * dsPIC33CK512MP606 Memory Map (verify against DS70005349):
 *   Program Flash : 0x000200 - 0x055FFF  (~512 KB, 24-bit instructions)
 *   Data RAM      : 0x000800 - 0x00D7FF  ( 52 KB, byte-addressable)
 *   SFRs          : 0x000000 - 0x0007FF
 *
 * Compiler: MPLAB XC16 (v2.x recommended)
 * Tool:     MPLAB X IDE
 *
 * Official References:
 *   [1] DS70005349  - dsPIC33CK512MP606 Datasheet (memory map, section 4)
 *   [2] DS50002071  - MPLAB XC16 C Compiler User's Guide
 *                    (Chapter 14: Placing Variables and Functions)
 *   [3] DS70000657  - dsPIC33C Family Reference Manual - Memory Organisation
 *   [4] AN1xxxxA    - "Code Execution from RAM" App Note (search Microchip AN library)
 *   [5] DS01227A    - "Placing Functions in RAM using MPLAB XC16" (MLA community note)
 */

#ifndef RAM_DEMO_H
#define RAM_DEMO_H

#include <xc.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Memory-range constants – adjust if your linker script differs
 * ----------------------------------------------------------------------- */
#define DATA_RAM_START  0x000800UL   /* First byte of Data SRAM             */
#define DATA_RAM_END    0x00D7FFUL   /* Last  byte of Data SRAM (52 KB)     */

/* -----------------------------------------------------------------------
 * RAM-function attribute macro
 *
 * XC16 places the function body into the custom ELF section ".ramfunc".
 * The linker script (p33CK512MP606_ram.gld) maps that section so that:
 *   - LOAD address  = Program Flash  (hex file stores it here)
 *   - RUN  address  = Data RAM       (execution happens here)
 *
 * "long_call" forces a full indirect call through a 23-bit pointer so the
 * call works even when the RAM and the caller are far apart in address space.
 * ----------------------------------------------------------------------- */
#define RAM_FUNC __attribute__((section(".ramfunc"), long_call))

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

/**
 * @brief Copy all .ramfunc code from Flash to RAM.
 *
 * Must be called ONCE from main() before any RAM_FUNC function is invoked.
 * Uses the symbols _ramfunc_load_begin / _ramfunc_begin / _ramfunc_end
 * exported by the linker script.
 */
void init_ram_functions(void);

/**
 * @brief Return 1 if the given code address falls inside Data RAM.
 *
 * Use this to assert that a RAM_FUNC function pointer is really in RAM:
 *   assert(is_in_ram((void *)my_ram_function));
 */
int is_in_ram(void *addr);

/**
 * @brief Demo function that RUNS FROM RAM.
 *
 * Blinks an LED N times by toggling a port pin.
 * The body of this function is physically executing from Data RAM.
 *
 * @param lat_reg  Pointer to the LAT register of the LED port (e.g. &LATB)
 * @param pin_mask Bitmask of the LED pin (e.g. (1u << 8) for RB8)
 * @param count    Number of blink cycles
 */
void RAM_FUNC led_blink_from_ram(volatile uint16_t *lat_reg,
                                  uint16_t pin_mask,
                                  uint16_t count);

/**
 * @brief Same blink logic but compiled to run from FLASH (for comparison).
 */
void led_blink_from_flash(volatile uint16_t *lat_reg,
                           uint16_t pin_mask,
                           uint16_t count);

/* -----------------------------------------------------------------------
 * CAN-bus dynamic loading helpers
 * (used when you receive a function payload over CAN at runtime)
 * ----------------------------------------------------------------------- */

/** Maximum size (bytes) reserved in RAM for a CAN-downloaded function. */
#define CAN_FUNC_RAM_POOL_SIZE  512u

/**
 * @brief Write received CAN bytes into the function RAM pool.
 *
 * Call this from your CAN RX ISR / task for each received frame until
 * the full function image has been received.
 *
 * @param src    Pointer to received byte payload
 * @param offset Byte offset within the function image
 * @param len    Number of bytes in this chunk
 * @return 0 on success, -1 if offset+len exceeds CAN_FUNC_RAM_POOL_SIZE
 */
int can_load_function_chunk(const uint8_t *src, uint16_t offset, uint16_t len);

/**
 * @brief Validate and execute the function that was loaded via CAN.
 *
 * Performs a simple CRC-16 check, then casts the RAM pool base address to a
 * function pointer and calls it.
 *
 * @param expected_crc  CRC-16/CCITT value expected for the received image
 * @return Return value from the loaded function, or -1 on CRC failure
 */
int can_execute_loaded_function(uint16_t expected_crc);

#endif /* RAM_DEMO_H */
