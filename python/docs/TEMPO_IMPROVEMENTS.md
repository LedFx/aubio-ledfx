# Improved Tempo Tracking Features

## Overview

This fork includes enhanced tempo tracking capabilities inspired by librosa's beat tracking methodology. These improvements provide better accuracy, stability, and configurability for real-time tempo detection.

## New Features

### 1. Tempo Prior Configuration

Set expected tempo range for genre-specific optimization:

```python
from aubio import tempo, source

# Create tempo detector
t = tempo("default", 1024, 256, 44100)

# Configure for Electronic Dance Music (strict 128 BPM)
t.set_tempo_prior_mean(128.0)
t.set_tempo_prior_std(0.5)  # Tight tolerance

# Or configure for variable tempo classical music
t.set_tempo_prior_mean(100.0)
t.set_tempo_prior_std(3.0)  # Allow natural fluctuations
```

**Benefits:**
- Centers tempo detection around expected BPM
- Improves accuracy for genre-specific music
- Reduces false detections outside expected range

### 2. Adaptive Window Sizing

Enable faster response to tempo changes:

```python
t = tempo("default", 1024, 256, 44100)

# Enable adaptive window sizing
t.set_adaptive_winlen(1)  # 1 = enabled, 0 = disabled

# Now process audio - window size reduces when tempo is stable
# This allows faster response to tempo changes (5s instead of 6s)
```

**How it works:**
- When confidence > 0.6, analysis window is reduced by half
- Faster updates during stable tempo periods
- Maintains accuracy while improving responsiveness

### 3. Enhanced Confidence Metrics

Get more reliable confidence values:

```python
t = tempo("default", 1024, 256, 44100)

while True:
    samples, read = source_obj()
    is_beat = t(samples)
    
    if is_beat:
        bpm = t.get_bpm()
        confidence = t.get_confidence()  # Now cached for efficiency
        
        # High confidence (>0.8) = very stable tempo
        # Low confidence (<0.3) = uncertain or changing tempo
        print(f"BPM: {bpm:.1f}, Confidence: {confidence:.2f}")
```

## Performance Improvements

Based on benchmarks with test audio containing tempo changes:

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Avg BPM Error | 0.78 BPM | 0.72 BPM | -8% |
| Max BPM Error | 1.35 BPM | 1.07 BPM | -21% |
| BPM Jitter | baseline | - | -30% |
| Response Time | 6.34s | 5.22s | -18% |

## Complete Example

```python
#!/usr/bin/env python3
from aubio import tempo, source
import sys

def analyze_tempo(audio_file, genre='default'):
    """
    Analyze tempo with genre-specific optimization.
    
    Args:
        audio_file: Path to audio file
        genre: 'edm', 'classical', 'hiphop', or 'default'
    """
    win_s = 1024
    hop_s = 256
    
    # Open audio source
    s = source(audio_file, 0, hop_s)
    samplerate = s.samplerate
    
    # Create tempo detector
    t = tempo("default", win_s, hop_s, samplerate)
    
    # Configure based on genre
    if genre == 'edm':
        t.set_tempo_prior_mean(128.0)
        t.set_tempo_prior_std(0.5)  # Strict
    elif genre == 'hiphop':
        t.set_tempo_prior_mean(95.0)
        t.set_tempo_prior_std(1.5)
    elif genre == 'classical':
        t.set_tempo_prior_mean(100.0)
        t.set_tempo_prior_std(3.0)  # Flexible
    
    # Enable adaptive window for faster response
    t.set_adaptive_winlen(1)
    
    # Process audio
    beats = []
    total_frames = 0
    
    while True:
        samples, read = s()
        is_beat = t(samples)
        
        if is_beat:
            beat_time = total_frames / float(samplerate)
            bpm = t.get_bpm()
            confidence = t.get_confidence()
            beats.append((beat_time, bpm, confidence))
            print(f"{beat_time:.2f}s: {bpm:.1f} BPM (confidence: {confidence:.2f})")
        
        total_frames += read
        if read < hop_s:
            break
    
    # Calculate average tempo
    if beats:
        bpms = [bpm for _, bpm, _ in beats]
        avg_bpm = sum(bpms) / len(bpms)
        print(f"\nAverage tempo: {avg_bpm:.1f} BPM ({len(beats)} beats detected)")
    else:
        print("\nNo beats detected")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python tempo_example.py <audio_file> [genre]")
        print("Genres: edm, hiphop, classical, default")
        sys.exit(1)
    
    audio_file = sys.argv[1]
    genre = sys.argv[2] if len(sys.argv) > 2 else 'default'
    
    analyze_tempo(audio_file, genre)
```

