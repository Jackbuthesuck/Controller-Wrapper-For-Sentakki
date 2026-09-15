@echo off
echo Setting up Visual Studio 2022 environment and building...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
echo Compiling Simple Controller with Clockwise rotation...
cl /EHsc simple_controller.cpp /link dinput8.lib user32.lib /out:SimpleController_Clockwise.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful! Executable: SimpleController_Clockwise.exe
    echo You can now run SimpleController_Clockwise.exe
) else (
    echo Build failed!
)
pause
