@echo off
echo Setting up Visual Studio 2022 environment and building...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo.
echo ========================================
echo Compiling ControllerToNumberKeys.exe...
echo ========================================
cl /EHsc ControllerToNumberKeys.cpp /link dinput8.lib user32.lib gdi32.lib msimg32.lib /out:ControllerToNumberKeys.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful! Executable: ControllerToNumberKeys.exe
) else (
    echo Build failed for ControllerToNumberKeys!
)

echo.
echo ========================================
echo Compiling ControllerToMouse.exe...
echo ========================================
cl /EHsc ControllerToMouse.cpp /link dinput8.lib user32.lib gdi32.lib msimg32.lib /out:ControllerToMouse.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful! Executable: ControllerToMouse.exe
) else (
    echo Build failed for ControllerToMouse!
)

echo.
echo ========================================
echo Build Summary
echo ========================================
echo Both versions have been compiled:
echo - ControllerToNumberKeys.exe (Number keys only)
echo - ControllerToMouse.exe (Mouse + LMB)
pause

