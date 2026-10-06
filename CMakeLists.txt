# oratan - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(oratan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(ORATAN_SOURCES
    src/main.cpp
    src/stl_compat.cpp
    src/debug_tools.cpp
)

if(WIN32)
    add_executable(oratan WIN32 ${ORATAN_SOURCES})
else()
    add_executable(oratan ${ORATAN_SOURCES})
endif()

rexglue_setup_target(oratan GPU_PLUGINS xenos)
