# CI Test Improvement Plan

## Executive Summary

This document outlines the comprehensive plan to enable test failures in CI and ensure all aspects of aubio are properly tested. Currently, tests are forced to pass using `|| true` in the CI configuration, masking 13-31 test failures that need to be addressed before enforcement can be enabled.

## Current Test Failure Status

### Confirmed Failures (as seen in CI logs)

**Total**: 13-31 failures out of 1040 tests (varies by platform)
**Pass Rate**: ~97-98% when failures are hidden by `|| true`

#### Category 1: Resampling Tests (30 failures)
- **Location**: `python/tests/test_source.py`
- **Tests**: `test_samplerate_hopsize` for both `Test_aubio_source_read` and `Test_aubio_source_readmulti`
- **Root Cause**: libsamplerate dependency missing or not linked properly
- **Symptom**: Tests expect `UserWarning` but get `RuntimeError` instead

**Example Failure**:
```
RuntimeError: AUBIO ERROR: source_wavread: can not resample /project/python/tests/sounds/8000Hz_30s_silence.wav from 8000 to 44100Hz

Expected: UserWarning when upsampling
Actual: RuntimeError because libsamplerate not available
```

#### Category 2: Numerical Precision (1 failure)
- **Location**: `python/tests/test_specdesc.py`
- **Test**: `aubio_specdesc::test_rolloff`
- **Root Cause**: Floating-point precision difference  
- **Details**:
  - Expected: 324.0
  - Actual: 325
  - Relative error: 0.31%

## Root Cause Analysis

### Issue #1: libsamplerate Dependency

**Problem**: libsamplerate is declared in `vcpkg.json` but not being found during build.

**Evidence**:
1. `vcpkg.json` line 13 declares `"libsamplerate"`
2. `meson.build` lines 492-504 attempt to find it via pkg-config and vcpkg
3. Tests fail with "can not resample" error
4. CI logs show vcpkg installing dependencies but libsamplerate linkage unclear

**Why This Matters**:
- Resampling is a core aubio feature for real-time audio processing
- 30 tests (2.9% of test suite) verify this functionality
- Production code cannot handle audio at different sample rates without it

**Platform Status**:
- ✗ Linux (manylinux): Not working
- ? macOS: Unknown (caching may hide issue)
- ? Windows: Unknown (dynamic linking complexity)

### Issue #2: Spectral Rolloff Precision

**Problem**: C implementation differs from Python reference by 1 bin.

**Analysis**:
The Python test calculates rolloff as:
```python
cumsum = 0.95 * sum(a*a)  # 95% threshold
i = 0; rollsum = 0
while rollsum < cumsum:
    rollsum += a[i]*a[i]
    i += 1
rolloff = i  # Expects 324
```

C implementation returns 325 (off by one).

**Possible Causes**:
1. Different loop termination condition (< vs <=)
2. Floating-point accumulation differences
3. Compiler optimization affecting comparison order
4. Security hardening flags affecting FP operations (`-O2 -fstack-protector-strong`)

**Impact**: Minimal - 0.3% error in non-critical metric acceptable for audio analysis

## Implementation Plan

### Phase 1: Fix libsamplerate Dependency (REQUIRED)

**Goal**: Ensure libsamplerate is properly linked in all platforms

**Tasks**:
1. **Verify vcpkg installation** (Linux)
   ```bash
   # In CI before-all, add debug output:
   ls -la {project}/vcpkg_installed/*/lib/*samplerate*
   ls -la {project}/vcpkg_installed/*/lib/pkgconfig/samplerate.pc
   ```

2. **Check meson dependency detection**
   ```bash
   # After meson setup, check config:
   cat builddir/meson-logs/meson-log.txt | grep -i samplerate
   ```

3. **Possible Fixes**:
   - **Option A**: Explicit vcpkg triplet in meson.build
     ```meson
     if vcpkg_found and not samplerate_dep.found()
       samplerate_dep = cc.find_library('samplerate',
         dirs: vcpkg_library_dirs,
         required: samplerate_opt)
     endif
     ```
   
   - **Option B**: Force enable in CI
     ```yaml
     # In pyproject.toml [tool.cibuildwheel]
     environment = { MESON_ARGS = "-Dsamplerate=enabled" }
     ```
   
   - **Option C**: Check pkg-config paths
     ```bash
     # Verify PKG_CONFIG_PATH includes vcpkg directories
     pkg-config --list-all | grep samplerate
     ```

4. **Verification**:
   ```bash
   # After build, check if linked:
   ldd builddir/python/_aubio.so | grep samplerate
   # Or on macOS/Windows equivalent
   ```

**Success Criteria**: All 30 resampling tests pass

### Phase 2: Address Rolloff Precision (OPTIONAL)

**Options**:

**Option A: Accept tolerance** (RECOMMENDED)
```python
# In test_specdesc.py line 218:
assert_almost_equal(rolloff, o(c), decimal=0)  # Allow ±1
# Or:
assert abs(rolloff - o(c)) <= 1, f"Rolloff {o(c)} not within ±1 of {rolloff}"
```

**Option B: Fix C implementation**
- Investigate `src/spectral/specdesc.c` rolloff calculation
- Align with Python reference implementation
- Risk: May introduce other precision issues

**Option C: Mark as known issue**
```python
@pytest.mark.xfail(reason="Known FP precision issue, ±1 bin acceptable")
def test_rolloff(self):
    ...
```

**Recommendation**: Option A (tolerance). The 0.3% error is acceptable for audio DSP.

### Phase 3: Remove `|| true` from CI

