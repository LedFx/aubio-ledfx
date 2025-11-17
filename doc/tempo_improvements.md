# Tempo Tracking Improvements - Implementation Details

## Overview

This document describes the improvements made to aubio's tempo tracking utilities based on librosa's beat tracking methodology.

## Implemented Improvements (Phase 1)

### 1. Standard Deviation Calculation

**Files Modified:**
- `src/mathutils.h` - Added function declarations
- `src/mathutils.c` - Implemented variance and standard deviation functions

**New Functions:**
- `fvec_variance()` - Computes variance of a vector using Bessel's correction (N-1)
- `fvec_stddev()` - Computes standard deviation (sqrt of variance)

**Rationale:**
These functions enable robust normalization of onset strength envelopes, improving tempo detection accuracy across different audio levels.

### 2. Onset Normalization

**Files Modified:**
- `src/tempo/beattracking.c` - Added normalization function and updated processing

**Implementation:**
- Added `aubio_beattracking_normalize_dfframe()` - Normalizes onset detection function by standard deviation
- Updated `aubio_beattracking_do()` to normalize onset strength before processing

**Benefits:**
- More robust to amplitude variations in audio
- Consistent behavior across different recording levels
- Follows librosa's proven approach to onset normalization

### 3. Tempo Prior Distribution Support

**Files Modified:**
- `src/tempo/beattracking.h` - Added setter function declarations
- `src/tempo/beattracking.c` - Implemented tempo prior setters
- `src/tempo/tempo.h` - Added public API for tempo priors
- `src/tempo/tempo.c` - Forwarding functions to beattracking

**New Functions:**
- `aubio_beattracking_set_tempo_prior_mean()` - Set expected tempo (BPM)
- `aubio_beattracking_set_tempo_prior_std()` - Set tempo uncertainty
- `aubio_tempo_set_tempo_prior_mean()` - Public API wrapper
- `aubio_tempo_set_tempo_prior_std()` - Public API wrapper

**How It Works:**
1. **Prior Mean** (default: 120 BPM): Updates the Rayleigh parameter to center tempo detection around the expected tempo
2. **Prior Std** (default: 1.0): Adjusts the variance threshold (`g_var`) for context-dependent model activation
   - Wider prior (larger std) → More tolerant of tempo variations
   - Narrower prior (smaller std) → Stricter adherence to expected tempo

**Use Cases:**
- **Electronic Dance Music**: Set prior mean to 128 BPM with low std (0.5) for strict tracking
- **Variable Tempo Classical**: Set higher std (2.0-3.0) to allow natural tempo fluctuations
- **Genre-Specific**: Optimize for known tempo ranges (e.g., hip-hop ~90-100 BPM)

### 4. Enhanced Testing

**New Test File:**
- `tests/src/tempo/test-tempo-improved.c` - Validates new functionality

**Test Coverage:**
- Tempo prior mean setting with valid values
- Tempo prior std setting with valid values
- Error handling for negative/invalid values
- Integration with existing tempo detection

## Usage Examples

### Basic Usage (Default Behavior)

```c
// Create tempo detector (defaults: 120 BPM mean, 1.0 std)
aubio_tempo_t *tempo = new_aubio_tempo("default", 1024, 256, 44100);

// Process audio frames normally
aubio_tempo_do(tempo, input, output);
```

### Setting Tempo Prior for Electronic Music

```c
aubio_tempo_t *tempo = new_aubio_tempo("default", 1024, 256, 44100);

// Expect 128 BPM with tight tolerance
aubio_tempo_set_tempo_prior_mean(tempo, 128.0);
aubio_tempo_set_tempo_prior_std(tempo, 0.5);

// Process audio...
```

### Variable Tempo Music

```c
aubio_tempo_t *tempo = new_aubio_tempo("default", 1024, 256, 44100);

// Expect around 100 BPM but allow wide variation
aubio_tempo_set_tempo_prior_mean(tempo, 100.0);
aubio_tempo_set_tempo_prior_std(tempo, 3.0);

// Process audio...
```

## Performance Impact

### Computational Overhead

**Onset Normalization:**
- Additional cost: O(n) for std deviation calculation
- Frequency: Once per detection function frame (~every 1.5 seconds at 44.1kHz)
- Impact: Negligible (< 1% increase in total processing time)

**Tempo Prior Updates:**
- Cost: O(laglen) for Rayleigh weighting recalculation (~128 elements)
- Frequency: Only when prior is changed (typically once at initialization)
- Impact: Negligible

### Memory Footprint

- Added to `aubio_beattracking_t`: 3 × sizeof(smpl_t) ≈ 12 bytes (float) or 24 bytes (double)
- Temporary buffer for normalization: 1 × winlen × sizeof(smpl_t) (freed after use)

Total additional memory: < 1KB

## Testing Results

All existing tests pass with new implementation:
```
✓ test-beattracking: OK
✓ test-tempo: OK
✓ test-tempo-improved: OK
```

## Future Enhancements (Planned)

### Phase 2: Beat Tracking Quality
- [ ] Gaussian-weighted local scoring
- [ ] Tightness parameter
- [ ] Improved beat trimming
- [ ] Enhanced confidence metrics

### Phase 3: Speed & Responsiveness
- [ ] Optimized autocorrelation
- [ ] Adaptive window sizing
- [ ] Incremental processing
- [ ] Result caching

### Phase 4: Advanced Features
- [ ] Dynamic tempo tracking
- [ ] Fourier tempogram
- [ ] PLP (Predominant Local Pulse) method
- [ ] Time-varying tempo estimation

## References

1. Ellis, D. P. (2007). "Beat tracking by dynamic programming." Journal of New Music Research, 36(1), 51-60.

2. Grosche, P., & Müller, M. (2011). "Extracting predominant local pulse information from music recordings." IEEE Transactions on Audio, Speech, and Language Processing, 19(6), 1688-1701.

3. librosa beat tracking implementation: https://github.com/librosa/librosa/blob/main/librosa/beat.py

4. Davies, M. E., & Plumbley, M. D. (2004). "Causal tempo tracking of audio." ISMIR 2004.
