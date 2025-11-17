# Phase 3: Fourier Tempogram Implementation

## Overview

The Fourier tempogram provides a time-frequency representation of tempo across time, enabling:
1. Multi-resolution tempo analysis
2. Time-varying tempo tracking
3. Efficient computation via FFT
4. Foundation for PLP (Predominant Local Pulse) method

## Theory

### Fourier Tempogram

A Fourier tempogram is computed by:
1. Taking onset strength envelope (already computed in beat tracking)
2. Applying Short-Time Fourier Transform (STFT) to get frequency content over time
3. Converting frequency bins to tempo (BPM) representation
4. Result: 2D matrix showing tempo energy across time

**Mathematical formulation:**
```
T(τ, t) = |STFT{onset(t)}(f)|²
where τ = tempo lag, f = corresponding frequency
```

### Relationship to autocorrelation

Fourier tempogram and autocorrelation are mathematically related via Wiener-Khinchin theorem:
- Autocorrelation in time domain ←→ Power spectrum in frequency domain
- Fourier tempogram uses STFT bins directly (no inverse transform needed)
- Enables efficient multi-scale analysis

## Implementation Plan

### Phase 3.1: Fourier Tempogram Core (NEW STRUCTURE)

**New file: `src/tempo/tempogram.h`**
```c
typedef struct _aubio_tempogram_t aubio_tempogram_t;

// Create tempogram analyzer
aubio_tempogram_t* new_aubio_tempogram(uint_t win_s, uint_t hop_s, 
                                        uint_t samplerate);

// Delete tempogram
void del_aubio_tempogram(aubio_tempogram_t* o);

// Compute tempogram from onset strength
void aubio_tempogram_do(aubio_tempogram_t* o, const fvec_t* onset, 
                         fmat_t* tempogram);

// Get tempo from tempogram
smpl_t aubio_tempogram_get_tempo(const aubio_tempogram_t* o, 
                                  const fmat_t* tempogram);

// Get confidence
smpl_t aubio_tempogram_get_confidence(const aubio_tempogram_t* o);
```

**Internal structure:**
```c
struct _aubio_tempogram_t {
  uint_t win_s;              // Tempogram window size (e.g., 384)
  uint_t hop_s;              // Hop size
  uint_t samplerate;         // Sample rate
  
  aubio_fft_t* fft;          // FFT processor
  fvec_t* window;            // Hann window
  cvec_t* fftout;            // FFT output
  fmat_t* frames_buffer;     // Circular buffer of onset frames
  
  smpl_t tempo_min_bpm;      // Minimum tempo (default: 30)
  smpl_t tempo_max_bpm;      // Maximum tempo (default: 300)
  uint_t tempo_min_idx;      // Corresponding FFT bin
  uint_t tempo_max_idx;      // Corresponding FFT bin
  
  fvec_t* bpm_bins;          // BPM value for each FFT bin
  smpl_t current_tempo;      // Detected tempo
  smpl_t confidence;         // Detection confidence
};
```

### Phase 3.2: Integration with Beat Tracking

**Modify: `src/tempo/beattracking.h`**
```c
// Add to aubio_beattracking_t
aubio_tempogram_t* tempogram;  // Optional tempogram analyzer
uint_t use_tempogram;          // Flag to enable

// New API
uint_t aubio_beattracking_set_use_tempogram(aubio_beattracking_t* bt, uint_t use);
```

**Modify: `src/tempo/tempo.h`**
```c
// Public API
uint_t aubio_tempo_set_use_tempogram(aubio_tempo_t* o, uint_t use);
```

### Phase 3.3: PLP (Predominant Local Pulse)

PLP extracts the most salient periodic component from tempogram:

**Algorithm:**
1. Compute Fourier tempogram
2. For each time frame, find dominant tempo component
3. Apply adaptive threshold to suppress noise
4. Smooth across time for stability
5. Output time-varying tempo curve

**Implementation in tempogram.c:**
```c
// Get predominant tempo at specific time
smpl_t aubio_tempogram_get_plp_at_time(aubio_tempogram_t* o, 
                                        const fmat_t* tempogram, 
                                        uint_t time_idx);

// Get entire PLP curve
void aubio_tempogram_get_plp_curve(aubio_tempogram_t* o, 
                                    const fmat_t* tempogram, 
                                    fvec_t* plp_curve);
```

## Benchmarking Strategy

### Stage 1: Tempogram Core (Commit 1)
- Implement `new/del_aubio_tempogram()`
- Implement `aubio_tempogram_do()`
- Test FFT bin to BPM conversion
- Verify against librosa tempogram
- **Test**: `test-tempogram-basic`

### Stage 2: Tempo Extraction (Commit 2)
- Implement `aubio_tempogram_get_tempo()`
- Compare with autocorrelation method
- Benchmark on test_bpm_changes.wav
- **Expect**: Similar accuracy, potential speed improvement
- **Test**: `test-tempogram-tempo`

