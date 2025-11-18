# Python Bindings Migration Guide: Custom Generator → nanobind

**Project:** aubio-ledfx  
**Version:** 1.0  
**Created:** 2025-11-14  
**Purpose:** Comprehensive guide for migrating from custom C extension generator to nanobind

---

## Executive Summary

This document provides a complete migration plan for modernizing aubio-ledfx's Python bindings from the current custom code generation system to **nanobind**, a modern C++/Python binding framework.

### Decision: nanobind (not pybind11)

After comprehensive research, **nanobind is the recommended choice** over pybind11 for the following reasons:

| Criterion | pybind11 | nanobind | Winner |
|-----------|----------|----------|--------|
| **Compilation Speed** | Baseline | 4× faster | ✅ nanobind |
| **Binary Size** | Baseline | 5× smaller | ✅ nanobind |
| **Runtime Overhead** | Baseline | 10× lower | ✅ nanobind |
| **NumPy Integration** | Good | Excellent (zero-copy) | ✅ nanobind |
| **DSP/Audio Use Case** | Suitable | Optimized | ✅ nanobind |
| **Type Hints** | Yes | Yes (better) | ✅ nanobind |
| **API Familiarity** | Established | Similar to pybind11 | Tie |
| **Community Size** | Large | Growing | pybind11 |
| **Maturity** | Very mature | Modern (2022+) | pybind11 |
| **C++ Requirement** | C++11 | C++17 | Tie |
| **Wheel Size Impact** | Larger | Smaller | ✅ nanobind |

**Key advantages for aubio-ledfx:**
- **Performance-critical DSP:** nanobind's 10× lower overhead perfect for audio processing
- **Wheel distribution:** 5× smaller binaries reduce download size and storage
- **NumPy arrays:** Zero-copy exchange critical for real-time audio buffers
- **Build times:** 4× faster compilation improves CI/CD efficiency
- **Modern Python:** Excellent support for Python 3.8+ features
- **Future-proof:** Better support for free-threaded Python (PEP 703)

