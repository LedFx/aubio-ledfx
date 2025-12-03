# Custom triplet for x64 macOS (Intel) with release-only builds
# Based on community x64-osx triplet with VCPKG_BUILD_TYPE added

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Release-only builds - cuts build time in half
set(VCPKG_BUILD_TYPE release)

# Use the standard OSX platform (inherits Apple framework settings)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
