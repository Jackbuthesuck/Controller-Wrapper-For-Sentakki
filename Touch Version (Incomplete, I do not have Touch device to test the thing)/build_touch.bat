@echo off
echo Setting up Visual Studio 2022 environment and building Touch version...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo.
echo ========================================
echo Compiling ControllerToTouch.exe...
echo ========================================
cl /EHsc ControllerToTouch.cpp /link dinput8.lib dxguid.lib xinput.lib user32.lib gdi32.lib msimg32.lib /MANIFEST:EMBED /MANIFESTINPUT:ControllerToTouch.manifest /out:ControllerToTouch.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful! Executable: ControllerToTouch.exe
    echo.
    echo The program will now automatically request Administrator privileges.
    echo.
    echo IMPORTANT: Touch injection requires Windows 8 or later.
    echo If you get Error 87, you need a virtual touch screen driver.
    echo Try: USBMMIDD_v2 or TeamViewer's virtual touch driver.
) else (
    echo Build failed for ControllerToTouch!
)

pause

