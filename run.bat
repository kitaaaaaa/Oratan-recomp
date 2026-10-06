@echo off
rem Run the release build against the game files in assets\.
rem Extra arguments are passed through (e.g. --log_level=debug).
setlocal
set "ROOT=%~dp0"
set "EXE=%ROOT%out\build\win-amd64-release\oratan.exe"
if not exist "%EXE%" (
  echo oratan.exe not found. Run build.bat first.
  exit /b 1
)
if not exist "%ROOT%logs" mkdir "%ROOT%logs"
"%EXE%" --game_data_root="%ROOT%assets" --log_file="%ROOT%logs\oratan.log" %*
