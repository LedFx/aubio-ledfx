#pragma once

// C++ wrappers for aubio C API
// Provides RAII wrappers and type-safe interfaces for nanobind
//
// This file will be populated during Phase 2-4 of the migration.
// See PYTHON_BINDINGS_MIGRATION_GUIDE.md for details.

#include <memory>
#include <stdexcept>
#include <string>

// Include aubio C headers
extern "C" {
#include "aubio/aubio.h"
}

namespace aubio_cpp {

// Forward declarations
// Will be implemented in Phase 2
// class FVec;
// class CVec;

// Will be implemented in Phase 3
// class FFT;
// class PhaseVocoder;
// class Filter;
// class FilterBank;

// Will be implemented in Phase 4
// class Onset;
// class Pitch;
// class Tempo;
// class Notes;
// class MFCC;
// class SpectralDescriptor;
// class TSS;
// class PitchShift;
// class Wavetable;
// class Sampler;

} // namespace aubio_cpp
