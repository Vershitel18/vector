cmake_minimum_required(VERSION 3.24...4.2)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ConfigureCompiler)
include(CompilerWarnings)

project(vector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

ct_configure_compiler()
include(Dependencies)

add_subdirectory(src)
add_subdirectory(test)
