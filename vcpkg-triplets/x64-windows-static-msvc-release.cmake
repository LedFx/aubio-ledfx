# Custom triplet for x64 Windows with MSVC and static linking
# Uses MSVC compiler (not MinGW) for better compatibility with vcpkg packages like ffmpeg
# Creates fully portable Python extensions with no external DLL dependencies

set(VCPKG_TARGET_ARCHITECTURE x64)
# CRITICAL: Use static CRT (/MT) when linking with MSVC-built static libraries
# See https://www.ffmpeg.org/platform.html#Linking-to-FFmpeg-with-Microsoft-Visual-C_002b_002b
# "If you plan to link with MSVC-built static libraries, you will need to make sure
# you have Runtime Library set to Multi-threaded (/MT) in your project's settings."
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# Use Windows (MSVC) - not MinGW
# This ensures vcpkg uses MSVC compiler which has better support for ffmpeg and rubberband
set(VCPKG_CMAKE_SYSTEM_NAME Windows)

# Release-only builds - no debug libraries
set(VCPKG_BUILD_TYPE release)