## API Reference

### tempo.set_tempo_prior_mean(bpm)

Set the expected tempo (in BPM) for prior distribution.

**Parameters:**
- `bpm` (float): Expected tempo in beats per minute (20-300 BPM)

**Returns:** 
- `0` on success, non-zero on error

**Example:**
```python
t.set_tempo_prior_mean(120.0)  # Expect ~120 BPM
```

### tempo.set_tempo_prior_std(std)

Set the tempo uncertainty (standard deviation in BPM).

**Parameters:**
- `std` (float): Standard deviation in BPM (0.1-10.0)
  - Smaller values (< 1.0): Strict tracking, minimal tempo variation
  - Larger values (> 2.0): Flexible tracking, allows tempo changes

**Returns:**
- `0` on success, non-zero on error

**Example:**
```python
t.set_tempo_prior_std(0.5)  # Very strict (±0.5 BPM)
t.set_tempo_prior_std(3.0)  # Flexible (±3 BPM)
```

### tempo.set_adaptive_winlen(enabled)

Enable or disable adaptive window sizing for faster tempo change response.

**Parameters:**
- `enabled` (int): `1` to enable, `0` to disable

**Returns:**
- `0` on success, non-zero on error

**Example:**
```python
t.set_adaptive_winlen(1)  # Enable adaptive window
```

## Use Case Recommendations

### Music Library Analysis
**Goal:** Accurate BPM tagging for large music collections

```python
# Use defaults - optimized for accuracy
t = tempo("default", 1024, 256, 44100)
# Process entire file, report final BPM
```

Expected: ±0.5 BPM accuracy for 90-180 BPM range

### Real-Time Visualization
**Goal:** Responsive tempo display for live visualizations

```python
t = tempo("default", 1024, 256, 44100)
t.set_tempo_prior_std(3.0)  # Allow variation
t.set_adaptive_winlen(1)    # Faster response
```

Expected: 5-6s initial detection, ~3s updates

### Genre-Specific Analysis

**Electronic Dance Music:**
```python
t.set_tempo_prior_mean(128.0)
t.set_tempo_prior_std(0.5)  # EDM is very consistent
```

**Hip-Hop:**
```python
t.set_tempo_prior_mean(95.0)
t.set_tempo_prior_std(1.5)  # Some variation
```

**Classical:**
```python
t.set_tempo_prior_mean(100.0)
t.set_tempo_prior_std(3.0)  # Natural tempo rubato
```

## Limitations

### Response Time
- Initial tempo detection: ~5-6 seconds
- This is inherent to the Davies algorithm (requires 5.8s analysis window)
- Adaptive window reduces subsequent updates to ~3s

### Slow Tempo Range
- Tempos below ~80 BPM may not be detected reliably
- Consider using tempo priors for slow music:
  ```python
  t.set_tempo_prior_mean(70.0)  # For slow ballads
  ```

### Very Fast Tempos
- Tempos above ~200 BPM may be detected as half-tempo
- Multi-octave analysis would be needed to address this

## Migration Guide

Existing code continues to work without changes:

```python
# Old code - still works exactly the same
t = tempo("default", 1024, 256, 44100)
# ... process audio
bpm = t.get_bpm()
```

To use new features, simply add configuration calls:

```python
# Enhanced code - same base, new features
t = tempo("default", 1024, 256, 44100)
t.set_tempo_prior_mean(120.0)  # NEW: Set expected tempo
t.set_adaptive_winlen(1)        # NEW: Enable fast response
# ... process audio same as before
bpm = t.get_bpm()
```

## See Also

- [demo_tempo_comparison.py](../demos/demo_tempo_comparison.py) - Compare original vs optimized modes
- [../../doc/tempo_benchmark_results.md](../../doc/tempo_benchmark_results.md) - Performance analysis
- [../../doc/TEMPO_IMPROVEMENTS_SUMMARY.md](../../doc/TEMPO_IMPROVEMENTS_SUMMARY.md) - Technical details
