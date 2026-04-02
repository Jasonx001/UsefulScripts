/**
 * flash_driver.c
 *
 * Flash driver for dsPIC33CK512MP606 bootloader.
 *
 * Key XC16 attributes used
 * ------------------------
 *   __attribute__((section(".ramfunc")))
 *       Places the function's machine code in the .ramfunc output section.
 *       The linker script loads this section from flash (LMA = program flash)
 *       but maps its VMA (run address) into data RAM.
 *       flash_driver_init_ramfunc() copies the bytes at startup.
 *
 *   __attribute__((near))
 *       Optional: tells the compiler the function is reachable via a short
 *       (near) call, saving one instruction.  Only useful if the RAM region
 *       is below 0x8000 (data address space lower half).
 *
 * NVM operation sequence (dsPIC33CK reference manual §8)
 * ------------------------------------------------------
 *   1. Load NVMADR with the target address.
 *   2. Load NVMDAT / staging registers with data (writes only).
 *   3. Set NVMCON.NVMOP to the desired operation.
 *   4. Set NVMCON.WREN = 1.
 *   5. Write the unlock sequence to NVMKEY (0x55 then 0xAA).
 *   6. Set NVMCON.WR = 1  — hardware clears WR when done.
 *   7. Wait for WR == 0.
 *   8. Clear NVMCON.WREN = 0.
 *   9. Check NVMCON.WRERR.
 */

#include "flash_driver.h"
#include <string.h>    /* memcpy */

/* -------------------------------------------------------------------------
 * Linker-provided symbols for the .ramfunc copy (see .gld)
 *   _ramfunc_lma_start  – load address in flash (source for copy)
 *   _ramfunc_vma_start  – run address  in RAM   (destination)
 *   _ramfunc_vma_end    – end of RAM region
 * ---------------------------------------------------------------------- */
extern uint8_t _ramfunc_lma_start;
extern uint8_t _ramfunc_vma_start;
extern uint8_t _ramfunc_vma_end;

/* -------------------------------------------------------------------------
 * Internal helpers – also placed in RAM so they can be called from
 * .ramfunc functions without leaving RAM.
 * ---------------------------------------------------------------------- */

/**
 * Perform the mandatory NVM unlock sequence and trigger the operation.
 * Interrupts MUST be disabled by the caller before this function is called.
 */
static void __attribute__((section(".ramfunc"), near))
nvm_unlock_and_start(void)
{
    NVMKEY = 0x55;
    NVMKEY = 0xAA;
    NVMCONbits.WR = 1;
    /* Wait for hardware to clear WR */
    while (NVMCONbits.WR) {
        /* spin – takes ~4 ms for page erase, ~1 µs for word write */
    }
}

/**
 * Validate that addr falls within the writable application region and
 * satisfies the given alignment mask.
 */
static FlashStatus __attribute__((section(".ramfunc"), near))
validate_address(uint32_t addr, uint32_t align_mask)
{
    if (addr < APPLICATION_FLASH_START || addr > APPLICATION_FLASH_END) {
        return FLASH_ERR_RANGE;
    }
    if (addr & align_mask) {
        return FLASH_ERR_ALIGNMENT;
    }
    return FLASH_OK;
}

/* -------------------------------------------------------------------------
 * Public API implementation
 * ---------------------------------------------------------------------- */

/* Called before main() – copies .ramfunc section from flash to RAM */
void flash_driver_init_ramfunc(void)
{
    size_t len = (size_t)(&_ramfunc_vma_end - &_ramfunc_vma_start);
    if (len > 0u) {
        memcpy(&_ramfunc_vma_start, &_ramfunc_lma_start, len);
    }
}

/* ---------- Page erase -------------------------------------------------- */

FlashStatus __attribute__((section(".ramfunc"), near))
flash_erase_page(uint32_t page_addr)
{
    FlashStatus status;
    uint32_t    aligned;
    uint16_t    sr_save;

    /* Align down to page boundary */
    aligned = page_addr & ~((uint32_t)(FLASH_PAGE_SIZE_INSTR - 1u));

    status = validate_address(aligned, 0u); /* page align already done */
    if (status != FLASH_OK) {
        return status;
    }

    /* Load address registers (24-bit PC address split into NVMADRU:NVMADR) */
    NVMADRU = (uint16_t)(aligned >> 16u);
    NVMADR  = (uint16_t)(aligned & 0xFFFFu);

    /* Select page-erase operation */
    NVMCON = 0x4003;   /* NVMOP = 0b0011 (page erase), WREN = 0 initially */

    /* Disable interrupts around the critical unlock sequence */
    __builtin_disi(6);               /* disable interrupts for 6 cycles    */
    sr_save = SR;
    SR |= 0x00E0u;                   /* IPL = 7, mask all user interrupts  */

    NVMCONbits.WREN = 1;
    nvm_unlock_and_start();
    NVMCONbits.WREN = 0;

    SR = sr_save;

    return NVMCONbits.WRERR ? FLASH_ERR_WR_PROTECT : FLASH_OK;
}

