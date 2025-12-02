# Overlay triplet for x64-windows to enforce release-only builds with static linking
# This overrides the default vcpkg x64-windows triplet used for build tools
# Prevents debug builds AND dynamic linking issues with MSVC/MinGW mismatch

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Release-only builds - cuts build time in half for helper packages
set(VCPKG_BUILD_TYPE release)
