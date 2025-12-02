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

# Do NOT set VCPKG_CMAKE_SYSTEM_NAME for native Windows/MSVC builds
# Leaving it unset tells vcpkg to use the native Windows toolchain (MSVC)
# Only set VCPKG_CMAKE_SYSTEM_NAME for cross-compilation (e.g., "MinGW" for MinGW builds)

# Release-only builds - no debug libraries
set(VCPKG_BUILD_TYPE release)
