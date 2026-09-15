@echo off
echo Building Controller to Maimai for Sentakki...

REM Try to find and use Visual Studio Developer Command Prompt
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    echo Found Visual Studio Installer...
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VSINSTALLDIR=%%i"
    )
    if defined VSINSTALLDIR (
        echo Found Visual Studio at: %VSINSTALLDIR%
        call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat"
        if %errorlevel% equ 0 (
            echo Environment set up successfully
            cl /EHsc /std:c++17 main.cpp /Fe:ControllerToMaimai.exe /link XInput.lib user32.lib kernel32.lib gdi32.lib winmm.lib
            if %errorlevel% equ 0 (
                echo Build successful! Executable: ControllerToMaimai.exe
            ) else (
                echo Build failed
            )
        ) else (
            echo Failed to set up Visual Studio environment
        )
    ) else (
        echo Visual Studio with C++ tools not found
    )
) else (
    echo Visual Studio Installer not found
)

echo.
echo If build failed, you may need to install Visual Studio Community with C++ tools
echo Download from: https://visualstudio.microsoft.com/downloads/
echo Make sure to install "Desktop development with C++" workload
pause

