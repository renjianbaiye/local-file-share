@echo off
setlocal

set ROOT=%~dp0
if "%~1"=="" (
  if "%SHARE_DIR%"=="" (
    set SHARE_DIR=C:\back\test
  )
) else (
  set SHARE_DIR=%~1
)

set BACKEND=%ROOT%build-vs\Debug\LocalFileShare.exe
set BACKEND_CMD=%ROOT%.local-backend-dev.cmd
set BACKEND_HOST=0.0.0.0
set FRONTEND_HOST=0.0.0.0
set BACKEND_URL=http://127.0.0.1:8080/api/list
set LAN_IP=127.0.0.1
set NODE_EXE=node.exe
set NPM_CLI=
set AUTH_TOKEN=
if "%ALBUM_CV_ROOT%"=="" set ALBUM_CV_ROOT=C:\Code\PythonCode\album-python-cv-
if "%ALBUM_CV_PYTHON%"=="" set ALBUM_CV_PYTHON=C:\Users\18361\.conda\envs\album-cv\python.exe
if "%ALBUM_CV_DEVICE%"=="" set ALBUM_CV_DEVICE=cuda
if "%ONNXRUNTIME_ROOT%"=="" set ONNXRUNTIME_ROOT=%ROOT%third_party\onnxruntime\onnxruntime-win-x64-gpu-1.23.2
if "%CUDA_BIN%"=="" set CUDA_BIN=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8\bin
if "%CUDNN_BIN%"=="" set CUDNN_BIN=C:\Users\18361\.conda\envs\album-cv\Lib\site-packages\torch\lib

if exist "C:\Program Files\nodejs\node.exe" set "PATH=C:\Program Files\nodejs;%PATH%"
if exist "%ONNXRUNTIME_ROOT%\lib\onnxruntime.dll" set "PATH=%ONNXRUNTIME_ROOT%\lib;%PATH%"
if exist "%CUDA_BIN%\cudart64_12.dll" set "PATH=%CUDA_BIN%;%PATH%"
if exist "%CUDNN_BIN%\cudnn64_9.dll" set "PATH=%CUDNN_BIN%;%PATH%"

for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "-join ((1..32) | ForEach-Object { '{0:x}' -f (Get-Random -Minimum 0 -Maximum 16) })"`) do set AUTH_TOKEN=%%i

for /f "tokens=2 delims=:" %%i in ('ipconfig ^| findstr /R /C:"IPv4.*192\." /C:"IPv4.*10\." /C:"IPv4.*172\."') do (
  set LAN_IP=%%i
  goto lan_ip_found
)
:lan_ip_found
set LAN_IP=%LAN_IP: =%

where node.exe >nul 2>nul
if errorlevel 1 (
  if exist "C:\Program Files\nodejs\node.exe" (
    set NODE_EXE=C:\Program Files\nodejs\node.exe
  )
)

if exist "C:\Program Files\nodejs\node_modules\npm\bin\npm-cli.js" (
  set NPM_CLI=C:\Program Files\nodejs\node_modules\npm\bin\npm-cli.js
) else (
  where npm.cmd >nul 2>nul
  if not errorlevel 1 set NPM_CLI=npm.cmd
)

if not exist "%BACKEND%" (
  echo Backend executable not found: %BACKEND%
  echo Please build the C++ project first: cmake --build build-vs --config Debug
  exit /b 1
)

if "%NPM_CLI%"=="" (
  echo npm was not found. Please install Node.js LTS, then reopen this terminal.
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

echo Starting C++ backend on http://%LAN_IP%:8080 ...
echo Shared directory: %SHARE_DIR%
echo Photo database: %ROOT%photos.db
echo Album CV root: %ALBUM_CV_ROOT%
echo Album CV python: %ALBUM_CV_PYTHON%
echo ONNX Runtime: %ONNXRUNTIME_ROOT%
> "%BACKEND_CMD%" echo @echo off
>> "%BACKEND_CMD%" echo cd /d "%ROOT%"
>> "%BACKEND_CMD%" echo set "PATH=%ONNXRUNTIME_ROOT%\lib;%CUDA_BIN%;%CUDNN_BIN%;%%PATH%%"
>> "%BACKEND_CMD%" echo "%BACKEND%" --dir "%SHARE_DIR%" --host %BACKEND_HOST% --port 8080 --no-open --dev --token %AUTH_TOKEN% --photo-db "%ROOT%photos.db" --album-cv-root "%ALBUM_CV_ROOT%" --album-cv-python "%ALBUM_CV_PYTHON%" --album-cv-device %ALBUM_CV_DEVICE%
start "LocalFileShare Backend :8080" cmd /k "%BACKEND_CMD%"

echo Waiting for backend...
for /l %%i in (1,1,30) do (
  powershell -NoProfile -Command "try { $r = Invoke-WebRequest -Uri '%BACKEND_URL%' -UseBasicParsing -TimeoutSec 1; if ($r.StatusCode -eq 200) { exit 0 } } catch { if ($_.Exception.Response -and ([int]$_.Exception.Response.StatusCode -eq 401 -or [int]$_.Exception.Response.StatusCode -eq 403)) { exit 0 }; exit 1 }"
  if not errorlevel 1 goto backend_ready
  timeout /t 1 /nobreak >nul
)

echo Backend did not start on 127.0.0.1:8080.
echo Check the "LocalFileShare Backend :8080" window for the C++ error message.
exit /b 1

:backend_ready
echo Backend is ready.
echo Album API: http://%LAN_IP%:8080/api/photos/timeline
echo Vite UI:   http://%LAN_IP%:5173
echo Auth URL:  http://%LAN_IP%:5173?token=%AUTH_TOKEN%

cd /d "%ROOT%frontend"
set "VITE_LFS_TOKEN=%AUTH_TOKEN%"
if /i "%NPM_CLI%"=="npm.cmd" (
  npm.cmd run dev -- --port 5173 --strictPort
) else (
  "%NODE_EXE%" "%NPM_CLI%" run dev -- --port 5173 --strictPort
)