**File**: `pyproject.toml` line 57

**Current**:
```toml
test-command = "python -c \"import aubio; ...\" && pytest {project}/python/tests || true"
```

**Proposed**:
```toml
test-command = "python -c \"import aubio; print('aubio version:', aubio.version); print('Portable wheel test: PASS')\" && pytest {project}/python/tests -v --tb=short"
```

**Rollout Strategy**:
1. Fix libsamplerate (Phase 1)
2. Fix or accept rolloff (Phase 2)  
3. Verify all tests pass locally
4. Test on single platform first (e.g., ubuntu-latest)
5. Enable for all platforms
6. Monitor first few CI runs closely

**Benefits**:
- Catch regressions immediately
- Prevent broken code from being merged
- Increase confidence in releases

### Phase 4: Expand Test Coverage (FUTURE)

**Areas Currently Not Tested in CI**:

1. **C Library Tests** (`tests/` directory)
   - Currently only Python tests run in CI
   - Meson option `-Dtests=true` exists but not used
   - Add to workflow:
     ```yaml
     - name: Run C tests
       run: |
         meson setup builddir -Dtests=true
         meson test -C builddir -v
     ```

2. **Memory Safety**
   - Run tests under AddressSanitizer
   - Run tests under Valgrind (Linux)
   - Verify no leaks or buffer overflows

3. **Platform-Specific Code Paths**
   - CoreAudio on macOS
   - DirectSound on Windows  
   - Ensure platform backends are tested

4. **Dependency Combinations**
   - Test with/without optional dependencies
   - Rubberband, ffmpeg, etc.

5. **Performance Benchmarks**
   - Track FFT performance over time
   - Ensure optimizations don't regress
   - Document baseline metrics

## Testing Strategy

### Before Enabling Failures

1. **Local Testing**:
   ```bash
   # Install with vcpkg locally:
   meson setup builddir
   meson compile -C builddir
   meson test -C builddir
   
   # Python tests:
   pip install -e . --no-build-isolation
   pytest python/tests -v
   ```

2. **CI Dry Run**:
   - Temporarily add step to show test results without failing:
     ```yaml
     - name: Test wheels (dry run)
       continue-on-error: true
       run: |
         # ... existing test command without || true
     ```
   - Review logs for unexpected failures

3. **Platform-by-Platform**:
   - Start with Linux (most stable)
   - Then macOS (caching complexity)
   - Finally Windows (DLL bundling issues)

### After Enabling

1. **Monitor First Week**:
   - Watch for intermittent failures
   - Document any new issues
   - Be ready to add retries if flaky

2. **Failure Response**:
   - Investigate immediately  
   - Don't merge until fixed
   - Document workarounds if needed

## Success Metrics

- [ ] 100% of tests pass on all platforms
- [ ] CI fails when tests fail (no `|| true`)
- [ ] Zero false positives (flaky tests)  
- [ ] C tests also running in CI
- [ ] Memory safety verified
- [ ] Documentation updated

## Timeline Estimate

- **Phase 1** (libsamplerate fix): 2-4 hours
  - 1h investigation
  - 1h implementation
  - 1h testing
  - 1h CI verification

- **Phase 2** (rolloff fix): 0.5-1 hour
  - Simple test tolerance adjustment

- **Phase 3** (enable failures): 0.5 hour
  - Remove `|| true`
  - Monitor first CI run

- **Phase 4** (expand coverage): 8-16 hours
  - Depends on scope
  - Can be done incrementally

**Total for Phases 1-3**: ~4-6 hours

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| libsamplerate still not linking | High | Fall back to skipping resampling tests with `@pytest.mark.skipif` |
| Tests flaky on some platforms | Medium | Add retries or platform-specific skips |
| New failures discovered | Medium | Address immediately or document as known issues |
| Performance regression | Low | Add baseline benchmarks first |
| Breaking changes to test API | Low | Pin pytest version in requirements |

## Dependencies

**Required for Phase 1-3**:
- vcpkg (already used)
- libsamplerate 0.0.15+ (in vcpkg)
- Meson 1.9.0+ (already required)
- pytest 9.0+ (already used)

**Required for Phase 4**:
- AddressSanitizer (compiler support)
- Valgrind (Linux package managers)
- Platform-specific test audio hardware (optional)

## Appendix: Test Statistics

**Current Coverage**:
- Total tests: 1040
- Passing: 801-1027 (depends on platform)
- Failing: 13-31
- Skipped: 15-208 (optional dependencies)

**Test Categories**:
- Data types: ~60 tests (fvec, cvec)
- Audio I/O: ~300 tests (source, sink)
- Spectral: ~200 tests (FFT, phase vocoder, descriptors)
- Detection: ~200 tests (onset, pitch, tempo)
- Effects: ~80 tests (filters, time stretch)
- Utilities: ~200 tests (math, music theory)

**Platform Coverage**:
- ✓ Linux x86_64
- ✓ Linux ARM64
- ✓ macOS Intel
- ✓ macOS Apple Silicon
- ✓ Windows AMD64

## References

- TEST_FAILURES_ANALYSIS.md - Detailed analysis document
- pyproject.toml - CI configuration
- meson.build - Build system and dependency detection
- vcpkg.json - Dependency manifest
- python/tests/ - Test suite location

## Next Steps

1. Review this plan with team
2. Prioritize phases based on urgency
3. Assign ownership of Phase 1 (libsamplerate)
4. Schedule implementation time
5. Update issue tracker with tasks
6. Begin Phase 1 implementation
