cmake_minimum_required(VERSION 3.16)
project(knight_moves_5 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(az src/az.cpp)

if(MSVC)
  target_compile_options(az PRIVATE /O2 /fp:fast /arch:AVX2)
  target_compile_definitions(az PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(az PRIVATE -O3 -march=native -ffast-math)
endif()
