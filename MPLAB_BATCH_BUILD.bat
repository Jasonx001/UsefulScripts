@echo off
title = MPLAB BATCH
goto Start
:Start
rem **********************************************************************************************************
rem * Update log:
rem * --V1.0 - Initial release @2026/03/12
rem * --V1.1 - [Release] section delegated to release.py @2026/03/26
rem * --V1.2 - Per-config build options (1-9/0), parallel build-all, build timing @2026/03/29
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
if not exist %CONFIG_XML% echo ERROR: Cannot find CONFIG_XML!
set index=0
for /f tokens^=2^ delims^=^" %%a in ('findstr /C:"<conf name=" "%CONFIG_XML%"') do (
    set /a index+=1
    set "conf[!index!]=%%a"
)

REM Setup environment
echo Setting up environment....
set "PROJECT_PATH=%CD%\%MPLAB_PROJECT_NAME%"
for /l %%i in (1,1,%index%) do (
    set "TGT_FLAG_PATH=%PROJECT_PATH%\%PROJECT_GEN_FOLDER%\!conf[%%i]!"
    echo [PREPARE] Generating flags for !conf[%%i]!...
    if not exist "!TGT_FLAG_PATH!\" (
        echo process flags for !conf[%%i]! in !TGT_FLAG_PATH!\
        CALL "%MAKEFILE_GEN%" "%PROJECT_PATH%"@!conf[%%i]!
    )
)

if not exist %LOGDIR% mkdir %LOGDIR%
if not exist %RELEASE_DIR% mkdir %RELEASE_DIR%
cls
echo *********************************************************************************************************
echo *     Name: MPLAB BATCH BUILD.bat                                                                       *
echo *     Version: V1.2                                                                                     *
echo *     Target: This script is written for batch building multiple configurations                         *
echo *********************************************************************************************************
echo %CBlue%--Build Options-------------------------------------------------------------------------
echo  c -- [CLEAN]   Clean all configurations
echo  a -- [BUILD]   Build all configurations (parallel)
echo  r -- [RELEASE] Release all  ^(pack to zip^)
echo  p -- [POSTB]   Post Build for all configurations
echo  u -- [CONF]    Update Config ^(regenerate make files^)
echo  ----Individual builds ^(clean first^)--------------------------------------------
for /l %%i in (1,1,%index%) do (
    set "_d=%%i"
    if %%i equ 10 set "_d=0"
    echo  !_d! -- [BUILD]   Build !conf[%%i]!
)
echo --------------------------------------------------------------------------------------%CEnd%

:Instruction
set "CHOICE_STR=carpu"
for /l %%i in (1,1,%index%) do (
    set "_d=%%i"
    if %%i equ 10 set "_d=0"
    set "CHOICE_STR=!CHOICE_STR!!_d!"
)
choice /c !CHOICE_STR! /n /m "Please enter your option here... "
set Op=%errorlevel%
if "!Op!"=="1" goto [Clean]Clean_All
if "!Op!"=="2" goto [Build]Build_All
if "!Op!"=="3" goto [Release]Release_Pack
if "!Op!"=="4" goto [POST_Build]Post_Build
if "!Op!"=="5" goto [CONF]Update_Makefiles
set /a BUILD_IDX=!Op!-5
goto [Build]Single_Config


REM =========================================================
:[CONF]Update_Makefiles
REM =========================================================
for /l %%i in (1,1,%index%) do (
    echo [CONF] Update for !conf[%%i]!...
    set "TGT_FLAG_PATH=%PROJECT_PATH%\%PROJECT_GEN_FOLDER%\!conf[%%i]!"
    rd /s /q "!TGT_FLAG_PATH!"
    CALL "%MAKEFILE_GEN%" "%PROJECT_PATH%"@!conf[%%i]!
    echo %CGreen%[SUCCESS] Successfully updated for !conf[%%i]! %CEnd%
)
echo %CGreen%[SUCCESS] Successfully updated all configurations! %CEnd%
goto End


REM =========================================================
:[POST_Build]Post_Build
REM =========================================================
echo [POST BUILD] Post build for all configurations...
for /l %%i in (1,1,%index%) do (
    echo [POST BUILD] !conf[%%i]!...
    CALL %POST_BUILD_DIR%\post_build.bat !conf[%%i]!
    if errorlevel 1 (
        echo %CRed%[ERROR] Post build failed for !conf[%%i]!%CEnd%
        goto End
    )
    echo %CGreen%[SUCCESS] Post build completed for !conf[%%i]! %CEnd%
)
echo %CGreen%[SUCCESS] Post build completed for all configurations! %CEnd%
goto End


