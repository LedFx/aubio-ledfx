# Phase 3D: Dynamic Programming Beat Tracking

## Overview

This document describes the implementation of Ellis (2007) Dynamic Programming beat tracking algorithm for aubio. The DP approach provides globally optimal beat sequence selection by balancing local onset evidence with tempo continuity constraints.

**Reference**: Ellis, D. P. W. (2007). "Beat Tracking by Dynamic Programming". Journal of New Music Research, 36(1), 51-60.

## Algorithm Summary

### Core Concept

The DP algorithm reformulates beat tracking as an optimization problem:
- **Goal**: Find the sequence of beat times that maximizes a global score
- **Balance**: Local onset strength vs. tempo continuity
- **Method**: Viterbi algorithm (dynamic programming) for efficient search

### Key Components

1. **Observation Model**: Onset strength function O(t) - likelihood of beat at time t
2. **Tempo Model**: Expected inter-beat interval δ̂ (from global tempo estimate)
3. **Cost Function**: Penalty for deviating from expected tempo
4. **DP Trellis**: State space of all possible beat sequences

## Mathematical Formulation

### Cost Function Formula

The penalty for deviation from ideal inter-beat interval:

```
P_δ̂(δ) = -[log₂(δ/δ̂)]²
```

Where:
- δ = actual inter-beat interval (samples between consecutive beats)
- δ̂ = ideal inter-beat interval (from tempo estimate)
- Penalty is symmetric on log scale (doubling/halving has equal penalty)
- Minimum penalty (0) when δ = δ̂

### DP Score Function

For a sequence of beat times {t₁, t₂, ..., tₙ}:

```
C({tᵢ}) = Σᵢ O(tᵢ) + Σᵢ P_δ̂(tᵢ - tᵢ₋₁)
```

Components:
- O(tᵢ): Onset strength at beat position i
- P_δ̂(tᵢ - tᵢ₋₁): Tempo continuity penalty

### DP Recursion

For each frame i:
```
DP[i] = O[i] + max_j (DP[j] + P_δ̂(i - j))
```

Where j ranges over valid previous beat positions (within tempo bounds).

## Implementation Design

### New Data Structures

#### 1. Dynamic Programming Tracker

```c
typedef struct _aubio_dptracker_t {
  uint_t win_s;              // DP window size (frames)
  uint_t hop_s;              // Hop size
  uint_t samplerate;         // Sample rate
  
  // DP state
  fvec_t *dp_score;          // DP[i] - best cumulative score to frame i
  lvec_t *dp_backptr;        // PREV[i] - previous beat index for path
  fvec_t *onset_buffer;      // Circular buffer of onset values
  uint_t buffer_pos;         // Current position in circular buffer
  
  // Tempo model
  smpl_t ideal_interval;     // δ̂ - ideal inter-beat interval (frames)
  smpl_t tempo_mean_bpm;     // Expected tempo (BPM)
  smpl_t tempo_std_bpm;      // Tempo uncertainty (BPM)
  
  // Search bounds (efficiency)
  uint_t min_interval;       // Minimum beat interval (frames)
  uint_t max_interval;       // Maximum beat interval (frames)
  
  // Output
  lvec_t *beat_sequence;     // Final beat positions
  uint_t num_beats;          // Number of beats in sequence
  
  // Performance
  smpl_t last_bpm;           // Most recent tempo estimate
  smpl_t confidence;         // Path confidence score
  
} aubio_dptracker_t;
```

#### 2. Integration with Existing Tempo System

Modify `aubio_beattracking_t`:
```c
struct _aubio_beattracking_t {
  // ... existing fields ...
  
  // Phase 3D: Dynamic Programming (optional)
  aubio_dptracker_t *dptracker;  // DP beat tracker
  uint_t use_dp;                 // Flag to enable DP mode
  
  // ... rest of structure ...
};
```

### API Design

#### Core DP Functions

```c
// Create DP tracker
aubio_dptracker_t *new_aubio_dptracker(uint_t win_s, uint_t hop_s,
                                        uint_t samplerate);

// Delete DP tracker
void del_aubio_dptracker(aubio_dptracker_t *dp);

// Feed onset value and update DP trellis
void aubio_dptracker_do(aubio_dptracker_t *dp, smpl_t onset_value);

// Get current beat sequence
void aubio_dptracker_get_beats(aubio_dptracker_t *dp, fvec_t *beats);

// Get current tempo estimate from DP path
smpl_t aubio_dptracker_get_bpm(const aubio_dptracker_t *dp);

// Get path confidence
smpl_t aubio_dptracker_get_confidence(const aubio_dptracker_t *dp);

// Set tempo prior for DP model
void aubio_dptracker_set_tempo(aubio_dptracker_t *dp, smpl_t bpm,
                                smpl_t std_bpm);
```

