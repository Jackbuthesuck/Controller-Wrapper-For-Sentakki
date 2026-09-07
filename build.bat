@echo off
setlocal enableextensions enabledelayedexpansion

rem Simple build script that avoids nested-parenthesis parsing issues
set "PF86=%ProgramFiles(x86)%"
if "%PF86%"=="" set "PF86=C:\Program Files (x86)"

echo Looking for vswhere...
set "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSINSTALL="
if exist "%VSWHERE%" (
    "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\vsinstall.txt" 2>nul
    if exist "%TEMP%\vsinstall.txt" (
        set /p VSINSTALL=<"%TEMP%\vsinstall.txt"
        del "%TEMP%\vsinstall.txt" >nul 2>&1
    )
)

if "%VSINSTALL%"=="" (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        set "VSINSTALL=C:\Program Files\Microsoft Visual Studio\2022\Community"
    ) else if exist "%PF86%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
        set "VSINSTALL=%PF86%\Microsoft Visual Studio\2019\Community"
    )
)

if "%VSINSTALL%"=="" (
    echo Could not find Visual Studio installation. Please run from a Developer Command Prompt or install VS with C++ workload.
    pause
    exit /b 1
)

echo Using VS at: %VSINSTALL%
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

echo Compiling...
cl /EHsc /std:c++20 /nologo /MP /c "main.cpp" "ControllerMapper.cpp" "TouchMode.cpp" "MouseMode.cpp" "KeyboardMode.cpp" "CameraMode.cpp"
if %ERRORLEVEL% NEQ 0 (
    echo Compilation failed.
    pause
    exit /b 1
)
echo Linking...
link /nologo main.obj ControllerMapper.obj TouchMode.obj MouseMode.obj KeyboardMode.obj CameraMode.obj dinput8.lib dxguid.lib xinput.lib user32.lib gdi32.lib msimg32.lib windowsapp.lib /out:ControllerInput.exe
if %ERRORLEVEL% NEQ 0 (
    echo Linking failed.
    pause
    exit /b 1
)
echo Build succeeded. Output: ControllerInput.exe
endlocal
exit /b 0
