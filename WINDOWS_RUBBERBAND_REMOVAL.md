# Windows Build Fix: Rubberband Removal

## Problem
Windows builds were failing with 7 unresolved external symbols when linking the Python extension:

```
rubberband.lib(src_common_FFT.cpp.obj) : error LNK2019: unresolved external symbol __imp_Sleef_malloc
rubberband.lib(src_common_FFT.cpp.obj) : error LNK2019: unresolved external symbol __imp_Sleef_free  
rubberband.lib(src_common_FFT.cpp.obj) : error LNK2019: unresolved external symbol __imp_SleefDFT_double_init1d
rubberband.lib(src_common_FFT.cpp.obj) : error LNK2019: unresolved external symbol __imp_SleefDFT_double_execute
rubberband.lib(src_common_FFT.cpp.obj) : error LNK2019: unresolved external symbol __imp_SleefDFT_float_init1d
rubberband.lib(src_common_FFT.cpp.obj) : error LNK2019: unresolved external symbol __imp_SleefDFT_float_execute
rubberband.lib(src_common_FFT.cpp.obj) : error LNK2019: unresolved external symbol __imp_SleefDFT_dispose
```

## Root Cause Analysis

The `__imp_` prefix on these symbols indicates that rubberband.lib was expecting SLEEF to be provided as a **DLL import library**, not a static library. This is a fundamental incompatibility issue:

1. **vcpkg's Windows rubberband package** builds rubberband with dynamic SLEEF expectations
2. **Our custom triplet** (`x64-windows-static-msvc-release.cmake`) specifies full static linking
3. Even when SLEEF static libraries are present, rubberband.lib references them with `__imp_` prefixes (DLL import symbols)

This is a **vcpkg packaging limitation** - rubberband on Windows does not support proper static linking with static SLEEF libraries when using the MSVC toolchain.

## Solution

Removed rubberband and SLEEF from Windows builds:

### Changes to `vcpkg.json`
```diff
- "platform": "!linux"  # Was: Windows AND macOS
+ "platform": "osx"      # Now: macOS only
```

This change affects two dependencies:
- `sleef`: SIMD math library (only needed for rubberband)
- `rubberband`: Audio time-stretching/pitch-shifting library

### Impact

**Functionality Loss on Windows:**
- No rubberband-based time stretching (`aubio_timestretch_t` with rubberband backend)
- No rubberband-based pitch shifting (`aubio_pitchshift_t` with rubberband backend)

**Fallback Behavior:**
- Aubio automatically uses dummy implementations when rubberband is not available
- These dummy implementations print an error message when called
- Core aubio functionality (onset detection, pitch tracking, tempo, MFCC, etc.) is NOT affected

**Platforms Still With Rubberband Support:**
- macOS: ✅ Full rubberband support
- Linux: ❌ Already disabled (rubberband not in vcpkg.json for Linux)

## Alternative Approaches Considered

1. **Try to fix vcpkg rubberband package** - Would require:
   - Modifying vcpkg's rubberband port to support static SLEEF
   - Ensuring SLEEF exports symbols without `__imp_` in static builds
   - Contributing changes upstream to vcpkg
   - Time-intensive and outside scope of this PR

2. **Use dynamic linking on Windows** - Rejected because:
   - Defeats the purpose of portable wheels
   - Would require bundling DLLs with the Python package
   - Increases package size and complexity
   - Makes distribution more error-prone

3. **Use MinGW instead of MSVC** - Rejected because:
   - MSVC is the standard Windows toolchain
   - Better compatibility with vcpkg packages (especially FFmpeg)
   - MinGW would require significant build system changes

## Verification

The build will now:
1. Skip rubberband installation on Windows
2. Meson will detect rubberband as "not found" on Windows
3. `HAVE_RUBBERBAND` will not be defined in config.h
4. Dummy timestretch/pitchshift implementations will be used
5. Python extension will build successfully without rubberband dependencies

## Future Work

If rubberband support on Windows becomes critical:
1. Investigate vcpkg rubberband port modifications
2. Consider contributing a fix to vcpkg upstream
3. Or implement a pure-Python fallback for time-stretching/pitch-shifting effects

## References

- SLEEF (SIMD Library for Evaluating Elementary Functions): https://sleef.org/
- Rubberband Library: https://breakfastquay.com/rubberband/
- vcpkg rubberband port: https://github.com/microsoft/vcpkg/tree/master/ports/rubberband
- Windows DLL import symbols: https://docs.microsoft.com/en-us/cpp/build/reference/link-input-files
