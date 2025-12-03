# Custom triplet for ARM64 macOS (Apple Silicon) with release-only builds
# Based on community arm64-osx triplet with VCPKG_BUILD_TYPE added

set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Release-only builds - cuts build time in half
set(VCPKG_BUILD_TYPE release)

# Use the standard OSX platform (inherits Apple framework settings)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
