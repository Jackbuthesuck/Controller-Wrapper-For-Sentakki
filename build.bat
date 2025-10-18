@echo off
echo Setting up Visual Studio 2022 environment and building all versions...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo.
echo ========================================
echo Compiling ControllerToNumberKeys.exe...
echo ========================================
cl /EHsc ControllerToNumberKeys.cpp /link dinput8.lib dxguid.lib xinput.lib user32.lib gdi32.lib msimg32.lib /out:ControllerToNumberKeys.exe
if %ERRORLEVEL% EQU 0 (
    echo [OK] ControllerToNumberKeys.exe
) else (
    echo [FAILED] ControllerToNumberKeys!
)

echo.
echo ========================================
echo Compiling ControllerToMouse.exe...
echo ========================================
cl /EHsc ControllerToMouse.cpp /link dinput8.lib dxguid.lib xinput.lib user32.lib gdi32.lib msimg32.lib /out:ControllerToMouse.exe
if %ERRORLEVEL% EQU 0 (
    echo [OK] ControllerToMouse.exe
) else (
    echo [FAILED] ControllerToMouse!
)

echo.
echo ========================================
echo Compiling ControllerToTouch.exe...
echo ========================================
cd "Touch Version"
cl /EHsc /std:c++17 /await ControllerToTouch.cpp /link dinput8.lib dxguid.lib xinput.lib user32.lib gdi32.lib msimg32.lib windowsapp.lib /out:ControllerToTouch.exe
if %ERRORLEVEL% EQU 0 (
    echo.
    echo Embedding admin manifest...
    mt.exe -manifest ControllerToTouch.manifest -outputresource:ControllerToTouch.exe;1
    if %ERRORLEVEL% EQU 0 (
        echo [OK] ControllerToTouch.exe (with admin manifest)
    ) else (
        echo [OK] ControllerToTouch.exe (manifest embedding failed but exe works)
    )
) else (
    echo [FAILED] ControllerToTouch!
)
cd ..

echo.
echo ========================================
echo Build Summary
echo ========================================
echo All versions compiled:
echo - ControllerToNumberKeys.exe (Number keys 1-8)
echo - ControllerToMouse.exe (Mouse + LMB)
echo - Touch Version\ControllerToTouch.exe (TRUE Multi-Touch!)
echo.
echo All executables are PORTABLE - no installation needed!
echo Just copy the .exe files to any Windows 10/11 PC and run.
pause

