@echo off
echo Setting up Visual Studio 2022 environment and building Touch version...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo.
echo ========================================
echo Compiling ControllerToTouch.exe with C++/WinRT...
echo ========================================
cl /EHsc /std:c++17 /await ControllerToTouch.cpp /link dinput8.lib dxguid.lib xinput.lib user32.lib gdi32.lib msimg32.lib windowsapp.lib /out:ControllerToTouch.exe
if %ERRORLEVEL% EQU 0 (
    echo.
    echo Embedding admin manifest...
    mt.exe -manifest ControllerToTouch.manifest -outputresource:ControllerToTouch.exe;1
    if %ERRORLEVEL% EQU 0 (
        echo.
        echo ========================================
        echo Build successful! Executable: ControllerToTouch.exe
        echo ========================================
        echo.
        echo This version uses UWP InputInjector API
        echo - Does NOT require physical touch hardware
        echo - Supports true multi-touch (2 independent points)
        echo - Requires Windows 10 or later
        echo - Will automatically request Administrator privileges
        echo.
    ) else (
        echo Warning: Manifest embedding failed but exe is built.
    )
) else (
    echo Build failed for ControllerToTouch!
    echo.
    echo Make sure you have:
    echo - Visual Studio 2022 with C++/WinRT support
    echo - Windows 10 SDK installed
)

pause

