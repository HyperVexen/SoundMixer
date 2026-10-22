@echo off
cd /d "%~dp0"
if exist "build\Release\SoundMixer.exe" (
    start "" "build\Release\SoundMixer.exe"
) else (
    echo [ERROR] SoundMixer.exe not found in build\Release.
    echo Please run build.bat first.
    pause
)
