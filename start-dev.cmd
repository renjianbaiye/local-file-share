@echo off
setlocal

set ROOT=%~dp0
if "%~1"=="" (
  if "%SHARE_DIR%"=="" (
    set SHARE_DIR=D:\Share
  )
) else (
  set SHARE_DIR=%~1
)

set BACKEND=%ROOT%build\Debug\LocalFileShare.exe
set BACKEND_CMD=%ROOT%.local-backend-dev.cmd
set BACKEND_URL=http://127.0.0.1:8080/api/list

if not exist "%BACKEND%" (
  echo Backend executable not found: %BACKEND%
  echo Please build the C++ project first: cmake --build build
  exit /b 1
)

if "%SHARE_DIR%"=="" (
  echo Shared directory is empty.
  exit /b 1
)

if not exist "%SHARE_DIR%" (
  echo Shared directory not found: %SHARE_DIR%
  echo Pass a directory explicitly, for example:
  echo   start-dev.cmd D:\tmp
  exit /b 1
)

echo Starting C++ backend on http://127.0.0.1:8080 ...
echo Shared directory: %SHARE_DIR%
echo Photo database: %ROOT%photos.db
> "%BACKEND_CMD%" echo @echo off
>> "%BACKEND_CMD%" echo cd /d "%ROOT%"
>> "%BACKEND_CMD%" echo "%BACKEND%" --dir "%SHARE_DIR%" --host 127.0.0.1 --port 8080 --no-open --no-auth --dev --photo-db "%ROOT%photos.db"
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
echo Album API: http://127.0.0.1:8080/api/photos/timeline
echo Vite UI:   http://127.0.0.1:5173

cd /d "%ROOT%frontend"
npm.cmd run dev -- --port 5173 --strictPort
