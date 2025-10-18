@echo off
echo Building ControllerInput.exe...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul

echo.
cl /EHsc /std:c++17 /await /nologo ControllerInput.cpp /link dinput8.lib dxguid.lib xinput.lib user32.lib gdi32.lib msimg32.lib windowsapp.lib /out:ControllerInput.exe
if %ERRORLEVEL% EQU 0 (
    mt.exe -manifest ControllerInput.manifest -outputresource:ControllerInput.exe;1 >nul 2>&1
    echo.
    echo ========================================
    echo [SUCCESS] ControllerInput.exe built!
    echo ========================================
    echo.
    echo Features:
    echo   [1] Touch Mode - Multi-touch for Sentakki
    echo   [2] Mouse Mode - Cursor + click control
    echo   [3] Keyboard Mode - Number keys 1-8
    echo.
    echo All modes include overlay + debug UI
    echo Works on any Windows 10/11 PC - no special hardware!
    echo PORTABLE - just copy and run!
) else (
    echo.
    echo [FAILED] Build failed - see errors above
    pause
    exit /b 1
)
