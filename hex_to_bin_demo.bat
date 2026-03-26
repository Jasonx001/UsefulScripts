@echo off
REM ============================================================
REM  Demo batch script: Intel HEX -> Binary conversion
REM  Adjust the variables below to match your environment.
REM ============================================================

REM Path to Python executable (use "python" or full path if needed)
SET PYTHON=python

REM Script location (same folder as this .bat by default)
SET SCRIPT=%~dp0hex_to_bin.py

REM ---- Example 1: basic conversion ----
SET INPUT=firmware\app.hex
SET OUTPUT=output\app.bin

echo [INFO] Converting %INPUT% to %OUTPUT% ...
%PYTHON% "%SCRIPT%" "%INPUT%" "%OUTPUT%"

IF ERRORLEVEL 1 (
    echo [ERROR] Conversion failed for %INPUT%.
    exit /b 1
)

REM ---- Example 2: custom fill byte (use 0x00 instead of default 0xFF) ----
SET INPUT2=firmware\bootloader.hex
SET OUTPUT2=output\sub\bootloader.bin

echo.
echo [INFO] Converting %INPUT2% to %OUTPUT2% with fill=0x00 ...
%PYTHON% "%SCRIPT%" "%INPUT2%" "%OUTPUT2%" --fill 0x00

IF ERRORLEVEL 1 (
    echo [ERROR] Conversion failed for %INPUT2%.
    exit /b 1
)

echo.
echo [INFO] All conversions completed successfully.