#### High-Level API (Integration)

```c
// Enable DP tracking in beattracking
uint_t aubio_beattracking_set_use_dp(aubio_beattracking_t *bt, uint_t use);

// Enable DP tracking in tempo
uint_t aubio_tempo_set_use_dp(aubio_tempo_t *tempo, uint_t use);

// Set tempo prior for DP (pass-through to dptracker)
uint_t aubio_tempo_set_dp_tempo_prior(aubio_tempo_t *tempo, smpl_t bpm,
                                       smpl_t std_bpm);
```

### Algorithm Implementation

#### Initialization

```c
aubio_dptracker_t *new_aubio_dptracker(uint_t win_s, uint_t hop_s,
                                        uint_t samplerate) {
  aubio_dptracker_t *dp = AUBIO_NEW(aubio_dptracker_t);
  
  // Allocate buffers
  dp->dp_score = new_fvec(win_s);
  dp->dp_backptr = new_lvec(win_s);
  dp->onset_buffer = new_fvec(win_s);
  dp->beat_sequence = new_lvec(win_s);
  
  // Set defaults (120 BPM)
  aubio_dptracker_set_tempo(dp, 120.0, 10.0);
  
  // Compute search bounds
  // min_interval = 30 BPM (2 seconds per beat at 44100 Hz)
  // max_interval = 240 BPM (0.25 seconds per beat)
  dp->min_interval = 60.0 * samplerate / (240.0 * hop_s);
  dp->max_interval = 60.0 * samplerate / (30.0 * hop_s);
  
  return dp;
}
```

#### DP Core Loop

```c
void aubio_dptracker_do(aubio_dptracker_t *dp, smpl_t onset_value) {
  uint_t i = dp->buffer_pos;
  
  // Store onset value
  dp->onset_buffer->data[i] = onset_value;
  
  // Initialize DP score for current frame
  dp->dp_score->data[i] = onset_value;
  dp->dp_backptr->data[i] = -1;  // No predecessor yet
  
  // Search for best predecessor
  smpl_t best_score = onset_value;
  sint_t best_prev = -1;
  
  uint_t j_min = (i > dp->max_interval) ? (i - dp->max_interval) : 0;
  uint_t j_max = (i > dp->min_interval) ? (i - dp->min_interval) : 0;
  
  for (uint_t j = j_min; j < j_max; j++) {
    // Compute inter-beat interval
    smpl_t delta = (smpl_t)(i - j);
    
    // Compute tempo continuity penalty
    smpl_t log_ratio = LOG(delta / dp->ideal_interval) / LOG(2.0);
    smpl_t penalty = -(log_ratio * log_ratio);
    
    // Compute cumulative score via this path
    smpl_t score = dp->dp_score->data[j] + onset_value + penalty;
    
    // Update if better
    if (score > best_score) {
      best_score = score;
      best_prev = j;
    }
  }
  
  // Store best path
  dp->dp_score->data[i] = best_score;
  dp->dp_backptr->data[i] = best_prev;
  
  // Advance buffer position
  dp->buffer_pos = (dp->buffer_pos + 1) % dp->win_s;
}
```

#### Beat Sequence Extraction (Viterbi Backtracking)

```c
void aubio_dptracker_get_beats(aubio_dptracker_t *dp, fvec_t *beats) {
  // Find highest score in DP table
  uint_t best_end = 0;
  smpl_t best_score = dp->dp_score->data[0];
  
  for (uint_t i = 1; i < dp->win_s; i++) {
    if (dp->dp_score->data[i] > best_score) {
      best_score = dp->dp_score->data[i];
      best_end = i;
    }
  }
  
  // Backtrack to recover beat sequence
  uint_t num_beats = 0;
  sint_t idx = best_end;
  
  while (idx >= 0 && num_beats < dp->beat_sequence->length) {
    dp->beat_sequence->data[num_beats] = idx;
    num_beats++;
    idx = dp->dp_backptr->data[idx];
  }
  
  dp->num_beats = num_beats;
  
  // Reverse sequence (was built backwards)
  for (uint_t i = 0; i < num_beats; i++) {
    beats->data[i] = dp->beat_sequence->data[num_beats - 1 - i];
  }
}
```

#### Tempo Estimation from DP Path

