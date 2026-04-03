# CAN Bus Dynamic Function Loading – dsPIC33CK512MP606

## Overview

This guide explains how to compile a function to a hex image, transmit it
over CAN bus, load it into RAM, and execute it.

---

## Step 1 – Compile the "payload" function to a raw binary

The payload function must be compiled **separately**, linked at the exact RAM
address where you will place it (`_can_func_pool` in this demo), so that all
internal branch offsets are correct.

```bash
# Example XC16 build of a single C file to a raw binary
xc16-gcc payload_fn.c \
    -mcpu=33CK512MP606 \
    -mno-eds-warn \
    -Os \
    -Wl,--script=payload_link.gld \   # see below
    -o payload.elf

xc16-objcopy -O binary payload.elf payload.bin
```

`payload_link.gld` must set the VMA/LMA to the base address of `_can_func_pool`.
That address is printed at boot by `main.c` (look at the `[CAN]` log lines),
or you can fix it with `__attribute__((address(0x00A000)))` on the pool array.

### Position-Independent Code alternative

If you cannot fix the address ahead of time, compile with:
```bash
-fPIC   # not fully supported on dsPIC – prefer fixed-address approach
```
dsPIC33CK has limited PIC support; the fixed-address method is recommended.

---

## Step 2 – Convert the binary to CAN-transmittable frames

Each CAN 2.0B frame carries up to **8 bytes** of payload.  
Define a simple protocol:

| Byte | Field        | Description                              |
|------|--------------|------------------------------------------|
| 0-1  | Offset (LE)  | Byte offset within the function image    |
| 2-3  | Length (LE)  | Number of data bytes in this frame (≤ 6)|
| 4-9  | Data[0..5]  | Function binary bytes                    |

Last frame: send a special "commit" frame (offset = 0xFFFF) carrying the
CRC-16 of the entire image.

A host-side Python script to split and send:
```python
import can, struct, binascii

bus = can.interface.Bus(channel='can0', bustype='socketcan')

with open('payload.bin', 'rb') as f:
    data = f.read()

crc = binascii.crc_hqx(data, 0xFFFF)  # CRC-16/CCITT

CHUNK = 6
for offset in range(0, len(data), CHUNK):
    chunk = data[offset:offset+CHUNK]
    frame = struct.pack('<HH', offset, len(chunk)) + chunk
    bus.send(can.Message(arbitration_id=0x123, data=frame, is_extended_id=False))

# Commit frame
bus.send(can.Message(
    arbitration_id=0x123,
    data=struct.pack('<HH', 0xFFFF, crc),
    is_extended_id=False
))
```

---

## Step 3 – Receive on dsPIC side

In your CAN RX ISR or task:

```c
#include "ram_demo.h"

void CAN_RX_handler(uint8_t *frame_data, uint8_t dlc)
{
    uint16_t offset = *(uint16_t *)&frame_data[0];
    uint16_t length = *(uint16_t *)&frame_data[2];

    if (offset == 0xFFFFu)
    {
        /* Commit: frame_data[2..3] = expected CRC */
        uint16_t expected_crc = *(uint16_t *)&frame_data[2];
        int result = can_execute_loaded_function(expected_crc);
        if (result == -1)
            printf("[CAN] CRC mismatch or bad address – NOT executed\r\n");
        else
            printf("[CAN] Function returned %d\r\n", result);
    }
    else
    {
        can_load_function_chunk(&frame_data[4], offset, (uint8_t)length);
    }
}
```

---

## Step 4 – Execution flow summary

```
Flash (hex file)          Data RAM
┌────────────────┐        ┌────────────────────────────┐
│ .text          │        │ .data / .bss               │
│ .ramfunc (LMA)─┼──────► │ .ramfunc (VMA) ← init copy │  <- led_blink_from_ram
│                │        │                            │
│                │  CAN   │ _can_func_pool ────────────┼── can_execute_loaded_function()
│                │ ─────► │  (received payload bytes)  │
└────────────────┘        └────────────────────────────┘
```

---

## Checklist before executing CAN-loaded code

- [ ] CRC-16 passes
- [ ] `is_in_ram()` returns 1 for the target address
- [ ] Payload was compiled at (or relocated to) the exact pool base address
- [ ] No Flash erase/write operations are in progress while executing from RAM
- [ ] Stack usage of payload function is within bounds (watchdog optional)

---

## Official References

| Document        | Title / Notes |
|-----------------|---------------|
| **DS70005349**  | dsPIC33CK512MP606 Datasheet – Table 4-1 (memory map), Section 4 |
| **DS50002071**  | MPLAB XC16 C Compiler User's Guide – Ch. 14 (attributes), Ch. 15 (linker scripts) |
| **DS70000657**  | dsPIC33C Family Reference Manual – Memory Organisation |
| **TB3261**      | Microchip Tech Brief: "Running Code from RAM on dsPIC33C devices" |
| **AN1264**      | "Implementing a CAN Bootloader for dsPIC DSC Devices" |
| **AN1095**      | "Self-Write/Erase (RTSP) for dsPIC33F" – still relevant for flash-write context |
| **DS50002666**  | MPLAB X IDE User's Guide – Linker script customisation |

> **Tip:** Search Microchip's documentation library at
> https://www.microchip.com/en-us/search#q=dsPIC33CK%20RAM%20execution
> for the most up-to-date application notes.
