# nanobind Python Bindings for aubio

This directory contains the modern nanobind-based Python bindings for aubio.

## Overview

These bindings are a modernization effort to replace the custom C extension generator
(`gen_external.py`, `gen_code.py`) with nanobind, a high-performance C++/Python binding
framework.

## Status

**Phase 1: Infrastructure** (In Progress)
- [x] nanobind added to vcpkg dependencies
- [x] Meson build infrastructure created
- [x] Directory structure established
- [ ] C++ wrapper headers created
- [ ] Type stub templates created
- [ ] Test infrastructure prepared

See `PYTHON_BINDINGS_MIGRATION_GUIDE.md` in the project root for the complete migration plan.

## Benefits of nanobind

- **4-10× faster** compilation and runtime performance
- **5× smaller** binary size
- **Zero-copy NumPy** integration for efficient audio processing
- **Full type hints** for IDE support
- **Modern C++17** with automatic binding generation
- **Better multi-threading** support

## Directory Structure

```
nanobind_bindings/
├── include/
│   └── aubio_cpp.hpp       # C++ wrappers for aubio C API (RAII, type-safe)
├── src/
│   ├── module.cpp          # Main Python module definition
│   ├── core_types.cpp      # fvec, cvec bindings
│   ├── musicutils.cpp      # Music utility functions
│   ├── spectral.cpp        # FFT, Phase Vocoder
│   ├── onset.cpp           # Onset detection
│   ├── pitch.cpp           # Pitch detection
│   └── ...                 # Other object bindings
├── meson.build             # Build configuration
└── README.md               # This file
```

## Building

The nanobind bindings are built alongside the traditional C extension:

```bash
# Build with nanobind support (auto-detect)
meson setup builddir
meson compile -C builddir

# Explicitly enable nanobind
meson setup builddir -Dnanobind=enabled
meson compile -C builddir

# Disable nanobind (use only old C extension)
meson setup builddir -Dnanobind=disabled
meson compile -C builddir
```

## Usage

Once built, the nanobind module can be imported:

```python
# Try nanobind version (if available)
try:
    import aubio._aubio_nb as aubio
except ImportError:
    import aubio._aubio as aubio  # Fall back to old C extension

# Use as normal
vec = aubio.fvec(512)
onset = aubio.onset("default", 512, 256, 44100)
```

## Testing

Tests for nanobind bindings are in `python/tests/test_nanobind.py`:

```bash
pytest python/tests/test_nanobind.py -v
```

## Migration Plan

The migration from the old C extension to nanobind is phased:

1. **Phase 1:** Infrastructure setup (current)
2. **Phase 2:** Proof of Concept (fvec, cvec)
3. **Phase 3:** Core Types (FFT, utils, filters)
4. **Phase 4:** All Objects (onset, pitch, tempo, etc.)
5. **Phase 5:** Validation and cleanup

See the migration guide for details.

## Requirements

- **nanobind** >= 2.0.0 (via vcpkg)
- **C++17** compatible compiler
- **NumPy** >= 1.26.4
- **Python** >= 3.8

## References

- [nanobind documentation](https://nanobind.readthedocs.io/)
- [Migration Guide](../../PYTHON_BINDINGS_MIGRATION_GUIDE.md)
- [aubio C API](../../src/aubio.h)
