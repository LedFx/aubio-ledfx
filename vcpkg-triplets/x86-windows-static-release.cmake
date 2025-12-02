# Custom triplet for x86 Windows with static linking and release-only builds
# Creates fully portable Python extensions with no external DLL dependencies

set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# Release-only builds - no debug libraries
set(VCPKG_BUILD_TYPE release)
