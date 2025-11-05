@echo off
echo Building ControllerInput.exe...
taskkill /F /IM ControllerInput.exe >nul 2>&1
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Failed to initialize Visual Studio environment
    pause
    exit /b 1
)

echo.
echo Compiling source files...
cl /EHsc /std:c++17 /await /nologo /MP /c main.cpp ControllerMapper.cpp TouchMode.cpp MouseMode.cpp KeyboardMode.cpp 2>&1
set COMPILE_ERROR=%ERRORLEVEL%
if %COMPILE_ERROR% NEQ 0 (
    echo.
    echo ========================================
    echo [FAILED] Compilation errors detected
    echo ========================================
    echo.
    echo Check the error messages above for details.
    pause
    exit /b 1
)

echo.
echo Linking...
link /nologo main.obj ControllerMapper.obj TouchMode.obj MouseMode.obj KeyboardMode.obj dinput8.lib dxguid.lib xinput.lib user32.lib gdi32.lib msimg32.lib windowsapp.lib /out:ControllerInput.exe 2>&1
set LINK_ERROR=%ERRORLEVEL%
if %LINK_ERROR% EQU 0 (
    del main.obj ControllerMapper.obj TouchMode.obj MouseMode.obj KeyboardMode.obj >nul 2>&1
    mt.exe -manifest ControllerInput.manifest -outputresource:ControllerInput.exe;1 >nul 2>&1
    echo.
    echo ========================================
    echo [SUCCESS] ControllerInput.exe built!
    echo ========================================
) else (
    echo.
    echo ========================================
    echo [FAILED] Linking errors detected
    echo ========================================
    echo.
    echo Check the error messages above for details.
    pause
    exit /b 1
)
