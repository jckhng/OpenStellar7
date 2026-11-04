@echo off
setlocal DisableDelayedExpansion
"%~dp0stellar7.exe" --play %*
exit /b %ERRORLEVEL%