```c
smpl_t aubio_dptracker_get_bpm(const aubio_dptracker_t *dp) {
  if (dp->num_beats < 2) {
    return 0.0;  // Not enough beats
  }
  
  // Compute average inter-beat interval
  smpl_t total_interval = 0.0;
  for (uint_t i = 1; i < dp->num_beats; i++) {
    total_interval += dp->beat_sequence->data[i] - 
                      dp->beat_sequence->data[i-1];
  }
  smpl_t avg_interval = total_interval / (dp->num_beats - 1);
  
  // Convert to BPM
  smpl_t bpm = 60.0 * dp->samplerate / (avg_interval * dp->hop_s);
  
  return bpm;
}
```

### Integration with Tempogram

The DP tracker can use tempogram as observation model:

```c
// In aubio_beattracking_do() when use_dp is enabled:
if (bt->use_dp && bt->tempogram) {
  // Get tempo from tempogram
  smpl_t tempo_bpm = aubio_tempogram_get_tempo(bt->tempogram, ...);
  
  // Update DP tempo model
  aubio_dptracker_set_tempo(bt->dptracker, tempo_bpm, 5.0);
  
  // Feed onset to DP tracker
  aubio_dptracker_do(bt->dptracker, onset_value);
  
  // Get beat from DP
  aubio_dptracker_get_beats(bt->dptracker, output);
}
```

## Performance Considerations

### Computational Complexity

- **Per-frame cost**: O(W) where W = max_interval - min_interval
- **Typical W**: ~100-200 frames (for 30-240 BPM range)
- **Total cost**: ~10-20x higher than autocorrelation alone

### Memory Requirements

```
DP buffers = 2 × win_s × sizeof(smpl_t)  (dp_score + onset_buffer)
           + 2 × win_s × sizeof(uint_t)  (dp_backptr + beat_sequence)

Example (win_s=512): ~6 KB additional memory
```

### Optimization Strategies

1. **Bounded Search**: Only search within tempo_mean ± 2*tempo_std
2. **Pruning**: Skip DP updates for low-confidence onsets
3. **Lazy Backtracking**: Only extract beat sequence when needed
4. **SIMD**: Vectorize penalty computation loop

## Testing Strategy

### Unit Tests

#### test-dptracker-basic.c
- DP tracker creation/deletion
- Onset feeding
- Score accumulation
- Backpointer updates

#### test-dptracker-synthetic.c
- Synthetic beat sequences (80, 100, 120, 140, 160 BPM)
- Verify beat recovery from simulated onsets
- Tempo estimation accuracy

#### test-dptracker-penalty.c
- Verify penalty function: P_δ̂(δ) = -[log₂(δ/δ̂)]²
- Symmetry test: P(2×δ̂) == P(0.5×δ̂)
- Minimum at δ = δ̂

### Integration Tests

#### test-tempo-dp.c
- Full integration: tempo → beattracking → dptracker
- Compare DP vs autocorrelation vs tempogram
- Test on test_bpm_changes.wav

#### test-tempo-dp-gradual.c
- Gradual tempo changes (test_bpm_gradual.wav)
- Accelerando tracking
- Ritardando tracking

### Benchmark Tests

#### test-tempo-benchmark-dp.c
```c
// Test metrics:
// - Detection rate (should improve to >80%)
// - BPM accuracy (target <1.5 BPM avg error)
// - Response time (may increase to 7-8s due to DP window)
// - CPU usage (expect ~10-20x autocorr baseline)
```

## Benchmarking Plan

### Phase 1: Baseline Comparison

Run on test_bpm_changes.wav (6 sections, sudden changes):

| Method | Detection | Avg Error | Response | CPU |
|--------|-----------|-----------|----------|-----|
| Autocorrelation | 100% (6/6) | 1.66 BPM | 2.40s | 1.0× |
| Tempogram (multi-scale) | 50% (3/6) | 2.06 BPM | 1.71s | 3.0× |
| **DP (target)** | **>80% (5/6)** | **<1.5 BPM** | **<8s** | **~10× est** |

### Phase 2: Gradual Tempo Changes

Run on test_bpm_gradual.wav (4 sections, accelerando/ritardando):

| Method | Tracking Quality | Smoothness | Accuracy |
|--------|-----------------|------------|----------|
| Autocorrelation | Good (steps) | Low | High |
| Tempogram + PLP | Good (smooth) | High | Medium |
| **DP (target)** | **Excellent** | **High** | **High** |

### Phase 3: Real-World Music