### Stage 3: Integration (Commit 3)
- Add tempogram to beattracking
- Add `aubio_tempo_set_use_tempogram()` API
- Python bindings auto-generated
- Full benchmark comparison
- **Test**: `test-tempo-benchmark` with tempogram enabled

### Stage 4: PLP Method (Commit 4)
- Implement PLP extraction
- Add `aubio_tempo_set_use_plp()` API
- Benchmark time-varying tempo
- Test on test_bpm_gradual.wav
- **Test**: `test-tempogram-plp`

## Expected Performance

### Fourier Tempogram vs Autocorrelation

**Advantages:**
- O(N log N) complexity (already have FFT)
- Multi-resolution: different window sizes for different tempo ranges
- Time-varying tempo: see tempo evolution
- Foundation for advanced methods (PLP, beat tracking DP)

**Trade-offs:**
- Additional memory for tempogram matrix
- Requires buffering onset frames
- May need parameter tuning

### Benchmark Goals

**Accuracy:**
- Maintain 100% detection rate
- Keep BPM error < 1.0 BPM average
- Comparable or better than autocorrelation

**Speed:**
- Response time: Target < 2.0s average (currently 2.69s)
- Computational overhead: < 5% vs current implementation

**Time-Varying Tempo (gradual changes):**
- Track tempo changes within 1.0 second
- Smooth transitions (no jumps)
- Adapt to accelerando/ritardando

## Implementation Notes

### FFT Window Size Selection

For tempo analysis, window size determines tempo resolution:
```
tempo_resolution_bpm = 60 * samplerate / (window_size * hop_size)
```

**Example**: samplerate=44100, hop_size=256
- window_size=384 → resolution ≈ 6.9 BPM (coarse, fast tempo changes)
- window_size=768 → resolution ≈ 3.4 BPM (medium)
- window_size=1536 → resolution ≈ 1.7 BPM (fine, slow tempo changes)

**Strategy**: Use adaptive window based on confidence
- Low confidence → larger window (more stable)
- High confidence → smaller window (more responsive)

### Memory Requirements

**Tempogram matrix size:**
```
rows = fft_size / 2 + 1  (number of tempo bins)
cols = buffer_frames      (time frames, e.g., 32)
```

Example: 384 FFT, 32 frames buffer
- Matrix: 193 × 32 × sizeof(smpl_t) ≈ 25 KB

### Security Considerations

All new code will follow SECURITY/DEFENSIVE_PROGRAMMING.md:
- `AUBIO_ASSERT_NOT_NULL()` on all pointers
- `AUBIO_ASSERT_BOUNDS()` on all array accesses
- `AUBIO_ASSERT_LENGTH()` on buffer allocations
- `goto beach` cleanup pattern
- Proper memory allocation checks

## Testing Plan

### Unit Tests

1. **test-tempogram-basic.c**
   - FFT bin to BPM conversion
   - Window function application
   - Frame buffering

2. **test-tempogram-tempo.c**
   - Tempo extraction from tempogram
   - Comparison with autocorrelation
   - Edge cases (very slow/fast tempo)

3. **test-tempogram-plp.c**
   - PLP extraction
   - Time-varying tempo tracking
   - Accelerando/ritardando

### Integration Tests

1. **test-tempo-benchmark** (modified)
   - Add `--tempogram` flag
   - Compare autocorrelation vs tempogram
   - Full metrics comparison

2. **test-tempo-benchmark-gradual** (new)
   - Test on test_bpm_gradual.wav
   - Measure tempo tracking accuracy during transitions
   - PLP vs standard method

### Python Demos

1. **demo_tempogram_visualization.py**
   - Generate and plot tempogram
   - Compare with autocorrelation
   - Show time-varying tempo

2. **demo_plp_tracking.py**
   - Demonstrate PLP extraction
   - Show smooth tempo transitions
   - Visualize tempo curve

## References

1. Grosche, P., & Müller, M. (2011). "Extracting predominant local pulse information from music recordings." IEEE Transactions on Audio, Speech, and Language Processing, 19(6), 1688-1701.

2. librosa tempogram: https://librosa.org/doc/main/generated/librosa.feature.tempogram.html

3. Ellis, D. P. (2007). "Beat tracking by dynamic programming." Journal of New Music Research, 36(1), 51-60.

## Deliverables

- [ ] `src/tempo/tempogram.h` - Header with API
- [ ] `src/tempo/tempogram.c` - Implementation
- [ ] `tests/src/tempo/test-tempogram-basic.c` - Unit tests
- [ ] `tests/src/tempo/test-tempogram-tempo.c` - Tempo extraction tests
- [ ] `tests/src/tempo/test-tempogram-plp.c` - PLP tests
- [ ] `doc/TEMPOGRAM_API.md` - API documentation
- [ ] `python/demos/demo_tempogram_visualization.py` - Python demo
- [ ] Updated `doc/tempo_improvements.md` - Mark Phase 3 complete
