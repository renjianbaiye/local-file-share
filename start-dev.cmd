@echo off
setlocal

set ROOT=%~dp0
set SHARE_DIR=D:\Share
set BACKEND=%ROOT%build\Debug\LocalFileShare.exe
set BACKEND_CMD=%ROOT%.local-backend-dev.cmd
set BACKEND_URL=http://127.0.0.1:8080/api/list

if not exist "%BACKEND%" (
  echo Backend executable not found: %BACKEND%
  echo Please build the C++ project first: cmake --build build
  exit /b 1
)

echo Starting C++ backend on http://127.0.0.1:8080 ...
> "%BACKEND_CMD%" echo @echo off
>> "%BACKEND_CMD%" echo cd /d "%ROOT%"
>> "%BACKEND_CMD%" echo "%BACKEND%" --dir "%SHARE_DIR%" --host 127.0.0.1 --port 8080 --no-open
start "LocalFileShare Backend :8080" cmd /k "%BACKEND_CMD%"

echo Waiting for backend...
for /l %%i in (1,1,30) do (
  powershell -NoProfile -Command "try { $r = Invoke-WebRequest -Uri '%BACKEND_URL%' -UseBasicParsing -TimeoutSec 1; if ($r.StatusCode -eq 200) { exit 0 } } catch { exit 1 }"
  if not errorlevel 1 goto backend_ready
  timeout /t 1 /nobreak >nul
)

echo Backend did not start on 127.0.0.1:8080.
echo Check the "LocalFileShare Backend :8080" window for the C++ error message.
exit /b 1

:backend_ready
echo Backend is ready.

cd /d "%ROOT%frontend"
npm.cmd run dev -- --port 5173 --strictPort