Test on diverse music:
- Electronic (strict tempo) → Expect excellent performance
- Classical (rubato) → Test gradual change handling
- Live performance (imperfect timing) → Test robustness

## Expected Results

### Advantages over Current Methods

1. **Global Optimality**: Finds best overall beat sequence, not greedy
2. **Tempo Continuity**: Natural handling of gradual tempo changes
3. **Robustness**: Less sensitive to single onset errors
4. **Flexibility**: Works with any observation model (autocorr, tempogram, etc.)

### Known Limitations

1. **Latency**: Requires buffering (win_s frames) before final decision
2. **CPU Cost**: 10-20× higher than simple autocorrelation
3. **Memory**: Additional ~6KB for DP buffers
4. **Constant Tempo Assumption**: Best for roughly steady tempo

## Implementation Phases

### Phase 1: Core DP Infrastructure (Session 2)
- [ ] Create dptracker.h with API declarations
- [ ] Implement new_aubio_dptracker() / del_aubio_dptracker()
- [ ] Implement penalty function
- [ ] Implement aubio_dptracker_do() (DP loop)
- [ ] Unit tests: test-dptracker-basic.c

### Phase 2: Beat Extraction (Session 2)
- [ ] Implement aubio_dptracker_get_beats() (Viterbi backtrack)
- [ ] Implement aubio_dptracker_get_bpm()
- [ ] Implement aubio_dptracker_get_confidence()
- [ ] Unit tests: test-dptracker-synthetic.c

### Phase 3: Integration (Session 3)
- [ ] Add dptracker to aubio_beattracking_t
- [ ] Implement aubio_beattracking_set_use_dp()
- [ ] Implement aubio_tempo_set_use_dp()
- [ ] Connect with tempogram observation model
- [ ] Integration test: test-tempo-dp.c

### Phase 4: Optimization & Benchmarking (Session 4)
- [ ] Profile performance
- [ ] Optimize search bounds
- [ ] Add SIMD optimizations (if needed)
- [ ] Benchmark: test-tempo-benchmark-dp.c
- [ ] Compare all methods on test suite

### Phase 5: Documentation (Session 5)
- [ ] Update TEMPO_WORK_SUMMARY.md
- [ ] Create usage examples
- [ ] Document when to use DP vs other methods
- [ ] Performance recommendations
- [ ] Final validation and testing

## Usage Recommendations (Planned)

### When to Use DP Tracking

**Best for:**
- Music analysis applications (offline processing)
- Classical music with gradual tempo changes
- Live recordings with imperfect timing
- Applications prioritizing accuracy over latency

**Not recommended for:**
- Real-time beat visualization (use autocorrelation)
- Resource-constrained devices (use autocorrelation)
- Electronic music with perfect timing (autocorr is sufficient)

### Example Usage

```python
from aubio import tempo, source

# Create tempo detector
t = tempo("default", 1024, 256, 44100)

# Enable DP tracking for optimal beat sequence
t.set_use_dp(1)

# Set tempo prior (helps DP performance)
t.set_dp_tempo_prior(120.0, 10.0)  # Expect 120±10 BPM

# Process audio
while True:
    samples, read = src()
    is_beat = t(samples)
    if is_beat:
        print(f"Beat at {t.get_last_beat_s():.2f}s, BPM: {t.get_bpm():.1f}")
    if read < hop_size:
        break
```

## Security Considerations

All code follows SECURITY/DEFENSIVE_PROGRAMMING.md:

```c
// Input validation
AUBIO_ASSERT_NOT_NULL(dp);
AUBIO_ASSERT_BOUNDS(i, dp->win_s);
AUBIO_ASSERT_RANGE(tempo_bpm, 30.0, 300.0);

// Memory safety
if (!dp->dp_score) goto beach;

// Proper cleanup
beach:
  if (dp->dp_score) del_fvec(dp->dp_score);
  if (dp->dp_backptr) del_lvec(dp->dp_backptr);
  AUBIO_FREE(dp);
  return NULL;
```

## References

1. Ellis, D. P. W. (2007). "Beat Tracking by Dynamic Programming". Journal of New Music Research, 36(1), 51-60.
2. Müller, M. "Fundamentals of Music Processing" - Section 6.3.2 Beat Tracking
3. Columbia University ELEN E4896 Music Signal Processing - Lecture 10
4. https://www.audiolabs-erlangen.de/resources/MIR/FMP/C6/C6S3_BeatTracking.html

---

**Document Version**: 1.0  
**Created**: 2025-11-18  
**Status**: Design Phase - Not Yet Implemented