/* ---------- Double-word write ------------------------------------------- */

FlashStatus __attribute__((section(".ramfunc"), near))
flash_write_double_word(uint32_t addr,
                        uint32_t data_lo,
                        uint32_t data_hi)
{
    FlashStatus status;
    uint16_t    sr_save;

    /* Double-word requires 4-word (2-instruction) alignment */
    status = validate_address(addr, 0x3u);
    if (status != FLASH_OK) {
        return status;
    }

    NVMADRU = (uint16_t)(addr >> 16u);
    NVMADR  = (uint16_t)(addr & 0xFFFFu);

    /* Load the two instruction words into the staging registers.
     * NVMDAT0/1 hold the low words; NVMDAT2/3 hold the phantom (upper) bytes.
     */
    NVMDAT0 = (uint16_t)(data_lo & 0xFFFFu);
    NVMDAT1 = (uint16_t)(data_lo >> 16u);    /* upper byte in bits [7:0]   */
    NVMDAT2 = (uint16_t)(data_hi & 0xFFFFu);
    NVMDAT3 = (uint16_t)(data_hi >> 16u);

    /* Select double-word-program operation: NVMOP = 0b0001 */
    NVMCON = 0x4001;

    __builtin_disi(6);
    sr_save = SR;
    SR |= 0x00E0u;

    NVMCONbits.WREN = 1;
    nvm_unlock_and_start();
    NVMCONbits.WREN = 0;

    SR = sr_save;

    return NVMCONbits.WRERR ? FLASH_ERR_WR_PROTECT : FLASH_OK;
}

/* ---------- Row write --------------------------------------------------- */

/**
 * Row size in instruction locations (each needs a lo+hi uint32_t pair).
 * dsPIC33CK row = 128 instructions.
 */
#define ROW_SIZE_INSTR  128u
#define ROW_ALIGN_MASK  ((uint32_t)(ROW_SIZE_INSTR - 1u))

FlashStatus __attribute__((section(".ramfunc"), near))
flash_write_row(uint32_t dest_addr, const uint32_t *src)
{
    FlashStatus status;
    uint32_t    i;
    uint16_t    sr_save;

    status = validate_address(dest_addr, ROW_ALIGN_MASK);
    if (status != FLASH_OK) {
        return status;
    }

    /* Write row as 64 double-words */
    for (i = 0u; i < ROW_SIZE_INSTR; i += 2u) {
        uint32_t word_addr = dest_addr + i;

        NVMADRU = (uint16_t)(word_addr >> 16u);
        NVMADR  = (uint16_t)(word_addr & 0xFFFFu);

        /* src is an array of uint32_t where each element encodes one
         * instruction as: [15:0] low word, [23:16] upper byte, [31:24] = 0 */
        NVMDAT0 = (uint16_t)(src[i]     & 0xFFFFu);
        NVMDAT1 = (uint16_t)(src[i]     >> 16u);
        NVMDAT2 = (uint16_t)(src[i + 1] & 0xFFFFu);
        NVMDAT3 = (uint16_t)(src[i + 1] >> 16u);

        NVMCON = 0x4001;   /* double-word program */

        __builtin_disi(6);
        sr_save = SR;
        SR |= 0x00E0u;

        NVMCONbits.WREN = 1;
        nvm_unlock_and_start();
        NVMCONbits.WREN = 0;

        SR = sr_save;

        if (NVMCONbits.WRERR) {
            return FLASH_ERR_WR_PROTECT;
        }
    }
    return FLASH_OK;
}

/* ---------- Word read --------------------------------------------------- */

/*
 * Reading program memory uses the Table Read (TBLRD) instruction via the
 * __builtin_tblrdl / __builtin_tblrdh builtins.
 * This does NOT need to run from RAM (reads don't touch NVM write registers).
 */
FlashStatus flash_read_word(uint32_t addr, uint32_t *word)
{
    uint16_t lo, hi;

    if (addr < BOOTLOADER_FLASH_START || addr > APPLICATION_FLASH_END) {
        return FLASH_ERR_RANGE;
    }

    /* XC16 builtins: argument is the 24-bit PC address */
    lo = __builtin_tblrdl(addr);   /* lower 16 bits of the instruction word */
    hi = __builtin_tblrdh(addr);   /* upper  8 bits (phantom byte), in [7:0] */

    *word = ((uint32_t)hi << 16u) | lo;
    return FLASH_OK;
}
