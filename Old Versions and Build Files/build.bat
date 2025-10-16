@echo off
echo Building Controller to Maimai Mapper...

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
cmake .. -G "Visual Studio 17 2022" -A x64

REM Build the project
cmake --build . --config Release

REM Copy executable to parent directory
copy Release\controller_to_maimai.exe ..\

echo.
echo Build complete! Run controller_to_maimai.exe
echo Press F7 to exit the program
pause
