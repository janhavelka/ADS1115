@echo off
setlocal

rem PlatformIO prints Unicode dependency trees; force deterministic UTF-8
rem output even when the invoking Windows console still uses a legacy codepage.
set "PYTHONUTF8=1"

set "PIO_EXE=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"
rem Keep the project tool/package cache isolated from other repositories. Use a
rem deliberately short root because pioarduino's bundled ESP Matter headers can
rem otherwise exceed the legacy Windows MAX_PATH limit while unpacking.
if not defined PLATFORMIO_CORE_DIR set "PLATFORMIO_CORE_DIR=%SystemDrive%\.pio-ads1115"

if not exist "%PIO_EXE%" (
    >&2 echo VS Code-managed PlatformIO was not found at: "%PIO_EXE%". Stop and report the missing installation; do not install another PlatformIO Core.
    exit /b 1
)

"%PIO_EXE%" %*
exit /b %ERRORLEVEL%
