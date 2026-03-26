#!/usr/bin/env python3
"""
release.py - Release pack script for MPLAB batch build.

Usage:
    python release.py <RELEASE_DIR> <CONF1> <CONF2> [<CONF3> <CONF4> ...]

Example:
    python release.py .\\utils\\post_build_hd20\\Release ^
        USER_VARIANT1_APP_A USER_VARIANT1_APP_B ^
        USER_VARIANT2_APP_A USER_VARIANT2_APP_B

Configs must be supplied in even count; each APP_A must have a matching APP_B
for the same variant base name (e.g. USER_VARIANT1_APP_A / USER_VARIANT1_APP_B).
"""

import sys
import os
import shutil
import subprocess
from pathlib import Path

# =========================================================
# PROJECT-LEVEL CONSTANTS  (mirror the .bat variables)
# =========================================================
MPLAB_PROJECT_NAME = "PXEBIC_mplab.X"

# All paths are relative to the current working directory (project root),
# which is where the .bat file runs from.
APP_HEX_DIR   = Path(f"./{MPLAB_PROJECT_NAME}/dist")
BOOT_HEX_DIR  = Path("./utils/post_build_hd20/BOOT_HEX")
PY_HEX_MERGE  = Path("./utils/post_build_hd20/hex_merger.py")
ZIP7          = Path(r"C:\Program Files\7-Zip\7z.exe")

BM_ECOM   = BOOT_HEX_DIR / "BIC2HD20_BM_ECOM.hex"
BM_IECOM  = BOOT_HEX_DIR / "BIC2HD20_BM_IECOM.hex"
FBL_ECOM  = BOOT_HEX_DIR / "BIC2HD20_FBL_ECOM.hex"
FBL_IECOM = BOOT_HEX_DIR / "BIC2HD20_FBL_IECOM.hex"

# Memory ranges passed to hex_merger.py
PRG_START_RANGE  = "0x0000-0x0008"
APP_A_IVT_RANGE  = "0x0008-0x0400"
BM_RANGE         = "0x0400-0x6400"
FBL_RANGE        = "0x6400-0x16000"
APP_A_RANGE      = "0x16000-0x4E000"
APP_A_CFG_RANGE  = "0x4E000-0x57FFD"
APP_B_HEAD_RANGE = "0x800000-0x800400"
APP_B_RANGE      = "0x816000-0x84E000"
APP_B_CFG_RANGE  = "0x84E000-0x857FFD"
DUAL_FLAG_RANGE  = "0x1003000-0x1003004"

# =========================================================
# ANSI COLOR HELPERS
# =========================================================
GREEN  = "\033[32m"
RED    = "\033[31m"
YELLOW = "\033[33m"
BLUE   = "\033[36m"
RESET  = "\033[0m"


def log_info(msg: str) -> None:
    print(f"[INFO] {msg}")


def log_success(msg: str) -> None:
    print(f"{GREEN}[SUCCESS] {msg}{RESET}")


def log_warn(msg: str) -> None:
    print(f"{YELLOW}[WARN] {msg}{RESET}")


def log_error(msg: str) -> None:
    print(f"{RED}[ERROR] {msg}{RESET}", file=sys.stderr)


def abort(msg: str) -> None:
    log_error(msg)
    sys.exit(1)


# =========================================================
# CONFIG PARSING
# =========================================================

