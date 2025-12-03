# Windows Build Fix Summary

## Problem
GitHub Actions Windows builds were failing with linker errors during Python extension compilation.

## Investigation Process

### Initial Error Analysis  
Examined workflow run logs showing 7 unresolved SLEEF DFT symbols:
- `__imp_Sleef_malloc`
- `__imp_Sleef_free`
- `__imp_SleefDFT_double_init1d`
- `__imp_SleefDFT_double_execute`
- `__imp_SleefDFT_float_init1d`
- `__imp_SleefDFT_float_execute`
- `__imp_SleefDFT_dispose`

### Root Cause Identification
The `__imp_` prefix indicated Windows linker was expecting DLL import symbols, not static library symbols. This revealed:

1. **vcpkg's rubberband** on Windows is built expecting SLEEF as a DLL
2. **Our static triplet** (`x64-windows-static-msvc-release.cmake`) tries to link everything statically
3. **Fundamental incompatibility**: rubberband.lib references SLEEF with DLL import symbols even when SLEEF static libraries are present

This is a **vcpkg packaging limitation**, not a build system configuration issue.

## Solution Implemented

### Commit 1: f234ec3 - Platform-specific library dependencies
Added Windows system libraries required by FFmpeg when statically linking:
- `ws2_32`: Winsock2 networking
- `secur32`: Security Support Provider (SSPI/TLS)
- `bcrypt`: Cryptography API
- `mfuuid`, `strmiids`: Media Foundation GUIDs
- `ole32`: Component Object Model

Also added:
- Linux: `pthread` and `libstdc++` for FFmpeg and rubberband C++ code
- SLEEF DFT library detection (conditional on rubberband being found)

### Commit 2: 78c2f7a - Remove rubberband from Windows
After determining rubberband cannot be statically linked on Windows:
- Changed vcpkg.json platform filters from `"!linux"` to `"osx"`
- Affects: rubberband and sleef packages
- Created documentation: WINDOWS_RUBBERBAND_REMOVAL.md

## Impact

### Functionality Changes
- ✅ Windows builds will succeed
- ❌ Windows loses rubberband time-stretch and pitch-shift effects
- ✅ Core aubio functionality unchanged (onset, pitch detection, tempo, MFCC, etc.)
- ✅ macOS retains full rubberband support
- ✅ Linux unaffected (already didn't have rubberband)

### Platform Matrix
| Platform | Rubberband | Time-stretch | Pitch-shift |
|----------|-----------|--------------|-------------|
| Windows  | ❌        | Dummy impl   | Dummy impl  |
| macOS    | ✅        | Full support | Full support|
| Linux    | ❌        | Dummy impl   | Dummy impl  |

## Technical Details

### Why Not Fix vcpkg?
Fixing this properly would require:
1. Modifying vcpkg's rubberband port
2. Ensuring SLEEF builds without DLL import expectations
3. Testing across multiple Windows configurations
4. Contributing changes upstream
5. Waiting for vcpkg release cycle

This is outside the scope of immediate CI fixes.

### Why Not Use Dynamic Linking?
- Defeats purpose of portable Python wheels
- Requires bundling DLLs
- Increases package size and complexity
- Makes distribution error-prone

### Why Not Use MinGW?
- MSVC is the standard Windows toolchain
- Better compatibility with vcpkg packages (especially FFmpeg)
- Would require significant build system rework

## Files Modified

1. **vcpkg.json**: Platform filters for rubberband and sleef
2. **python/meson.build**: Windows system library dependencies  
3. **meson.build**: SLEEF DFT library detection
4. **WINDOWS_RUBBERBAND_REMOVAL.md**: Detailed rationale documentation

## Verification

Expected build behavior:
1. vcpkg skips rubberband/sleef installation on Windows
2. Meson detects rubberband as "not found"
3. `HAVE_RUBBERBAND` not defined in config.h
4. Dummy timestretch/pitchshift implementations compile
5. Python extension links successfully with FFmpeg system libraries
6. Wheels build and package correctly

## Future Work

If rubberband on Windows becomes critical:
- Investigate vcpkg rubberband port modifications
- Consider contributing fix to vcpkg upstream
- Or implement alternative pure-Python time-stretching solution