**Trade-offs accepted:**
- Smaller ecosystem than pybind11 (but similar API makes examples transferable)
- Requires C++17 (aubio is C99, but we'll create C++ wrapper layer)


### Current State Analysis

**Existing Python Bindings Architecture:**
```
python/
├── ext/                    # Hand-written C extension code
│   ├── aubiomodule.c      # Main Python module
│   ├── py-cvec.c          # Complex vector type
│   ├── py-fft.c           # FFT wrapper
│   ├── py-filter.c        # Filter wrapper
│   └── ...                # 10 hand-written wrappers
├── lib/
│   ├── gen_external.py    # Code generator (352 lines)
│   ├── gen_code.py        # Template engine (642 lines)
│   └── aubio/             # Pure Python helpers
└── gen/                   # Generated at build time
    ├── gen-onset.c        # Generated onset wrapper
    ├── gen-pitch.c        # Generated pitch wrapper
    └── ...                # 11 generated wrappers
```

**Build Process:**
1. `gen_external.py` parses `src/aubio.h` using C preprocessor
2. `gen_code.py` generates C extension wrappers using templates
3. Meson compiles hand-written + generated C code
4. Links against static `libaubio.a`
5. Produces `_aubio.so` (Linux/macOS) or `_aubio.pyd` (Windows)

**Issues with Current System:**
- ❌ 994 lines of custom code generation logic
- ❌ Fragile header parsing (depends on specific C syntax)
- ❌ No type hints or `.pyi` stub files
- ❌ Manual NumPy C API usage (error-prone)
- ❌ Build-time code generation adds complexity
- ❌ Hard to extend with new object types
- ❌ Limited error messages
- ❌ No IDE autocomplete support

### Migration Goals

**Primary Objectives:**
1. ✅ **Eliminate custom code generation** - Replace 994 lines with nanobind's automatic binding
2. ✅ **Add type hints** - Full `.pyi` stub files for IDE support
3. ✅ **Improve NumPy integration** - Use nanobind's zero-copy ndarray
4. ✅ **Reduce maintenance burden** - Simpler, more maintainable code
5. ✅ **Maintain API compatibility** - Existing Python code should work unchanged
6. ✅ **Improve build times** - Faster compilation with nanobind
7. ✅ **Smaller wheels** - Reduce wheel size by ~5×

**Non-Goals:**
- ❌ Changing the C library API (`src/aubio.h`)
- ❌ Rewriting existing Python tests (should pass unchanged)
- ❌ Breaking changes to public Python API

### Success Metrics

| Metric | Current | Target | Measurement |
|--------|---------|--------|-------------|
| **Code Generation Lines** | 994 | 0 | Delete `gen_external.py`, `gen_code.py` |
| **Type Hints** | None | Full coverage | `.pyi` files for all APIs |
| **Wheel Size** | ~5-8 MB | ~1-2 MB | Post-migration wheel comparison |
| **Build Time** | Baseline | 4× faster | CI build time comparison |
| **API Compatibility** | N/A | 100% | All existing tests pass |
| **NumPy Overhead** | Copy | Zero-copy | Verify `ndarray` usage |
| **IDE Support** | None | Full | mypy/pyright validation |

---

## Migration Strategy

### Phase Overview

The migration is divided into 5 phases to minimize risk and ensure smooth transition:

```
Phase 1: Preparation & Infrastructure (2-3 days)
  ↓
Phase 2: Proof of Concept (2-3 days)
  ↓
Phase 3: Core Types Migration (3-4 days)
  ↓
Phase 4: Objects Migration (5-7 days)
  ↓
Phase 5: Validation & Cleanup (2-3 days)

Total: 14-20 days
```


### Phase 1: Preparation & Infrastructure (2-3 days)

**Objective:** Set up nanobind build infrastructure without breaking existing bindings.

#### Step 1.1: Add nanobind Dependency

Add nanobind to vcpkg dependencies:

**File:** `vcpkg.json`
```json
{
  "dependencies": [
    "libsndfile",
    "libsamplerate",
    "fftw3",
    {
      "name": "nanobind",
      "version>=": "2.0.0"
    }
  ]
}
```

**Verify installation:**
```bash
vcpkg install --triplet=x64-linux
ls vcpkg_installed/x64-linux/include/nanobind/
```

#### Step 1.2: Create Meson Build Infrastructure

**File:** `python/nanobind_bindings/meson.build`
```meson
# nanobind Python bindings for aubio

nanobind_dep = dependency('nanobind', required: get_option('nanobind'))

if not nanobind_dep.found()
  warning('nanobind not found, skipping modern bindings')
  subdir_done()
endif

# C++ include directories
nanobind_inc = [
  include_directories('include'),
  include_directories('../../src'),
  config_inc,
]

# Will add sources incrementally during migration
nanobind_sources = files()

if nanobind_sources.length() == 0
  message('nanobind infrastructure prepared, no bindings yet')
  subdir_done()
endif

py.extension_module('_aubio_nb',
  nanobind_sources,
  dependencies: [nanobind_dep, py.dependency()],
  include_directories: nanobind_inc,
  link_with: libaubio_static,
  install: true,
  subdir: 'aubio',
  override_options: ['cpp_std=c++17'],
)
```

**File:** `meson_options.txt` (add option)
```meson
option('nanobind', type: 'feature', value: 'auto',
  description: 'Build modern nanobind Python bindings'
)
```

#### Step 1.3: Create C++ Wrapper Infrastructure

Since aubio is C99 but nanobind requires C++17, create RAII wrappers:

**File:** `python/nanobind_bindings/include/aubio_cpp.hpp`

This file contains C++ wrappers for aubio objects with:
- RAII memory management (automatic cleanup)
- Exception safety
- Move semantics
- Type-safe interfaces

Key wrapper classes:
- `FVec` - wraps `fvec_t*`
- `CVec` - wraps `cvec_t*`  
- `Onset` - wraps `aubio_onset_t*`
- `Pitch` - wraps `aubio_pitch_t*`
- `Tempo` - wraps `aubio_tempo_t*`
- And more for all aubio objects

#### Step 1.4: Create Type Stub Template

**File:** `python/aubio-stubs/__init__.pyi`

Contains type hints for IDE support and static type checking with:
- Class definitions with docstrings
- Method signatures with type annotations
- NumPy array type hints
- Literal types for method parameters

#### Step 1.5: Create Testing Infrastructure

**File:** `python/tests/test_nanobind.py`

Test suite structure:
- `TestFVec` - test fvec bindings
- `TestCVec` - test cvec bindings
- `TestOnset` - test onset detection
- `TestPitch` - test pitch detection
- And more for each object type

Tests verify:
- Object creation
- Method calls
- NumPy interoperability
- API compatibility with old bindings

**Validation:**
```bash
meson setup builddir -Dnanobind=enabled
meson compile -C builddir
pytest python/tests/test_nanobind.py -v
```

---

### Phase 2: Proof of Concept (2-3 days)

**Objective:** Create minimal working bindings to validate the approach.

#### Step 2.1: Implement fvec Bindings

**File:** `python/nanobind_bindings/src/core_types.cpp`

Implements nanobind bindings for fvec with:
- Constructor taking length
- `__len__`, `__getitem__`, `__setitem__`
- NumPy integration via `as_numpy()` (zero-copy view)
- `from_numpy()` static method (copy constructor)
- Proper Python indexing (negative indices)

Key nanobind features used:
- `nb::class_<T>` - bind C++ class
- `nb::init<Args...>` - bind constructor
- `nb::ndarray<...>` - NumPy array integration
- Lambda functions for custom behavior

#### Step 2.2: Add NumPy Integration Tests

Tests verify:
- Zero-copy NumPy views work correctly
- Modifications to NumPy array reflect in fvec
- Array creation from NumPy works
- Type conversions are correct

**Run benchmarks:**
```bash
python3 python/tests/bench_nanobind.py
```

Expected improvements:
- 4-10× faster object creation
- Similar or better element access speed
- Zero overhead on NumPy interop

#### Phase 2 Decision Point

If tests pass and performance improves, proceed to Phase 3.
Otherwise, revisit approach or stay with current system.

---

### Phase 3: Core Types Migration (3-4 days)

**Objective:** Migrate all core utility functions and types.

#### Objects to Migrate:

1. **Music utility functions** (`musicutils.cpp`)
   - Frequency/MIDI conversions
   - dB conversions
   - Silence/level detection
   - Zero-crossing rate
   - Autocorrelation

2. **FFT** (`spectral.cpp`)
   - Forward/inverse FFT
   - Integration with fvec/cvec

3. **Phase Vocoder** (`spectral.cpp`)
   - Forward/inverse phase vocoder
   - Window/hop size configuration

4. **Filters** (`filters.cpp`)
   - Digital filters
   - Filter banks
   - Mel filter banks

Each binding follows the pattern:
1. Add C++ wrapper to `aubio_cpp.hpp`
2. Create nanobind binding in source file
3. Add tests
4. Update type stubs

---

### Phase 4: Objects Migration (5-7 days)

**Objective:** Migrate all processing objects.

#### Objects to Migrate (in order):

1. **onset** - Onset detection
   - Methods: default, energy, hfc, complex, phase, etc.
   - Threshold/silence configuration
   - Minioi (minimum inter-onset interval)

2. **pitch** - Pitch detection
   - Methods: yin, yinfft, fcomb, mcomb, schmitt, etc.
   - Unit configuration (Hz, MIDI, cent, bin)
   - Confidence estimation

3. **tempo** - Tempo/beat tracking
   - BPM estimation
   - Beat positions
   - Confidence

4. **notes** - Note transcription
   - MIDI note detection
   - Onset/offset times
   - Velocity estimation

5. **mfcc** - Mel-Frequency Cepstral Coefficients
   - Feature extraction for ML
   - Configurable number of coefficients

6. **specdesc** - Spectral descriptors
   - Various spectral features
   - Used for onset detection algorithms

7. **tss** - Transient/Steady-State Separation
   - Separate transient from steady components

8. **pitchshift** - Pitch shifting
   - Time-stretch and pitch-shift audio

9. **wavetable** - Wavetable synthesis
   - Generate waveforms

10. **sampler** - Audio sampler
    - Playback samples with pitch/amplitude control

Each object migration includes:
- C++ wrapper class with RAII
- nanobind bindings with all methods
- Comprehensive tests
- Type stub updates
- Documentation

---

### Phase 5: Validation & Cleanup (2-3 days)

**Objective:** Ensure complete compatibility and remove old code.

#### Step 5.1: API Compatibility Validation

**File:** `python/tests/test_api_compat.py`

Automated tests that:
- Extract all public APIs from old extension
- Verify all exist in new bindings
- Check signatures match
- Test behavior equivalence

#### Step 5.2: Switch Default Bindings

**File:** `python/lib/aubio/__init__.py`

```python
# Try nanobind first, fall back to old extension
try:
    from aubio._aubio_nb import *
    _USING_NANOBIND = True
except ImportError:
    from aubio._aubio import *
    _USING_NANOBIND = False

def binding_info():
    if _USING_NANOBIND:
        return "nanobind (modern C++ bindings)"
    else:
        return "legacy C extension"
```

#### Step 5.3: Remove Old Code (FINAL STEP)

**Only after complete validation:**

Delete:
1. `python/lib/gen_external.py`
2. `python/lib/gen_code.py`
3. `python/ext/` directory
4. Generated code handling in `python/meson.build`

Update `python/meson.build` to only build nanobind version.

---

## Testing Strategy

### Test Levels

1. **Unit Tests** - Individual bindings
2. **Integration Tests** - Complete workflows
3. **Compatibility Tests** - API equivalence
4. **Performance Tests** - Benchmarks
5. **Regression Tests** - Existing tests unchanged

### Coverage Requirements

- Core Types: 100%
- Processing Objects: 95%
- Utility Functions: 90%
- NumPy Integration: 100%
- API Compatibility: 100%

### Continuous Testing

During migration: Both versions tested
After migration: Only nanobind version

---

## Risk Mitigation

### Risk 1: API Incompatibility
- **Mitigation:** Comprehensive tests, gradual migration, automated API comparison

### Risk 2: Performance Regression  
- **Mitigation:** Early benchmarking, profiling, optimization, fallback option

### Risk 3: Build Complexity
- **Mitigation:** vcpkg handles dependencies, clear documentation, CI validation

### Risk 4: Platform Issues
- **Mitigation:** Early multi-platform testing, platform-specific workarounds, fallback

---

## Timeline

| Phase | Duration | Tasks |
|-------|----------|-------|
| Phase 1 | 2-3 days | Infrastructure setup |
| Phase 2 | 2-3 days | Proof of concept |
| Phase 3 | 3-4 days | Core types |
| Phase 4 | 5-7 days | All objects |
| Phase 5 | 2-3 days | Validation & cleanup |
| **Total** | **14-20 days** | |

---

## Success Criteria

### Must Have
- ✅ All existing tests pass
- ✅ 100% API compatible
- ✅ Builds on all platforms
- ✅ Smaller wheels (5× target)
- ✅ Full type stubs
- ✅ Zero-copy NumPy
- ✅ Documentation updated

### Should Have
- ✅ 4× faster builds
- ✅ 10× lower runtime overhead
- ✅ Type checking passes
- ✅ Performance improvements

---

## References

### nanobind Resources
- Documentation: https://nanobind.readthedocs.io/
- GitHub: https://github.com/wjakob/nanobind
- Example: https://github.com/wjakob/nanobind_example
- Benchmarks: https://nanobind.readthedocs.io/en/latest/benchmark.html

### Technical Papers
- nanobind motivation: https://nanobind.readthedocs.io/en/latest/why.html
- NumPy C API: https://numpy.org/doc/stable/reference/c-api/
- DLPack: https://github.com/dmlc/dlpack

---

## Conclusion

This migration from custom C extension generation to nanobind will:

1. **Eliminate 994 lines** of fragile code generation logic
2. **Provide full type hints** for modern IDE support
3. **Improve performance** with 4-10× faster builds and lower runtime overhead
4. **Reduce wheel size** by ~5× for faster downloads
5. **Enable zero-copy NumPy** integration for efficient audio processing
6. **Simplify maintenance** with cleaner, more maintainable code

The phased approach ensures:
- Minimal risk (both versions coexist during migration)
- Continuous validation (tests run throughout)
- Clear rollback path (if issues arise)
- Thorough documentation (for future maintainers)

**Total effort:** 14-20 days for complete migration

**Document Version:** 1.0  
**Created:** 2025-11-14  
**Status:** Ready for implementation
