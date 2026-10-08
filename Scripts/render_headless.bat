@echo off
setlocal EnableExtensions
setlocal EnableDelayedExpansion

REM ===========================================================================
REM  MetaHuman headless state render launcher
REM
REM  Usage:
REM    render_headless.bat [--build] [state.json]
REM
REM  Defaults (override by editing the variables below or passing a state path):
REM    state  : <project>\Saved\ToolOutput\exported_state.json
REM    output : <project>\Saved\ToolOutput
REM    warm-up: 32 frames (Groom/Cloth settle)
REM ===========================================================================

set "PROJECT_DIR=%~dp0.."
REM 测试环境下直接写死本地目录，发布需改实现
set "ENGINE_DIR=D:\Program Files\Epic Games\UE_5.6"

if not exist "%ENGINE_DIR%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" (
    echo [error] UnrealEditor-Cmd.exe not found under "%ENGINE_DIR%".
    echo         Edit ENGINE_DIR in this script to point at your UE 5.6 install.
    exit /b 1
)

set "META_STATE=%PROJECT_DIR%\Saved\ToolOutput\exported_state.json"
set "META_OUT=%PROJECT_DIR%\Saved\ToolOutput"
set "META_WARMUP=32"
set "DO_BUILD=0"

:parse
if "%~1"=="" goto run
if /I "%~1"=="--build" set "DO_BUILD=1" & shift & goto parse
if /I "%~1"=="-build"   set "DO_BUILD=1" & shift & goto parse
set "META_STATE=%~1"
shift
goto parse

:run
tasklist /FI "IMAGENAME eq UnrealEditor.exe" 2>NUL | findstr /I /C:"UnrealEditor.exe" >NUL

if not errorlevel 1 (
    exit /b 1
)

if "%DO_BUILD%"=="1" (
    echo [build] JobTestProjectEditor Win64 Development ...
    call "%ENGINE_DIR%\Engine\Build\BatchFiles\Build.bat" JobTestProjectEditor Win64 Development -Project="%PROJECT_DIR%\JobTestProject.uproject" -WaitMutex
    if errorlevel 1 (
        exit /b 1
    )
)

echo [render] state  = %META_STATE%
echo [render] out    = %META_OUT%
echo [render] warmup = %META_WARMUP%

"%ENGINE_DIR%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "%PROJECT_DIR%\JobTestProject.uproject" ^
  /Game/Main ^
  -ExecutePythonScript="%PROJECT_DIR%\Scripts\render_headless.py" ^
  -unattended -nosplash -RenderOffscreen -stdout -FullStdOutLogOutput

set "EXITCODE=%ERRORLEVEL%"
echo [render] UnrealEditor-Cmd.exe exited with code %EXITCODE%
exit /b %EXITCODE%
