@echo off
echo Setting up Visual Studio 2022 environment and building...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
echo Compiling Controller to Maimai (Final Version)...
cl /EHsc simple_controller.cpp /link dinput8.lib user32.lib /out:ControllerToMaimai.exe
if %ERRORLEVEL% EQU 0 (
    echo Build successful! Executable: ControllerToMaimai.exe
    echo You can now run ControllerToMaimai.exe
) else (
    echo Build failed!
)
pause

