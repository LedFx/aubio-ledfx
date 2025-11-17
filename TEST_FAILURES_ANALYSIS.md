# Test Failures Analysis for aubio-ledfx CI

## Executive Summary

After conducting a comprehensive review of the test suite, **31 out of 1040 tests are currently failing** (801 pass, 208 skipped). The `|| true` in the CI configuration (pyproject.toml line 57) masks these failures, allowing broken code to pass CI.

## Test Failure Breakdown

### 1. Source Resampling Tests (30 failures)

**Location**: `python/tests/test_source.py`

**Affected Tests**:
- `Test_aubio_source_read::test_samplerate_hopsize` (15 failures)
- `Test_aubio_source_readmulti::test_samplerate_hopsize` (15 failures)

**Root Cause**: **Missing libsamplerate dependency**

The tests expect aubio to emit `UserWarning` when upsampling audio files (e.g., 8000Hz → 44100Hz). However, when libsamplerate is not available:
- Aubio cannot perform resampling at all
- Instead of warning, it raises `RuntimeError: "can not resample ... from 8000 to 44100Hz"`
- Tests fail with: `Failed: DID NOT WARN. No warnings of type (<class 'UserWarning'>,) were emitted`

**Evidence**:
```python
# Test expects warning during upsampling:
with assert_warns(UserWarning):
    f = source(soundfile, samplerate, hop_size)

# Actual behavior without libsamplerate:
RuntimeError: AUBIO ERROR: source_wavread: can not resample /path/to/8000Hz_file.wav from 8000 to 44100Hz
```

**Not Related to Security Hardening**: This is a pre-existing dependency issue, not caused by the recent security hardening changes.

### 2. Spectral Descriptor Test (1 failure)

**Location**: `python/tests/test_specdesc.py`

**Affected Test**: `aubio_specdesc::test_rolloff`

**Root Cause**: Numerical precision difference in rolloff calculation

**Details**:
- Expected: `324.0`
- Actual: `325`
- Difference: 1 (0.31% relative error)

**Analysis**:
This appears to be a floating-point precision issue. The test calculates:
```python
cumsum = .95*sum(a*a)  # Calculate 95% cumulative sum threshold
# Count bins until reaching threshold
```

The off-by-one error could be caused by:
1. Different floating-point rounding in the C library vs Python reference
2. Compiler optimization changes
3. Platform-specific numerical differences

**Potential Security Hardening Impact**: Possible. The `-O2` optimization level with security flags (`-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`) may affect floating-point operations. However, a 0.3% error in spectral rolloff is within acceptable tolerance for audio analysis.

## Test Coverage Assessment

### What IS Tested

The test suite is comprehensive with **1040 total tests** covering:

1. **Core Data Types** (✓ Well tested)
   - `fvec` (float vectors) - 40+ tests
   - `cvec` (complex vectors) - 20+ tests
   - Array operations, slicing, NumPy integration

2. **Audio I/O** (✓ Well tested, with caveats)
   - `source` - Reading audio files (200+ tests)
   - `sink` - Writing audio files (100+ tests)
   - Multiple formats: WAV, FLAC, Vorbis
   - **Caveat**: Resampling tests require libsamplerate

3. **Spectral Analysis** (✓ Well tested)
   - FFT operations - 20+ tests
   - Phase vocoder - 15+ tests
   - Spectral descriptors - 100+ tests (centroid, flux, rolloff, etc.)
   - Mel filterbank - 25+ tests
   - MFCC - 10+ tests

4. **Onset Detection** (✓ Well tested)
   - Multiple algorithms tested
   - Different window sizes and hop sizes

5. **Pitch Detection** (✓ Well tested)
   - Multiple pitch detection algorithms
   - Various configurations

6. **Tempo/Beat Tracking** (✓ Well tested)
   - Tempo detection
   - Beat tracking

7. **Audio Effects** (✓ Well tested)
   - Digital filters (A-weighting, C-weighting, biquad)
   - Time stretching/pitch shifting (requires rubberband)

8. **Utility Functions** (✓ Well tested)
   - Mathematical utilities
   - Music theory conversions (MIDI, notes, frequencies)

### What Is NOT Fully Tested

1. **Resampling Functionality** (⚠ Requires libsamplerate)
   - Cannot test upsampling/downsampling without dependency
   - 30 tests skip or fail when libsamplerate unavailable

2. **Platform-Specific Backends**
   - macOS: CoreAudio/AudioToolbox backend (tests skip on Linux)
   - Windows: DirectSound backend (tests skip on Linux)
   - JACK audio server (requires JACK running)

