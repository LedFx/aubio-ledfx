# Custom triplet for x64 Windows with MinGW toolchain and static linking
# This triplet is compatible with cibuildwheel which uses MinGW on Windows
# Creates fully portable Python extensions with no external DLL dependencies

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# MinGW-specific configuration
set(VCPKG_CMAKE_SYSTEM_NAME MinGW)
set(VCPKG_ENV_PASSTHROUGH PATH)

# Release-only builds - no debug libraries
set(VCPKG_BUILD_TYPE release)
