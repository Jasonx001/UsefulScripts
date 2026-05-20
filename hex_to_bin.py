#!/usr/bin/env python3
"""
Intel HEX to Binary file converter.
Usage: python hex_to_bin.py <input.hex> <output.bin>
"""

import sys
import os
import argparse


def parse_intel_hex(hex_file_path):
    """Parse an Intel HEX file and return a dict of {address: byte_value}."""
    data = {}
    base_address = 0
    segment_base = 0

    with open(hex_file_path, 'r') as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line or not line.startswith(':'):
                continue

            # Strip leading ':'
            record = line[1:]

            if len(record) < 10:
                raise ValueError(f"Line {line_num}: Record too short: {line}")

            byte_count   = int(record[0:2],   16)
            address      = int(record[2:6],   16)
            record_type  = int(record[6:8],   16)
            data_bytes   = bytes.fromhex(record[8:8 + byte_count * 2])
            checksum     = int(record[8 + byte_count * 2: 10 + byte_count * 2], 16)

            # Verify checksum
            total = byte_count + (address >> 8) + (address & 0xFF) + record_type
            total += sum(data_bytes)
            if (total + checksum) & 0xFF != 0:
                raise ValueError(f"Line {line_num}: Checksum mismatch")

            if record_type == 0x00:   # Data record
                abs_address = base_address + segment_base + address
                for i, byte in enumerate(data_bytes):
                    data[abs_address + i] = byte

            elif record_type == 0x01:  # End Of File
                break

            elif record_type == 0x02:  # Extended Segment Address
                segment_base = int.from_bytes(data_bytes, 'big') << 4

            elif record_type == 0x03:  # Start Segment Address (ignored)
                pass

            elif record_type == 0x04:  # Extended Linear Address
                base_address = int.from_bytes(data_bytes, 'big') << 16

            elif record_type == 0x05:  # Start Linear Address (ignored)
                pass

            else:
                raise ValueError(f"Line {line_num}: Unknown record type: {record_type:#04x}")

    return data


def data_to_binary(data, fill_byte=0xFF):
    """Convert sparse address→byte dict to a contiguous binary blob."""
    if not data:
        return b''

    min_addr = min(data.keys())
    max_addr = max(data.keys())
    size = max_addr - min_addr + 1

    binary = bytearray([fill_byte] * size)
    for addr, byte in data.items():
        binary[addr - min_addr] = byte

    return bytes(binary)


def convert(input_path, output_path, fill_byte=0xFF):
    # Create output directory if it doesn't exist
    output_dir = os.path.dirname(output_path)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    data = parse_intel_hex(input_path)
    binary = data_to_binary(data, fill_byte)

    with open(output_path, 'wb') as f:
        f.write(binary)

    min_addr = min(data.keys()) if data else 0
    print(f"Converted '{input_path}' -> '{output_path}'")
    print(f"  Start address : 0x{min_addr:08X}")
    print(f"  Binary size   : {len(binary)} bytes ({len(binary) / 1024:.2f} KB)")


def main():
    parser = argparse.ArgumentParser(description='Convert Intel HEX file to binary.')
    parser.add_argument('input',  help='Path to input .hex file')
    parser.add_argument('output', help='Path to output .bin file')
    parser.add_argument(
        '--fill', type=lambda x: int(x, 0), default=0xFF, metavar='BYTE',
        help='Fill byte for gaps between data records (default: 0xFF)'
    )
    args = parser.parse_args()

    if not os.path.isfile(args.input):
        print(f"Error: Input file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    convert(args.input, args.output, args.fill)


if __name__ == '__main__':
    main()
