/**
 * @file ram_demo.c
 * @brief RAM Function Execution Demo - dsPIC33CK512MP606
 *
 * See ram_demo.h for full documentation and references.
 */

#include "ram_demo.h"
#include <string.h>   /* memcpy */

/* -----------------------------------------------------------------------
 * Linker-script symbols
 *
 * These three symbols are exported by the custom .gld section:
 *
 *   _ramfunc_load_begin  – Flash address where .ramfunc bytes are stored
 *   _ramfunc_begin       – RAM  address where .ramfunc bytes will be copied
 *   _ramfunc_end         – RAM  address of the last+1 byte of .ramfunc
 *
 * XC16 linker declares symbols as _name, C code accesses them as below.
 * ----------------------------------------------------------------------- */
extern uint8_t _ramfunc_load_begin;   /* LMA: where bytes live in Flash    */
extern uint8_t _ramfunc_begin;        /* VMA: where they run in RAM        */
extern uint8_t _ramfunc_end;          /* VMA: end of .ramfunc in RAM       */

/* -----------------------------------------------------------------------
 * Pool for CAN-downloaded function image
 * Aligned to 4 bytes so a function pointer cast is safe.
 * ----------------------------------------------------------------------- */
static uint8_t _can_func_pool[CAN_FUNC_RAM_POOL_SIZE] __attribute__((aligned(4)));

/* -----------------------------------------------------------------------
 * init_ram_functions
 * ----------------------------------------------------------------------- */
void init_ram_functions(void)
{
    uint16_t size = (uint16_t)(&_ramfunc_end - &_ramfunc_begin);

    /*
     * Copy the .ramfunc section from its LOAD address (Flash) to its
     * RUN address (RAM).  After this call every RAM_FUNC function
     * pointer is valid.
     */
    memcpy(&_ramfunc_begin, &_ramfunc_load_begin, size);
}

/* -----------------------------------------------------------------------
 * is_in_ram
 * ----------------------------------------------------------------------- */
int is_in_ram(void *addr)
{
    uint32_t a = (uint32_t)(uintptr_t)addr;
    return (a >= DATA_RAM_START && a <= DATA_RAM_END);
}

/* -----------------------------------------------------------------------
 * led_blink_from_ram   (body lives in .ramfunc section -> copied to RAM)
 *
 * NOTE: Do NOT call any standard-library functions from here unless those
 * are also in RAM – function calls resolve through absolute addresses and
 * a library function still in Flash will be jumped to correctly, but if
 * Flash is being erased/reprogrammed concurrently this would be unsafe.
 * ----------------------------------------------------------------------- */
void RAM_FUNC led_blink_from_ram(volatile uint16_t *lat_reg,
                                  uint16_t pin_mask,
                                  uint16_t count)
{
    uint16_t i, d;

    for (i = 0; i < count; i++)
    {
        /* LED ON */
        *lat_reg |= pin_mask;

        /* Simple software delay (~200 ms at 100 MHz Fcy) */
        for (d = 0; d < 20000u; d++) { __asm__ volatile ("nop"); }

        /* LED OFF */
        *lat_reg &= (uint16_t)(~pin_mask);

        /* Delay again */
        for (d = 0; d < 20000u; d++) { __asm__ volatile ("nop"); }
    }
}

/* -----------------------------------------------------------------------
 * led_blink_from_flash  (plain function, no special attribute -> stays in Flash)
 * ----------------------------------------------------------------------- */
void led_blink_from_flash(volatile uint16_t *lat_reg,
                           uint16_t pin_mask,
                           uint16_t count)
{
    uint16_t i, d;

    for (i = 0; i < count; i++)
    {
        *lat_reg |= pin_mask;
        for (d = 0; d < 20000u; d++) { __asm__ volatile ("nop"); }
        *lat_reg &= (uint16_t)(~pin_mask);
        for (d = 0; d < 20000u; d++) { __asm__ volatile ("nop"); }
    }
}

/* -----------------------------------------------------------------------
 * CAN dynamic loading helpers
 * ----------------------------------------------------------------------- */

int can_load_function_chunk(const uint8_t *src, uint16_t offset, uint16_t len)
{
    if ((uint32_t)offset + len > CAN_FUNC_RAM_POOL_SIZE)
        return -1;

    memcpy(&_can_func_pool[offset], src, len);
    return 0;
}

/* Minimal CRC-16/CCITT (poly 0x1021, init 0xFFFF) */
static uint16_t crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i, j;

    for (i = 0; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8u);
        for (j = 0; j < 8u; j++)
        {
            if (crc & 0x8000u)
                crc = (uint16_t)((crc << 1u) ^ 0x1021u);
            else
                crc <<= 1u;
        }
    }
    return crc;
}

int can_execute_loaded_function(uint16_t expected_crc)
{
    /*
     * The sender must tell us the exact byte-length of the image.
     * Here we use the full pool as a simplification; in production
     * you would track the actual received size.
     */
    uint16_t img_len = CAN_FUNC_RAM_POOL_SIZE; /* replace with actual length */

    /* 1. Integrity check */
    if (crc16(_can_func_pool, img_len) != expected_crc)
        return -1;   /* CRC mismatch – do NOT execute */

    /* 2. Cast pool base to a no-arg, int-return function pointer */
    typedef int (*ram_fn_t)(void);
    ram_fn_t fn = (ram_fn_t)(void *)_can_func_pool;

    /* 3. Verify the target address is really inside RAM before jumping */
    if (!is_in_ram((void *)fn))
        return -1;

    /* 4. Execute */
    return fn();
}
