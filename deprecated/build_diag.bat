@echo off
setlocal enableextensions enabledelayedexpansion
set SCRIPT_DIR=%~dp0
echo Building ControllerInput.exe (working dir: "%SCRIPT_DIR%")

rem Cache ProgramFiles(x86) into a simple variable to avoid parentheses parsing issues
set "PF86=%ProgramFiles(x86)%"
if "%PF86%"=="" (
    rem Fallback default when ProgramFiles(x86) is not set
    set "PF86=C:\Program Files (x86)"
)

rem Kill any running instance
taskkill /F /IM ControllerInput.exe >nul 2>&1

rem Diagnostic mode: print environment info and exit
if /I "%1"=="diag" (
    echo.
    echo ===== Diagnostic Information =====
    echo Current PATH:
    echo !PATH!
    echo.
    echo Checking for cl.exe...
    where cl.exe 2>nul || echo cl.exe not found in PATH
    echo.
    echo ProgramFiles^(x86^) resolved to: !PF86!
    set "VSWHERE=!PF86!\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        echo vswhere found at !VSWHERE!
        "!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath || echo vswhere returned no result
    ) else (
        echo vswhere not found at !VSWHERE!
    )
    echo.
    echo ===== End Diagnostic Information =====
    pause
    exit /b 0
)

rem If cl.exe is already in PATH, assume environment is ready
where cl.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Found cl.exe in PATH. Using existing developer environment.
) else (
    echo cl.exe not found in PATH — attempting to locate Visual Studio using vswhere...
    set "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSINSTALL=%%I
    ) else (
        set VSINSTALL=
    )

    if defined VSINSTALL (
        set "VCVARS=!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat"
        if exist "!VCVARS!" (
            echo Calling "!VCVARS!" ...
            call "!VCVARS!" >nul 2>&1
        ) else (
            echo Found Visual Studio install at !VSINSTALL! but vcvars64.bat missing.
        )
    ) else (
        rem Try common VS paths (2022, 2019)
        if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
            call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
        ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
            call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
        ) else (
            echo.
            echo [ERROR] Could not locate Visual Studio Developer Command Prompt or vcvars64.bat.
            echo Install Visual Studio (with C++), or run this script from a Developer Command Prompt.
            pause
            exit /b 1
        )
    )
    if %ERRORLEVEL% NEQ 0 (
        echo.
        echo [ERROR] Failed to initialize Visual Studio environment
        pause
        exit /b 1
    )
)

echo.
echo Compiling source files...
pushd %SCRIPT_DIR%
cl /EHsc /std:c++17 /await /nologo /MP /c "%SCRIPT_DIR%main.cpp" "%SCRIPT_DIR%ControllerMapper.cpp" "%SCRIPT_DIR%TouchMode.cpp" "%SCRIPT_DIR%MouseMode.cpp" "%SCRIPT_DIR%KeyboardMode.cpp" 2>&1
set COMPILE_ERROR=%ERRORLEVEL%
if %COMPILE_ERROR% NEQ 0 (
    echo.
    echo ========================================
    echo [FAILED] Compilation errors detected
    echo ========================================
    echo.
    echo Check the error messages above for details.
    popd
    pause
    exit /b 1
)

echo.
echo Linking...
link /nologo main.obj ControllerMapper.obj TouchMode.obj MouseMode.obj KeyboardMode.obj dinput8.lib dxguid.lib xinput.lib user32.lib gdi32.lib msimg32.lib windowsapp.lib /out:ControllerInput.exe 2>&1
set LINK_ERROR=%ERRORLEVEL%
if %LINK_ERROR% EQU 0 (
    del main.obj ControllerMapper.obj TouchMode.obj MouseMode.obj KeyboardMode.obj >nul 2>&1
    if exist ControllerInput.manifest (
        mt.exe -manifest ControllerInput.manifest -outputresource:ControllerInput.exe;1 >nul 2>&1
    )
    echo.
    echo ========================================
    echo [SUCCESS] ControllerInput.exe built!
    echo ========================================
    popd
) else (
    echo.
    echo ========================================
    echo [FAILED] Linking errors detected
    echo ========================================
    echo.
    echo Check the error messages above for details.
    popd
    pause
    exit /b 1
)

endlocal
