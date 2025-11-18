#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Example: Improved Beat Tracking with Tempo Enhancements

Demonstrates the new tempo tracking features from Phase 1-3C:
- Phase 1: Tempo priors, adaptive smoothing, multi-octave analysis
- Phase 2: Tempogram FFT-based detection
- Phase 3A: Onset enhancement (median filtering + adaptive thresholding)
- Phase 3B: Multi-scale tempogram analysis
- Phase 3C: PLP temporal smoothing

Based on TEMPO_WORK_SUMMARY.md implementation.
"""

import sys
import aubio
import numpy as np


def example_basic_autocorrelation():
    """Example 1: Basic autocorrelation with optimizations (default, recommended)."""
    
    print("\n" + "="*70)
    print("Example 1: Basic Autocorrelation (Default - Recommended)")
    print("="*70)
    print("Use case: Live performance, DJ software, real-time applications")
    print("Performance: 83% detection, 0.51 BPM error, 1-3s response time")
    
    # Configuration
    win_s = 1024      # Window size
    hop_s = 256       # Hop size
    samplerate = 44100 # Sample rate
    
    # Create tempo object (autocorrelation is default)
    tempo = aubio.tempo("default", win_s, hop_s, samplerate)
    
    # Enable multi-octave analysis for better accuracy
    tempo.set_multi_octave(1)
    
    print("\nConfiguration:")
    print(f"  Method: Autocorrelation (default)")
    print(f"  Window size: {win_s} samples")
    print(f"  Hop size: {hop_s} samples")
    print(f"  Sample rate: {samplerate} Hz")
    print(f"  Multi-octave: Enabled")
    
    # Simulate processing some audio
    # In real use, you would read from an audio source
    print("\nProcessing audio...")
    for i in range(10):
        # Create dummy audio samples (in real use, read from source)
        samples = np.zeros(hop_s, dtype=aubio.float_type)
        
        # Process frame
        tempo(samples)
        
        # Get results
        bpm = tempo.get_bpm()
        confidence = tempo.get_confidence()
        
        if i == 9:  # Print last result
            print(f"  Final BPM: {bpm:.2f}")
            print(f"  Confidence: {confidence:.3f}")


def example_genre_specific():
    """Example 2: Genre-specific optimization with tempo priors."""
    
    print("\n" + "="*70)
    print("Example 2: Genre-Specific Optimization")
    print("="*70)
    print("Use case: Genre-aware applications (EDM, Classical, Hip-hop, etc.)")
    print("Benefit: Reduces false detections by biasing toward expected range")
    
    # EDM configuration
    tempo = aubio.tempo("default", 1024, 256, 44100)
    tempo.set_tempo_prior_mean(128.0)  # Expected BPM: 128
    tempo.set_tempo_prior_std(0.5)     # Tight range: ±0.5 BPM
    
    print("\nEDM Preset:")
    print(f"  Expected BPM: 128.0")
    print(f"  Uncertainty: ±0.5 BPM (tight range)")
    print(f"  Result: Locks onto 128 BPM quickly, rejects outliers")
    
    # Classical configuration
    tempo = aubio.tempo("default", 1024, 256, 44100)
    tempo.set_tempo_prior_mean(100.0)  # Expected BPM: 100
    tempo.set_tempo_prior_std(3.0)     # Wider range for rubato
    
    print("\nClassical Preset:")
    print(f"  Expected BPM: 100.0")
    print(f"  Uncertainty: ±3.0 BPM (wider range for rubato)")
    print(f"  Result: Accommodates gradual tempo changes")
    
    # Additional genre presets
    print("\nOther Genre Presets:")
    print(f"  Hip-hop: mean=90.0, std=2.0")
    print(f"  Drum & Bass: mean=174.0, std=4.0")


def example_tempogram_multiscale():
    """Example 3: Multi-scale tempogram for analysis."""
    
    print("\n" + "="*70)
    print("Example 3: Multi-Scale Tempogram Analysis")
    print("="*70)
    print("Use case: Post-processing, music analysis (>30s content), research")
    print("Performance: 50% detection, 2.06 BPM error, excellent accuracy when stable")
    
    # Create tempo object
    tempo = aubio.tempo("default", 1024, 256, 44100)
    
    # Enable tempogram with all enhancements
    tempo.set_use_tempogram(1)          # Phase 2: FFT-based tempo detection
    tempo.set_onset_enhancement(1)       # Phase 3A: Median filter + adaptive threshold
    tempo.set_multiscale_tempogram(1)    # Phase 3B: Short/medium/long scales
    
    # Also enable autocorr optimizations for fallback
    tempo.set_multi_octave(1)
    tempo.set_fft_autocorr(1)
    
    print("\nConfiguration:")
    print(f"  Method: Multi-scale tempogram")
    print(f"  Scales: short (256), medium (512), long (1024) samples")
    print(f"  Onset enhancement: Enabled (7-sample median filter)")
    print(f"  Weighted combination: Averages when scales agree")
    print(f"  Startup latency: 20-30 seconds for harmonic resolution")
    
    print("\nBest for:")
    print(f"  - Music analysis tools requiring smooth tempo curves")
    print(f"  - Post-processing where 30s latency is acceptable")
    print(f"  - Complex music where accuracy > speed")


def example_hybrid_approach():
    """Example 4: Hybrid autocorrelation → tempogram switching."""
    
    print("\n" + "="*70)
    print("Example 4: Hybrid Approach (Best Overall)")
    print("="*70)
    print("Use case: Professional applications needing quick start AND accuracy")
    print("Performance: 75-90% detection, best overall accuracy")
    
    # Create tempo object
    tempo = aubio.tempo("default", 1024, 256, 44100)
    
    # Start with autocorrelation optimizations
    tempo.set_multi_octave(1)
    tempo.set_fft_autocorr(1)
    
    print("\nPhase 1 (0-30 seconds): Autocorrelation")
    print(f"  - Quick startup (1-3s)")
    print(f"  - 83% detection rate")
    print(f"  - 0.51 BPM error")
    
    # After 30 seconds, switch to tempogram
    tempo.set_use_tempogram(1)
    tempo.set_multiscale_tempogram(1)
    tempo.set_onset_enhancement(1)
    
    print("\nPhase 2 (30+ seconds): Tempogram")
    print(f"  - Higher accuracy after stabilization")
    print(f"  - Better for complex/gradual tempo changes")
    print(f"  - Smooth tempo curves via PLP")
    
    print("\nResult: Fast startup + long-term accuracy")


def example_real_audio_processing():
    """Example 5: Real audio file processing."""
    
    print("\n" + "="*70)
    print("Example 5: Real Audio File Processing")
    print("="*70)
    
    # Check if test audio exists
    import os
    test_file = 'tests/test_bpm_changes.wav'
    
    if not os.path.exists(test_file):
        print(f"Test file not found: {test_file}")
        print("Skipping real audio example")
        return
    
    print(f"Processing: {test_file}")
    print("Audio contains 6 sections with tempo transitions:")
    print("  120 → 140 → 100 → 160 → 80 → 120 BPM")
    
    # Setup
    samplerate = 44100
    hop_size = 256
    
    # Create source and tempo
    source = aubio.source(test_file, samplerate, hop_size)
    tempo = aubio.tempo(buf_size=1024, hop_size=hop_size, samplerate=samplerate)
    
    # Enable optimizations
    tempo.set_multi_octave(1)
    
    print("\nProcessing audio...")
    
    # Track detections
    detections = []
    total_frames = 0
    
    while True:
        samples, read = source()
        tempo(samples)
        
        bpm = tempo.get_bpm()
        confidence = tempo.get_confidence()
        time_s = total_frames * hop_size / samplerate
        
        if confidence > 0.5 and bpm > 0:
            detections.append({
                'time': time_s,
                'bpm': bpm,
                'confidence': confidence
            })
        
        total_frames += 1
        if read < hop_size:
            break
    
    print(f"Processed {total_frames} frames ({total_frames * hop_size / samplerate:.1f}s)")
    print(f"Found {len(detections)} confident detections")
    
    if detections:
        # Show sample detections
        print("\nSample detections:")
        for i in range(min(5, len(detections))):
            d = detections[i * len(detections) // 5]
            print(f"  {d['time']:6.1f}s: {d['bpm']:6.2f} BPM (confidence: {d['confidence']:.3f})")


def print_performance_summary():
    """Print performance summary from TEMPO_WORK_SUMMARY.md."""
    
    print("\n" + "="*70)
    print("Performance Summary (from TEMPO_WORK_SUMMARY.md)")
    print("="*70)
    
    print("\nMethod Comparison:")
    print("-" * 70)
    print(f"{'Method':<25} {'Detection':<12} {'Avg Error':<12} {'Use Case':<20}")
    print("-" * 70)
    print(f"{'Autocorrelation':<25} {'83% (5/6)':<12} {'0.51 BPM':<12} {'Live, DJ, Games':<20}")
    print(f"{'Multi-Scale Tempogram':<25} {'50% (3/6)':<12} {'2.06 BPM':<12} {'Analysis':<20}")
    print(f"{'Hybrid (autocorr→temp)':<25} {'75-90%':<12} {'0.51-2.06':<12} {'Professional':<20}")
    
    print("\nKey Achievements:")
    print("  ✓ State-of-the-art: < 1 BPM error on clean signals")
    print("  ✓ Minimal jitter: ~30% reduction in BPM stability")
    print("  ✓ Fast response: 1-3s startup with autocorrelation")
    print("  ✓ Accurate analysis: 2.06 BPM error with tempogram")
    
    print("\nAll Features Available in Python:")
    print("  ✓ tempo.set_tempo_prior_mean(bpm)")
    print("  ✓ tempo.set_tempo_prior_std(std)")
    print("  ✓ tempo.set_adaptive_winlen(enabled)")
    print("  ✓ tempo.set_multi_octave(enabled)")
    print("  ✓ tempo.set_use_tempogram(enabled)")
    print("  ✓ tempo.set_onset_enhancement(enabled)")
    print("  ✓ tempo.set_multiscale_tempogram(enabled)")


def main():
    """Run all examples."""
    
    print("\n" + "="*70)
    print("AUBIO IMPROVED BEAT TRACKING EXAMPLES")
    print("="*70)
    print("\nBased on TEMPO_WORK_SUMMARY.md:")
    print("  - Phase 1: Core tempo improvements (priors, adaptive smoothing)")
    print("  - Phase 2: Tempogram implementation (FFT-based)")
    print("  - Phase 3A: Onset enhancement (median filter + threshold)")
    print("  - Phase 3B: Multi-scale tempogram analysis")
    print("  - Phase 3C: PLP temporal smoothing")
    
    # Run examples
    example_basic_autocorrelation()
    example_genre_specific()
    example_tempogram_multiscale()
    example_hybrid_approach()
    example_real_audio_processing()
    print_performance_summary()
    
    print("\n" + "="*70)
    print("For more information, see TEMPO_WORK_SUMMARY.md")
    print("="*70 + "\n")


if __name__ == '__main__':
    main()
