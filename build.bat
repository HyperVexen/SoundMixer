@echo off
setlocal
cd /d "%~dp0"

echo [SoundMixer] Configuring and Building SoundMixer...

set "PATH=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd;%PATH%"

cmake -B build -G "Visual Studio 18 2026" -A x64
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b %ERRORLEVEL%
)

cmake --build build --config Release --target SoundMixer
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ============================================================
echo [SUCCESS] SoundMixer built successfully!
echo Executable: build\Release\SoundMixer.exe
echo ============================================================
pause
