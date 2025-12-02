# Custom triplet for x64 Windows with MSVC and static linking
# Uses MSVC compiler (not MinGW) for better compatibility with vcpkg packages like ffmpeg
# Creates fully portable Python extensions with no external DLL dependencies

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Use Windows (MSVC) - not MinGW
# This ensures vcpkg uses MSVC compiler which has better support for ffmpeg and rubberband
set(VCPKG_CMAKE_SYSTEM_NAME Windows)

# Release-only builds - no debug libraries
set(VCPKG_BUILD_TYPE release)