def parse_variant_pairs(configs: list[str]) -> list[tuple[str, str, str]]:
    """
    Group configs into (base, cfg_a, cfg_b) triples.

    A variant base is derived by stripping the trailing '_APP_A' or '_APP_B'
    suffix (same logic as the .bat: SET BASE=!CFG:_APP_A=!).

    Raises SystemExit if any variant is missing its counterpart.
    """
    app_a_map: dict[str, str] = {}
    app_b_map: dict[str, str] = {}

    for cfg in configs:
        if "_APP_A" in cfg:
            base = cfg.replace("_APP_A", "", 1)
            app_a_map[base] = cfg
        elif "_APP_B" in cfg:
            base = cfg.replace("_APP_B", "", 1)
            app_b_map[base] = cfg
        else:
            abort(
                f"Config '{cfg}' does not contain '_APP_A' or '_APP_B'. "
                "All configurations must follow the naming convention."
            )

    # Verify symmetric pairing
    all_bases = set(app_a_map) | set(app_b_map)
    for base in sorted(all_bases):
        if base not in app_a_map:
            abort(f"Variant '{base}' has APP_B but no APP_A configuration.")
        if base not in app_b_map:
            abort(f"Variant '{base}' has APP_A but no APP_B configuration.")

    return [(base, app_a_map[base], app_b_map[base]) for base in sorted(all_bases)]


# =========================================================
# HEX MERGE
# =========================================================

def merge_hex(release_dir: Path, base: str, cfg_a: str, cfg_b: str) -> None:
    """Call hex_merger.py to produce the combined plant hex for one variant."""

    # Select ECOM or IECOM boot files (same check as the .bat)
    if "_IECOM_" in cfg_a:
        bm  = BM_IECOM
        fbl = FBL_IECOM
    else:
        bm  = BM_ECOM
        fbl = FBL_ECOM

    app_a     = APP_HEX_DIR / cfg_a / "output"     / f"{cfg_a}_CRC.hex"
    app_b     = APP_HEX_DIR / cfg_b / "output"     / f"{cfg_b}_CRC.hex"
    app_a_org = APP_HEX_DIR / cfg_a / "production" / f"{MPLAB_PROJECT_NAME}.production.hex"
    app_b_org = APP_HEX_DIR / cfg_b / "production" / f"{MPLAB_PROJECT_NAME}.production.hex"

    plant_dir  = release_dir / "PLANT"
    plant_dir.mkdir(parents=True, exist_ok=True)
    merged_hex = plant_dir / f"{base}_MNF.hex"

    print(f"  {'Found Variant':<14}: {base}")
    print(f"  {'APP_A config':<14}: {cfg_a}")
    print(f"  {'APP_B config':<14}: {cfg_b}")
    print(f"  {'Boot BM':<14}: {bm}")
    print(f"  {'Boot FBL':<14}: {fbl}")
    print()

    # Mandatory CRC hex files
    for path, label in [(app_a, "APP_A CRC hex"), (app_b, "APP_B CRC hex")]:
        if not path.exists():
            abort(f"HEX not found: {path}")

    # Original production hex files (warn, not fatal, mirrors .bat behaviour)
    for path, label in [(app_a_org, "APP_A original hex"), (app_b_org, "APP_B original hex")]:
        if not path.exists():
            log_warn(f"{label} not found: {path}")

    print(f"[MERGE] {cfg_a} + {cfg_b}")
    print(f"[OUT  ] {merged_hex}")

    cmd = [
        sys.executable, str(PY_HEX_MERGE),
        str(merged_hex),
        str(bm),          PRG_START_RANGE,
        str(app_a_org),   APP_A_IVT_RANGE,
        str(bm),          BM_RANGE,
        str(fbl),         FBL_RANGE,
        str(app_a),       APP_A_RANGE,
        str(app_a_org),   APP_A_CFG_RANGE,
        str(app_b_org),   APP_B_HEAD_RANGE,
        str(app_b),       APP_B_RANGE,
        str(app_b_org),   APP_B_CFG_RANGE,
        str(app_b_org),   DUAL_FLAG_RANGE,
    ]

    result = subprocess.run(cmd)
    if result.returncode != 0:
        abort(f"HEX merge failed for variant '{base}'")

    log_success(f"{merged_hex.name} generated")


# =========================================================
# COPY BINS
# =========================================================

