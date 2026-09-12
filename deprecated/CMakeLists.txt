cmake_minimum_required(VERSION 3.16)
project(ControllerToMaimai)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find required packages
find_package(PkgConfig REQUIRED)

# Add executable
add_executable(ControllerToMaimai main.cpp)

# Link libraries
target_link_libraries(ControllerToMaimai 
    XInput
    user32
    kernel32
    gdi32
    winmm
)

# Set Windows-specific properties
if(WIN32)
    set_target_properties(ControllerToMaimai PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(ControllerToMaimai PRIVATE /W4)
else()
    target_compile_options(ControllerToMaimai PRIVATE -Wall -Wextra -pedantic)
endif()