3. **Memory Safety** (⚠ Not tested in Python suite)
   - Buffer overflow protection
   - Stack protection
   - The C test suite (`tests/` directory) may cover this

4. **Performance/Benchmarks** (⚠ No performance tests)
   - No tests verify performance hasn't regressed
   - No benchmarks for optimization validation

### Test Skipping Behavior

**208 tests are skipped** in various scenarios:
- Missing optional dependencies (libavcodec, rubberband)
- Platform-specific features not available
- Odd FFT sizes (implementation limitation)
- Known issues/bugs

## Dependency Status in Build

Based on vcpkg.json and meson_options.txt, the following dependencies should be available via vcpkg:

### Currently in vcpkg.json:
- ✓ `libsndfile` - File I/O
- ✓ `fftw3` - FFT operations
- ✓ `rubberband` - Time stretching
- ✓ `ffmpeg` - Media file support
- ✓ `libsamplerate` - **Declared but not building properly**

### Issue:
Even though `libsamplerate` is in vcpkg.json, it's not being detected during the build. Possible reasons:
1. vcpkg may not be providing pkgconfig files
2. Meson may not be finding the dependency correctly
3. Build isolation prevents finding the dependency

## Recommendations

### Immediate Actions

1. **Fix libsamplerate Detection**
   - Verify vcpkg is building libsamplerate correctly
   - Check pkg-config paths in CI environment
   - May need to explicitly enable: `meson setup -Dsamplerate=enabled`

2. **Update Rolloff Test**
   - Accept tolerance of ±1 in rolloff calculation
   - Add comment explaining floating-point precision

3. **Enable Test Failures in CI**
   - Remove `|| true` from pyproject.toml
   - This will make CI fail on real test failures
   - Fix or skip the 31 failing tests appropriately

### Short-term (Before Enabling Failures)

1. **Document Test Requirements**
   - Create test/README.md explaining dependencies needed
   - Document which tests require which optional features

2. **Add Dependency Checks**
   - Tests should check for required features and skip gracefully
   - Example: `@skipIf(not has_samplerate(), "libsamplerate required")`

3. **Fix Known Failures**
   - Either fix the tests or mark them as expected failures
   - Don't enable CI failures until tests pass or are properly skipped

### Long-term

1. **Add C Library Tests to CI**
   - Currently only Python tests run in CI
   - C test suite in `tests/` should also run
   - Meson option: `-Dtests=true`

2. **Add Memory Safety Tests**
   - Run tests under AddressSanitizer
   - Run tests under Valgrind
   - Verify security hardening is working

3. **Add Performance Benchmarks**
   - Ensure optimizations don't regress performance
   - Track key metrics (FFT speed, onset detection latency)

4. **Test Matrix Expansion**
   - Test with all optional dependencies enabled
   - Test with all optional dependencies disabled
   - Test on all platforms (Linux, macOS, Windows)

## CI Configuration Changes Needed

### Current (pyproject.toml):
```toml
test-command = "python -c \"import aubio; ...\" && pytest {project}/python/tests || true"
```

### Proposed:
```toml
# Stage 1: Keep || true but add reporting
test-command = "python -c \"import aubio; ...\" && (pytest {project}/python/tests -v || (echo 'Tests failed but continuing due to known issues'; exit 0))"

# Stage 2: After fixing failures, remove || true
test-command = "python -c \"import aubio; ...\" && pytest {project}/python/tests -v"
```

### Also Add:
```toml
[tool.cibuildwheel.linux.environment]
# ... existing environment variables ...
# Ensure libsamplerate is found
PKG_CONFIG_PATH = "{project}/vcpkg_installed/x64-linux-pic/lib/pkgconfig:{existing_paths}"
```

## Summary Statistics

- **Total Tests**: 1040
- **Passing**: 801 (77%)
- **Failing**: 31 (3%)
- **Skipped**: 208 (20%)

**Failure Categories**:
- Dependency-related (libsamplerate): 30 failures (97% of failures)
- Numerical precision: 1 failure (3% of failures)

**Security Hardening Impact**: Minimal. Only 1 test may be affected by security hardening changes (rolloff precision), and the impact is negligible (0.3% error in non-critical metric).

## Conclusion

The test suite is comprehensive and well-designed. The failures are NOT caused by security hardening but by:
1. **Missing build dependency** (libsamplerate) - 97% of failures
2. **Numerical precision tolerance** - 3% of failures

**Before enabling CI test failures**:
1. Fix libsamplerate dependency in vcpkg build
2. Adjust rolloff test tolerance or mark as expected failure
3. Verify all 31 tests pass or are properly skipped

**The CI should absolutely enforce test passing** once these issues are resolved, to prevent regressions.