REM =========================================================
:[Clean]Clean_All
REM =========================================================
echo [CLEAN] Cleaning all configurations...
for /l %%i in (1,1,%index%) do (
    echo [CLEAN] Cleaning !conf[%%i]!...
    make -C %MPLAB_PROJECT_NAME% -j%NUMBER_OF_PROCESSORS% CONF=!conf[%%i]! clean
    echo %CGreen%[SUCCESS] Cleaned !conf[%%i]! %CEnd%
)
echo %CGreen%[SUCCESS] All configurations cleaned!%CEnd%
goto End


REM =========================================================
:[Build]Build_All
REM =========================================================
REM Builds all configurations in parallel, then runs post-build sequentially.
echo [BUILD] Starting parallel build for all %index% configuration(s)...
set "T_START=%time: =0%"

REM Clean stale flag files from previous runs
for /l %%i in (1,1,%index%) do (
    del /f /q "%LOGDIR%\ok_%%i.flag" "%LOGDIR%\fail_%%i.flag" 2>nul
)

REM Regen check (sequential) then launch each build in background
for /l %%i in (1,1,%index%) do (
    call :NeedRegen "!conf[%%i]!"
    if errorlevel 1 (
        echo %CYellow%[UPDATE] Regenerating makefiles for !conf[%%i]!%CEnd%
        call "%MAKEFILE_GEN%" "%PROJECT_PATH%"@!conf[%%i]!
    ) else (
        echo [SKIP] !conf[%%i]! makefiles are up-to-date
    )
    echo [BUILD] Launching !conf[%%i]! ^(background^)...
    call :LaunchBuild "!conf[%%i]!" %%i
)

REM Poll until every build has written its ok/fail flag
echo [BUILD] Waiting for all builds to complete...
:WaitAllBuilds
set "ALL_DONE=1"
for /l %%i in (1,1,%index%) do (
    if not exist "%LOGDIR%\ok_%%i.flag" (
        if not exist "%LOGDIR%\fail_%%i.flag" (
            set "ALL_DONE=0"
        )
    )
)
if "!ALL_DONE!"=="0" (
    timeout /t 3 /nobreak >nul
    goto WaitAllBuilds
)

call :PrintElapsed "!T_START!" "Parallel build"

REM Check results
set "BUILD_FAILED=0"
for /l %%i in (1,1,%index%) do (
    if exist "%LOGDIR%\fail_%%i.flag" (
        echo %CRed%[ERROR] Build failed for !conf[%%i]! -- see %LOGDIR%\build_!conf[%%i]!.log%CEnd%
        set "BUILD_FAILED=1"
    ) else (
        echo %CGreen%[SUCCESS] Build completed for !conf[%%i]!%CEnd%
    )
)
if "!BUILD_FAILED!"=="1" goto End

REM Post-build runs sequentially after all builds complete
for /l %%i in (1,1,%index%) do (
    echo [POST BUILD] !conf[%%i]!...
    CALL %POST_BUILD_DIR%\post_build.bat !conf[%%i]!
    if errorlevel 1 (
        echo %CRed%[ERROR] Post build failed for !conf[%%i]!%CEnd%
        goto End
    )
    echo %CGreen%[SUCCESS] Post build completed for !conf[%%i]!%CEnd%
)
echo %CGreen%[SUCCESS] All configurations built successfully!%CEnd%
goto End


REM =========================================================
:[Build]Single_Config
REM =========================================================
REM Builds one configuration selected by number; cleans first.
if !BUILD_IDX! gtr !index! (
    echo %CRed%[ERROR] Configuration !BUILD_IDX! does not exist ^(only %index% found^)%CEnd%
    goto End
)
set "SINGLE_CONF=!conf[%BUILD_IDX%]!"
echo [BUILD] Selected: !SINGLE_CONF!
set "T_START=%time: =0%"

REM Clean
echo [CLEAN] Cleaning !SINGLE_CONF!...
make -C %MPLAB_PROJECT_NAME% -j%NUMBER_OF_PROCESSORS% CONF=!SINGLE_CONF! clean
if errorlevel 1 (
    echo %CRed%[ERROR] Clean failed for !SINGLE_CONF!%CEnd%
    goto End
)

