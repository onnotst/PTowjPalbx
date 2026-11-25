@echo off
chcp 65001 >nul
cd /d "%~dp0"
where node >nul 2>nul
if %errorlevel%==0 (
  echo 使用 Node 启动本地服务（COOP/COEP 已开启）...
  node server.js 8787
) else (
  echo 未检测到 Node，改用 PowerShell 启动本地服务...
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0server.ps1"
)
pause
