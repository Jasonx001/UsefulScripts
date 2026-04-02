/**
 * flash_driver.h
 *
 * Flash driver for dsPIC33CK512MP606 bootloader.
 *
 * Flash geometry:
 *   - Page size : 2048 instructions (4096 program words, each 24-bit)
 *   - Row size  : 128 instructions  (smallest erasable: full page only)
 *   - Word      : 2 PC-words (low 16-bit + high 8-bit packed into 32-bit)
 *   - Double-word: 4 PC-words (2 instruction locations)
 *
 * All functions that touch NVM registers MUST run from RAM (see flash_driver.c).
 * The linker script places them in .ramfunc, which is loaded from flash and
 * copied to RAM before main() via flash_driver_init_ramfunc().
 */

#ifndef FLASH_DRIVER_H
#define FLASH_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <xc.h>

/* -------------------------------------------------------------------------
 * Flash geometry
 * ---------------------------------------------------------------------- */
#define FLASH_PAGE_SIZE_INSTR    2048u          /* instructions per page    */
#define FLASH_PAGE_SIZE_BYTES    (FLASH_PAGE_SIZE_INSTR * 3u) /* 24-bit instr */

/* -------------------------------------------------------------------------
 * Address map  (match your .gld ORIGIN values)
 * ---------------------------------------------------------------------- */
#define BOOTLOADER_FLASH_START   0x000200ul     /* after reset/IVT vectors  */
#define BOOTLOADER_FLASH_END     0x007FFFul     /* ~32 KB reserved for BL   */

#define APPLICATION_FLASH_START  0x008000ul     /* application code start   */
#define APPLICATION_FLASH_END    0x055FFFul     /* rest of 512 KB flash     */

/* -------------------------------------------------------------------------
 * Error codes
 * ---------------------------------------------------------------------- */
typedef enum {
    FLASH_OK              =  0,
    FLASH_ERR_ALIGNMENT   = -1,   /* address not aligned to required boundary */
    FLASH_ERR_RANGE       = -2,   /* address outside allowed application area */
    FLASH_ERR_WR_PROTECT  = -3,   /* write-protect violation                  */
    FLASH_ERR_VERIFY      = -4,   /* readback mismatch after write            */
} FlashStatus;

/* -------------------------------------------------------------------------
 * Public API  (all execute from RAM – see flash_driver.c)
 * ---------------------------------------------------------------------- */

/**
 * Must be called once at startup (before any flash operation) to copy the
 * .ramfunc section from flash to RAM.
 */
void flash_driver_init_ramfunc(void);

/**
 * Erase a single flash page (2048 instructions).
 * @param page_addr  Any address within the target page (will be aligned down).
 *                   Must be in APPLICATION_FLASH_START … APPLICATION_FLASH_END.
 */
FlashStatus flash_erase_page(uint32_t page_addr);

/**
 * Write a single double-word (2 instruction locations = 4 PC-words).
 * @param addr     Destination address, must be double-word aligned (addr & 3 == 0).
 * @param data_lo  Low 32 bits  (instruction 0 low word | instruction 0 high byte<<16)
 * @param data_hi  High 32 bits (instruction 1 low word | instruction 1 high byte<<16)
 */
FlashStatus flash_write_double_word(uint32_t addr,
                                    uint32_t data_lo,
                                    uint32_t data_hi);

/**
 * Write a row (128 instructions) from a RAM buffer.
 * @param dest_addr  Row-aligned destination address.
 * @param src        Pointer to 128 × 2 uint32_t values (lo/hi pairs per instruction).
 *                   Array length must be 256.
 */
FlashStatus flash_write_row(uint32_t dest_addr, const uint32_t *src);

/**
 * Read a 24-bit instruction word.
 * @param addr   Program memory address.
 * @param word   Output: packed as  word[15:0] = low word, word[23:16] = upper byte.
 */
FlashStatus flash_read_word(uint32_t addr, uint32_t *word);

#endif /* FLASH_DRIVER_H */
