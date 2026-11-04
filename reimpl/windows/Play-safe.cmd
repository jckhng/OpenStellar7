@echo off
setlocal DisableDelayedExpansion
set "SDL_RENDER_DRIVER=software"
set "SDL_FRAMEBUFFER_ACCELERATION=0"
set "STELLAR7_SAFE_MODE=1"
set "STELLAR7_HOST_DIAGNOSTICS=1"
set "STELLAR7_LOG_DIR=%LOCALAPPDATA%\Stellar7Enhanced"
if not exist "%STELLAR7_LOG_DIR%" mkdir "%STELLAR7_LOG_DIR%"
if not exist "%STELLAR7_LOG_DIR%" exit /b 1
set "STELLAR7_LOG_FILE=%STELLAR7_LOG_DIR%\host-safe-%RANDOM%-%RANDOM%.log"
if exist "%STELLAR7_LOG_FILE%" exit /b 1
echo Diagnostic log: "%STELLAR7_LOG_FILE%"
"%~dp0stellar7.exe" --play %* >"%STELLAR7_LOG_FILE%" 2>&1
set "STELLAR7_EXIT=%ERRORLEVEL%"
if not "%STELLAR7_EXIT%"=="0" type "%STELLAR7_LOG_FILE%"
exit /b %STELLAR7_EXIT%
