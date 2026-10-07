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
    src/branding.cpp
    src/game_files.cpp
    src/stl_compat.cpp
    src/debug_tools.cpp
    src/frame_stats.cpp
    src/options_menu.cpp
    src/settings.cpp
    src/dual_monitor.cpp
    src/relaunch.cpp
    src/local_versus.cpp
    src/select_pages.cpp
)

if(WIN32)
    # Version info resource. The toolchain is clang/lld, so prefer LLVM's
    # resource compiler (MSVC's rc.exe may not be on PATH).
    if(NOT CMAKE_RC_COMPILER)
        set(CMAKE_RC_COMPILER "llvm-rc")
    endif()
    enable_language(RC)
    add_executable(oratan WIN32 ${ORATAN_SOURCES} src/oratan.rc)
else()
    add_executable(oratan ${ORATAN_SOURCES})
endif()

rexglue_setup_target(oratan GPU_PLUGINS xenos)

# Window title without the SDK build tag (see src/sdk_version_override.h).
set_source_files_properties("${REXGLUE_SHARE_DIR}/rex_app.cpp"
    TARGET_DIRECTORY oratan
    PROPERTIES COMPILE_OPTIONS
        "-include;${CMAKE_CURRENT_SOURCE_DIR}/src/sdk_version_override.h")
