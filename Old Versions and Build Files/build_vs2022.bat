@echo off
echo Setting up Visual Studio 2022 environment and building...

REM Set up Visual Studio 2022 environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM Now compile the program
echo Compiling Controller to Maimai...
cl /EHsc /std:c++17 main.cpp /Fe:ControllerToMaimai.exe /link XInput.lib user32.lib kernel32.lib gdi32.lib winmm.lib

if %errorlevel% equ 0 (
    echo.
    echo Build successful! Executable: ControllerToMaimai.exe
    echo You can now run ControllerToMaimai.exe
) else (
    echo.
    echo Build failed. Check the error messages above.
)

pause
