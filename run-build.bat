@echo off
cd /d "%~dp0"

rem Double-click to rebuild the index. All paths live in build_fileindex.py CONFIG.

where py >nul 2>nul
if errorlevel 1 (
  python build_fileindex.py %*
) else (
  py -3 build_fileindex.py %*
)

echo.
pause
