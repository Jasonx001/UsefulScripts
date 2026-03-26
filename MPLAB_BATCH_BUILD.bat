@echo off
@REM color f0
title = MPLAB BATCH
goto Start
:Start
rem **********************************************************************************************************
rem * Update log:
rem * --V1.0 - Inital release @2026/03/12
rem * --V1.1 - [Release] section delegated to release.py @2026/03/26
rem **********************************************************************************************************
REM =========================================================
REM COLOR SETTING
REM =========================================================
set CGreen=[32m
Set CRed=[31m
set CYellow=[33m
set CBlue=[36m
Set CEnd=[0m

REM =========================================================
REM MPLAB ENVIRONMENT SETTING
REM =========================================================
set "MPLABX=C:\Program Files\Microchip\MPLABX\v5.50"
set "MAKEFILE_GEN=%MPLABX%\mplab_platform\bin\prjMakefilesGenerator.bat"
set "PROJECT_GEN_FOLDER=.generated_files\flags"
set "PATH=%MPLABX%\gnuBins\GnuWin32\bin;%PATH%"
set "MPLAB_PROJECT_NAME=PXEBIC_mplab.X"
set "CONFIG_XML=.\%MPLAB_PROJECT_NAME%\nbproject\configurations.xml"
set "PROJECT_NB=.\%MPLAB_PROJECT_NAME%\nbproject"
set "LOGDIR=.\utils\build\logs"
set "POST_BUILD_DIR=.\utils\post_build_hd20"
set "RELEASE_DIR=.\utils\post_build_hd20\Release"
set "RELEASE_PY=.\utils\post_build_hd20\release.py"
set Zip7="C:\Program Files\7-Zip\7z.exe"
set PYTHON_EXE=python

REM =========================================================
REM PARSE CONFIGURATIONS
REM =========================================================
setlocal EnableDelayedExpansion
if not exist %CONFIG_XML% echo ERROR: Can not find the CONFIG_XML!
set index=0
for /f tokens^=2^ delims^=^" %%a in ('findstr /C:"<conf name=" "%CONFIG_XML%"') do (
    set /a index+=1
    set "conf[!index!]=%%a"
    @REM echo Found config: %%a
)

REM Setup environment
echo Setting up environment....
set "PROJECT_PATH=%CD%\%MPLAB_PROJECT_NAME%"
for /l %%i in (1,1,%index%) do (
    set "TGT_FLAG_PATH=%PROJECT_PATH%\%PROJECT_GEN_FOLDER%\!conf[%%i]!"
    echo [PREPARE] Generating flags for !conf[%%i]!...
    if not exist "!TGT_FLAG_PATH!\" (
        echo process falgs for !conf[%%i]! in !TGT_FLAG_PATH!\
        CALL "%MAKEFILE_GEN%" "%PROJECT_PATH%"@!conf[%%i]!
    )
)

if not exist %LOGDIR% mkdir %LOGDIR%
if not exist %RELEASE_DIR% mkdir %RELEASE_DIR%
cls
echo *********************************************************************************************************
echo *     Name: MPLAB BATCH BUILD.bat                                                                       *
echo *     Version: V1.1                                                                                     *
echo *     Target: This script is written for batch building multimul configurations                         *
echo *********************************************************************************************************
echo %CBlue%--Build Options-------------------------------------------------------------------------
echo  1 -- [CLEAN] Clean all               [ Delete all the files in dist                 ]
echo  2 -- [BUILD] Build all               [ Build all configurations                     ]
echo  3 -- [RELEASE] Release all           [ Pack all configurations to a zip file        ]
echo  p -- [POSTB] Post Build              [ Post build for all configurations            ]
echo  u -- [CONF] Update Config            [ Update make files after configuration change ]
echo --------------------------------------------------------------------------------------%CEnd%

:Instruction
choice /c 123pu /n /m "Please enter your option here... "
set Op=%errorlevel%
if 1==%Op% goto [CLean]Clean_All
if 2==%Op% goto [Build]Build_All
if 3==%Op% goto [Release]Release_Pack
if 4==%Op% goto [POST_Build]Post_Build
if 5==%Op% goto [CONF]Update_Makefiles


:[CONF]Update_Makefiles
for /l %%i in (1,1,%index%) do (
    echo [CONF] Update for !conf[%%i]!...
    set "TGT_FLAG_PATH=%PROJECT_PATH%\%PROJECT_GEN_FOLDER%\!conf[%%i]!"
    rd /s /q "!TGT_FLAG_PATH!"
    CALL "%MAKEFILE_GEN%" "%PROJECT_PATH%"@!conf[%%i]!
    echo %CGreen%[SUCCESSS] Successfully updated for !conf[%%i]! %CEnd%
)
echo %CGreen%[SUCCESSS] Successfully updated for all configurations! %CEnd%
goto End

:[POST_Build]Post_Build
echo [POST BUILD] Post build for all configurations...
for /l %%i in (1,1,%index%) do (
    echo [POST BUILD] Post build for !conf[%%i]!...
    CALL %POST_BUILD_DIR%\post_build.bat !conf[%%i]!
    if errorlevel 1 (
        echo %CRed%[ERROR] Failed to post build !conf[%%i]!%CEnd%
        goto End
    )
    echo %CGreen%[SUCCESSS] Successfully post build for !conf[%%i]! %CEnd%
)
echo %CGreen%[SUCCESSS] Successfully post build for all configurations! %CEnd%
goto End


:[CLean]Clean_All
echo [CLEAN] Cleaning all configurations
for /l %%i in (1,1,%index%) do (
    echo [CLEAN] Cleaning for !conf[%%i]!...
    make -C %MPLAB_PROJECT_NAME% -j%NUMBER_OF_PROCESSORS% CONF=!conf[%%i]! clean
    echo %CGreen%[SUCCESSS] Successfully cleaned for !conf[%%i]! %CEnd%
)
echo %CGreen%[SUCCESSS] All history files were cleaned!%CEnd%
goto End

:[Build]Build_All
for /l %%i in (1,1,%index%) do (
    echo [BUILD] Start building for "!conf[%%i]!"...

    call :NeedRegen "!conf[%%i]!"
        if errorlevel 1 (
            echo %CYellow%[UPDATE] Makefiles for "!conf[%%i]!" need regeneration%CEnd%
            call "%MAKEFILE_GEN%" "%PROJECT_PATH%"@!conf[%%i]!
        ) else (
            echo [SKIP] !conf[%%i]! is up-to-date
        )

    REM Build process
    make -C %MPLAB_PROJECT_NAME% CONF=!conf[%%i]! ^
        build ^
        -j%NUMBER_OF_PROCESSORS% ^
        2>&1 | tee %LOGDIR%\build_!conf[%%i]!.log

    if errorlevel 1 (
        echo %CRed%[ERROR] Build failed for !conf[%%i]!%CEnd%
        goto End
    )

    echo post building for !conf[%%i]!...
    CALL %POST_BUILD_DIR%\post_build.bat !conf[%%i]!

    echo %CGreen%[SUCCESS] Successfully build !conf[%%i]!%CEnd%
    echo =======================Build End=========================
)

echo %CGreen%[SUCCESS] All configurations were built successfully!%CEnd%
goto End

:[Release]Release_Pack
echo [RELEASE] Collecting configurations...

REM Build a space-separated list of all config names from the parsed conf array
set "CONF_ARGS="
for /l %%i in (1,1,%index%) do (
    set "CONF_ARGS=!CONF_ARGS! !conf[%%i]!"
)

echo [RELEASE] Calling release.py with %index% configuration(s):
echo           %CONF_ARGS%
echo.

%PYTHON_EXE% "%RELEASE_PY%" "%RELEASE_DIR%" !CONF_ARGS!

if errorlevel 1 (
    echo %CRed%[ERROR] Release pack failed%CEnd%
    goto End
)

echo %CGreen%[SUCCESS] Release pack completed successfully!%CEnd%
goto End

:End
echo Press any key to continue...
pause>nul
goto Start

REM =========================================================
REM FUNCTION: NEED REGEN
REM =========================================================
:NeedRegen
set "FLAG_PATH=%PROJECT_PATH%\%PROJECT_GEN_FOLDER%\%~1"
set "MAKEFILE_NAME=%PROJECT_NB%\Makefile-%~1.mk"

if not exist "!FLAG_PATH!\" (
    echo Regenerating because of no flag folder found!
    exit /b 1
)

for %%a in ("%CONFIG_XML%") do set CFG_TIME=%%~ta
for %%b in ("!MAKEFILE_NAME!") do set MAKEFILE_TIME=%%~tb

if "!CFG_TIME!" GTR "!MAKEFILE_TIME!" (
    echo Regenerating because of configuration file has been changed!
    rd /s /q "!FLAG_PATH!"
    exit /b 1
)
exit /b 0
