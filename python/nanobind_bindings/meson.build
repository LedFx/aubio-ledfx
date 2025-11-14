# nanobind Python bindings for aubio
# Modern C++ bindings using nanobind framework

nanobind_dep = dependency('nanobind', required: get_option('nanobind'))

if not nanobind_dep.found()
  if get_option('nanobind').enabled()
    error('nanobind requested but not found')
  endif
  warning('nanobind not found, skipping modern bindings')
  subdir_done()
endif

# C++ include directories
nanobind_inc = [
  include_directories('include'),          # aubio_cpp.hpp
  include_directories('../../src'),         # aubio C headers
  config_inc,                               # config.h
]

# nanobind binding sources (will add incrementally during migration)
# Phase 1: Infrastructure only, no sources yet
nanobind_sources = files(
  # Phase 2: Proof of Concept
  # 'src/core_types.cpp',    # fvec, cvec bindings
  # 'src/module.cpp',        # Module definition
  
  # Phase 3: Core Types
  # 'src/musicutils.cpp',    # Music utility functions
  # 'src/spectral.cpp',      # FFT, Phase Vocoder
  
  # Phase 4: Objects
  # 'src/onset.cpp',         # Onset detection
  # 'src/pitch.cpp',         # Pitch detection
  # 'src/tempo.cpp',         # Tempo detection
  # 'src/notes.cpp',         # Note transcription
  # 'src/mfcc.cpp',          # MFCC
  # 'src/specdesc.cpp',      # Spectral descriptors
  # 'src/tss.cpp',           # Transient/Steady-State Separation
  # 'src/pitchshift.cpp',    # Pitch shifting
  # 'src/wavetable.cpp',     # Wavetable
  # 'src/sampler.cpp',       # Sampler
)

# For Phase 1, no sources yet - just verify infrastructure
if nanobind_sources.length() == 0
  message('nanobind infrastructure prepared, no bindings yet')
  subdir_done()
endif

# Build nanobind extension (will be used in Phase 2+)
py.extension_module('_aubio_nb',
  nanobind_sources,
  dependencies: [nanobind_dep, py.dependency()],
  include_directories: nanobind_inc,
  link_with: libaubio_static,
  cpp_args: ['-DAUBIO_VERSION=' + aubio_version],
  install: true,
  subdir: 'aubio',
  override_options: ['cpp_std=c++17'],  # nanobind requires C++17
)
