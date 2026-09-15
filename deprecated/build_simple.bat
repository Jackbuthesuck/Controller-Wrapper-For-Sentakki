@echo off
echo Building Controller to Maimai for Sentakki (Simple Build)...

REM Try to find Visual Studio installation
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    echo Found Visual Studio Installer...
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALLDIR=%%i"
    if defined VSINSTALLDIR (
        echo Found Visual Studio at: %VSINSTALLDIR%
        call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat"
    ) else (
        echo Visual Studio with C++ tools not found. Trying alternative...
    )
)

REM Try to find Windows SDK
set "WINSDK=%ProgramFiles(x86)%\Windows Kits\10\bin\x64\rc.exe"
if exist "%WINSDK%" (
    echo Found Windows SDK...
    set "WINSDKPATH=%ProgramFiles(x86)%\Windows Kits\10"
) else (
    echo Windows SDK not found in default location...
)

REM Try to compile with cl.exe if available
where cl >nul 2>&1
if %errorlevel% equ 0 (
    echo Compiling with cl.exe...
    cl /EHsc /std:c++17 main.cpp /Fe:ControllerToMaimai.exe /link XInput.lib user32.lib kernel32.lib gdi32.lib winmm.lib
    if %errorlevel% equ 0 (
        echo Build successful! Executable: ControllerToMaimai.exe
    ) else (
        echo Build failed with cl.exe
    )
) else (
    echo cl.exe not found. Trying g++...
    where g++ >nul 2>&1
    if %errorlevel% equ 0 (
        echo Compiling with g++...
        g++ -std=c++17 -O2 main.cpp -o ControllerToMaimai.exe -lXInput -luser32 -lkernel32 -lgdi32 -lwinmm
        if %errorlevel% equ 0 (
            echo Build successful! Executable: ControllerToMaimai.exe
        ) else (
            echo Build failed with g++
        )
    ) else (
        echo No suitable compiler found. Please install Visual Studio with C++ tools or MinGW-w64.
        echo.
        echo You can download Visual Studio Community (free) from:
        echo https://visualstudio.microsoft.com/downloads/
        echo.
        echo Make sure to install "Desktop development with C++" workload.
    )
)

pause
