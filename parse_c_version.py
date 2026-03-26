#!/usr/bin/env python3
"""
Parse ECU_SW_VERSION constant from a C source file and return the version string.

Usage: python3 parse_c_version.py <path_to_c_source_file>
"""

import re
import sys


def parse_version(filepath: str) -> str:
    with open(filepath, "r") as f:
        content = f.read()

    # Match the ECU_SW_VERSION array initializer block, e.g.:
    #   const uint8_t ECU_SW_VERSION[6] __attribute__ ... = { 'V', 1, '.', 1, 0, 0 };
    # Strategy: find the array body that follows the ECU_SW_VERSION declaration.
    pattern = re.compile(
        r"\bECU_SW_VERSION\b"          # identifier
        r"(?:[^=]|\n)*?"               # skip __attribute__(...) and any whitespace
        r"=\s*\{"                      # = {
        r"(.*?)"                       # capture the initializer contents
        r"\}\s*;",                     # };
        re.DOTALL,
    )

    match = pattern.search(content)
    if not match:
        raise ValueError("ECU_SW_VERSION constant not found in the file.")

    initializer = match.group(1)

    # Split on commas to get individual elements, stripping whitespace
    elements = [e.strip() for e in initializer.split(",") if e.strip()]

    version = []
    for elem in elements:
        # Character literal: 'V' or '.'
        char_match = re.fullmatch(r"'(.)'", elem)
        if char_match:
            version.append(char_match.group(1))
        else:
            # Numeric literal
            num_match = re.fullmatch(r"\d+", elem)
            if num_match:
                version.append(elem)
            else:
                raise ValueError(f"Unexpected element in ECU_SW_VERSION: {elem!r}")

    return "".join(version)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <path_to_c_source_file>", file=sys.stderr)
        sys.exit(1)

    filepath = sys.argv[1]
    version = parse_version(filepath)
    print(version)


if __name__ == "__main__":
    main()