REM Regen check
call :NeedRegen "!SINGLE_CONF!"
if errorlevel 1 (
    echo %CYellow%[UPDATE] Regenerating makefiles for !SINGLE_CONF!%CEnd%
    call "%MAKEFILE_GEN%" "%PROJECT_PATH%"@!SINGLE_CONF!
) else (
    echo [SKIP] !SINGLE_CONF! makefiles are up-to-date
)

REM Build
echo [BUILD] Building !SINGLE_CONF!...
make -C %MPLAB_PROJECT_NAME% CONF=!SINGLE_CONF! build -j%NUMBER_OF_PROCESSORS% ^
    2>&1 | tee %LOGDIR%\build_!SINGLE_CONF!.log
if errorlevel 1 (
    echo %CRed%[ERROR] Build failed for !SINGLE_CONF!%CEnd%
    call :PrintElapsed "!T_START!" "!SINGLE_CONF!"
    goto End
)

REM Post-build
echo [POST BUILD] !SINGLE_CONF!...
CALL %POST_BUILD_DIR%\post_build.bat !SINGLE_CONF!
if errorlevel 1 (
    echo %CRed%[ERROR] Post build failed for !SINGLE_CONF!%CEnd%
    call :PrintElapsed "!T_START!" "!SINGLE_CONF!"
    goto End
)

call :PrintElapsed "!T_START!" "!SINGLE_CONF!"
echo %CGreen%[SUCCESS] !SINGLE_CONF! built successfully!%CEnd%
echo =======================Build End=========================
goto End


REM =========================================================
:[Release]Release_Pack
REM =========================================================
echo [RELEASE] Collecting configurations...
set "CONF_ARGS="
for /l %%i in (1,1,%index%) do (
    set "CONF_ARGS=!CONF_ARGS! !conf[%%i]!"
)

echo [RELEASE] Calling release.py with %index% configuration(s):
echo           !CONF_ARGS!
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
REM FUNCTION: LaunchBuild  %1=conf_name  %2=index
REM Starts a background build; writes ok_N.flag or fail_N.flag on completion.
REM =========================================================
:LaunchBuild
REM Each background build runs serially (-j1) so that N configs running
REM simultaneously do not flood the system with N*PROCESSORS xc16-gcc
REM processes, which causes "CreateProcess: No such file or directory".
start "" cmd /c "make -C %MPLAB_PROJECT_NAME% CONF=%~1 build -j1 > %LOGDIR%\build_%~1.log 2>&1 && (echo done > %LOGDIR%\ok_%~2.flag) || (echo done > %LOGDIR%\fail_%~2.flag)"
exit /b 0


REM =========================================================
REM FUNCTION: NeedRegen  %1=conf_name
REM Returns errorlevel 1 if makefiles need regeneration.
REM =========================================================
:NeedRegen
set "FLAG_PATH=%PROJECT_PATH%\%PROJECT_GEN_FOLDER%\%~1"
set "MAKEFILE_NAME=%PROJECT_NB%\Makefile-%~1.mk"

if not exist "!FLAG_PATH!\" (
    echo Regenerating: flag folder not found
    exit /b 1
)

for %%a in ("%CONFIG_XML%")    do set CFG_TIME=%%~ta
for %%b in ("!MAKEFILE_NAME!") do set MAKEFILE_TIME=%%~tb

if "!CFG_TIME!" GTR "!MAKEFILE_TIME!" (
    echo Regenerating: configurations.xml is newer than makefile
    rd /s /q "!FLAG_PATH!"
    exit /b 1
)
exit /b 0


REM =========================================================
REM FUNCTION: PrintElapsed  %1=start_time_string  %2=label
REM Prints elapsed seconds since the captured start time.
REM =========================================================
:PrintElapsed
set "T_END=%time: =0%"
set "T_S=%~1"
set /a _S=(1!T_S:~0,2!-100)*3600+(1!T_S:~3,2!-100)*60+(1!T_S:~6,2!-100)
set /a _E=(1!T_END:~0,2!-100)*3600+(1!T_END:~3,2!-100)*60+(1!T_END:~6,2!-100)
set /a _ELAPSED=_E-_S
if !_ELAPSED! lss 0 set /a _ELAPSED+=86400
echo %CBlue%[TIME] %~2 build time: !_ELAPSED! second(s)%CEnd%
exit /b 0