def copy_bins(release_dir: Path, configs: list[str]) -> None:
    """Copy the .bin output for every configuration to RELEASE_DIR\\CUSTOMER\\."""
    tgt = release_dir / "CUSTOMER"
    tgt.mkdir(parents=True, exist_ok=True)

    for cfg in configs:
        src = APP_HEX_DIR / cfg / "output" / f"{cfg}.bin"
        log_info(f"[COPY] {src}  ->  {tgt}")
        if not src.exists():
            abort(f"BIN not found: {src}")
        shutil.copy2(src, tgt / src.name)
        log_success(f"Copied {src.name}")


# =========================================================
# ZIP
# =========================================================

def zip_release(release_dir: Path) -> None:
    """Compress RELEASE_DIR into a .zip archive using 7-Zip."""
    if not ZIP7.exists():
        abort(f"7-Zip not found at {ZIP7}")

    zip_path = release_dir.parent / (release_dir.name + ".zip")
    if zip_path.exists():
        zip_path.unlink()

    log_info(f"Compressing  {release_dir}  ->  {zip_path}")
    # Run 7-Zip from the parent so the archive root is just the folder name,
    # not the full path (e.g. "Release\" not "utils\post_build_hd20\Release\").
    result = subprocess.run(
        [str(ZIP7), "a", "-tzip", str(zip_path.resolve()), release_dir.name],
        cwd=str(release_dir.parent.resolve()),
    )
    if result.returncode != 0:
        abort("7-Zip compression failed")

    log_success(f"Archive created: {zip_path}")


# =========================================================
# TREE VIEW
# =========================================================

def print_tree(directory: Path, prefix: str = "") -> None:
    """Recursively print a directory tree (similar to the 'tree' command)."""
    try:
        entries = sorted(directory.iterdir(), key=lambda x: (x.is_file(), x.name.lower()))
    except PermissionError:
        return

    for i, entry in enumerate(entries):
        connector = "└── " if i == len(entries) - 1 else "├── "
        print(prefix + connector + entry.name)
        if entry.is_dir():
            extension = "    " if i == len(entries) - 1 else "│   "
            print_tree(entry, prefix + extension)


# =========================================================
# MAIN
# =========================================================

def main() -> None:
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)

    release_dir = Path(sys.argv[1])
    configs     = sys.argv[2:]

    print(f"{BLUE}")
    print("=" * 72)
    print(" MPLAB RELEASE PACK")
    print("=" * 72)
    print(f"  RELEASE_DIR : {release_dir.resolve()}")
    print(f"  Configs     : {', '.join(configs)}")
    print(f"={RESET}")

    # ── Validate even count ────────────────────────────────────────────────
    if len(configs) % 2 != 0:
        abort(
            f"Number of configurations must be even, got {len(configs)}.\n"
            f"  Configurations: {configs}"
        )

    release_dir.mkdir(parents=True, exist_ok=True)

    # ── Parse variant pairs ────────────────────────────────────────────────
    pairs = parse_variant_pairs(configs)

    # ── Merge HEX files ────────────────────────────────────────────────────
    print(f"\n{BLUE}-- Merging HEX files ({len(pairs)} variant(s)) --{RESET}")
    for base, cfg_a, cfg_b in pairs:
        print(f"\n[MERGE] Processing variant {base}")
        print("-" * 50)
        merge_hex(release_dir, base, cfg_a, cfg_b)

    # ── Copy BIN files ─────────────────────────────────────────────────────
    print(f"\n{BLUE}-- Copying BIN files --{RESET}")
    copy_bins(release_dir, configs)

    # ── Compress ───────────────────────────────────────────────────────────
    print(f"\n{BLUE}-- Compressing release directory --{RESET}")
    zip_release(release_dir)

    # ── Tree view ──────────────────────────────────────────────────────────
    print(f"\n{BLUE}-- Release directory tree --{RESET}")
    print(str(release_dir))
    print_tree(release_dir)

    print()
    log_success("All files were packed. Release complete!")


if __name__ == "__main__":
    main()
