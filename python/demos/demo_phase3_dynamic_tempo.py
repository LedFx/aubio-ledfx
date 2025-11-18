#!/usr/bin/env python3
"""
Phase 3 Dynamic Tempo Tracking Demo

Demonstrates the new dynamic tempo tracking features:
- Frame-by-frame tempo estimation
- Instantaneous tempo (unsmoothed)
- Tempo variance for stability analysis
- Tempo change detection

This compares static vs dynamic tempo tracking modes.
"""

import sys
import numpy as np
from aubio import tempo, source

def analyze_with_dynamic_tempo(audio_file):
    """Analyze audio with Phase 3 dynamic tempo tracking enabled"""
    
    # Create tempo detector
    win_s, hop_s = 1024, 256
    src = source(audio_file, hop_size=hop_s)
    samplerate = src.samplerate
    
    t = tempo("default", win_s, hop_s, samplerate)
    
    # Enable Phase 3 features
    t.set_dynamic_tempo(1)  # Enable dynamic tracking
    t.set_multi_octave(1)   # Multi-octave detection
    t.set_tempo_prior_std(2.5)  # Allow tempo variation
    
    # Storage for analysis
    timestamps = []
    smoothed_bpms = []
    instantaneous_bpms = []
    variances = []
    confidences = []
    
    total_frames = 0
    
    print(f"\nAnalyzing: {audio_file}")
    print(f"Sample rate: {samplerate} Hz")
    print("\nPhase 3 Features Enabled:")
    print("  ✓ Dynamic tempo tracking")
    print("  ✓ Multi-octave detection")
    print("  ✓ Instantaneous BPM monitoring")
    print("  ✓ Tempo variance calculation\n")
    
    # Process audio
    while True:
        samples, read = src()
        is_beat = t(samples)
        
        total_frames += read
        current_time = total_frames / float(samplerate)
        
        # Get all tempo metrics
        smoothed_bpm = t.get_bpm()
        instantaneous_bpm = t.get_instantaneous_bpm()
        variance = t.get_tempo_variance()
        confidence = t.get_confidence()
        
        # Store every second for analysis
        if int(current_time) != int(current_time - hop_s / samplerate):
            timestamps.append(current_time)
            smoothed_bpms.append(smoothed_bpm)
            instantaneous_bpms.append(instantaneous_bpm)
            variances.append(variance)
            confidences.append(confidence)
            
        if read < hop_s:
            break
    
    return {
        'timestamps': np.array(timestamps),
        'smoothed_bpm': np.array(smoothed_bpms),
        'instantaneous_bpm': np.array(instantaneous_bpms),
        'variance': np.array(variances),
        'confidence': np.array(confidences)
    }


def analyze_static_mode(audio_file):
    """Analyze with traditional static tempo tracking"""
    
    win_s, hop_s = 1024, 256
    src = source(audio_file, hop_size=hop_s)
    samplerate = src.samplerate
    
    t = tempo("default", win_s, hop_s, samplerate)
    # Dynamic tempo is disabled by default
    
    timestamps = []
    bpms = []
    confidences = []
    
    total_frames = 0
    
    while True:
        samples, read = src()
        is_beat = t(samples)
        
        total_frames += read
        current_time = total_frames / float(samplerate)
        
        if int(current_time) != int(current_time - hop_s / samplerate):
            timestamps.append(current_time)
            bpms.append(t.get_bpm())
            confidences.append(t.get_confidence())
        
        if read < hop_s:
            break
    
    return {
        'timestamps': np.array(timestamps),
        'bpm': np.array(bpms),
        'confidence': np.array(confidences)
    }


def print_comparison(static_results, dynamic_results):
    """Print comparison of static vs dynamic tempo tracking"""
    
    print("\n" + "="*80)
    print("PHASE 3 DYNAMIC TEMPO TRACKING COMPARISON")
    print("="*80)
    
    # Filter out zero values for statistics
    static_bpm = static_results['bpm'][static_results['bpm'] > 0]
    smoothed_bpm = dynamic_results['smoothed_bpm'][dynamic_results['smoothed_bpm'] > 0]
    inst_bpm = dynamic_results['instantaneous_bpm'][dynamic_results['instantaneous_bpm'] > 0]
    
    print("\n1. TEMPO STATISTICS")
    print("-" * 80)
    print(f"{'Metric':<30} {'Static Mode':<20} {'Dynamic (Smoothed)':<20} {'Dynamic (Instant)':<20}")
    print("-" * 80)
    
    if len(static_bpm) > 0:
        print(f"{'Mean BPM':<30} {np.mean(static_bpm):>18.2f}  {np.mean(smoothed_bpm):>18.2f}  {np.mean(inst_bpm):>18.2f}")
        print(f"{'Std Deviation':<30} {np.std(static_bpm):>18.2f}  {np.std(smoothed_bpm):>18.2f}  {np.std(inst_bpm):>18.2f}")
        print(f"{'Min BPM':<30} {np.min(static_bpm):>18.2f}  {np.min(smoothed_bpm):>18.2f}  {np.min(inst_bpm):>18.2f}")
        print(f"{'Max BPM':<30} {np.max(static_bpm):>18.2f}  {np.max(smoothed_bpm):>18.2f}  {np.max(inst_bpm):>18.2f}")
    
    print("\n2. PHASE 3 DYNAMIC FEATURES")
    print("-" * 80)
    
    variance = dynamic_results['variance'][dynamic_results['variance'] > 0]
    if len(variance) > 0:
        print(f"Tempo Variance (mean): {np.mean(variance):.2f} BPM²")
        print(f"Tempo Variance (max):  {np.max(variance):.2f} BPM²")
        print(f"Tempo Stability: {'High' if np.mean(variance) < 10 else 'Medium' if np.mean(variance) < 50 else 'Low'}")
    
    # Detect tempo changes
    if len(inst_bpm) > 1:
        tempo_changes = np.abs(np.diff(inst_bpm))
        significant_changes = tempo_changes > 5.0
        print(f"Tempo Changes Detected: {np.sum(significant_changes)} significant changes (>5 BPM)")
        
        if np.sum(significant_changes) > 0:
            change_indices = np.where(significant_changes)[0]
            print("\nTempo Change Events:")
            for idx in change_indices[:5]:  # Show first 5
                time = dynamic_results['timestamps'][idx]
                from_bpm = inst_bpm[idx]
                to_bpm = inst_bpm[idx + 1]
                print(f"  {time:.1f}s: {from_bpm:.1f} → {to_bpm:.1f} BPM ({to_bpm - from_bpm:+.1f})")
    
    print("\n3. RESPONSIVENESS")
    print("-" * 80)
    print("Instantaneous BPM shows tempo WITHOUT smoothing")
    print("Smoothed BPM applies confidence-weighted averaging")
    print(f"Smoothing reduces jitter by ~{100 * (1 - np.std(smoothed_bpm) / np.std(inst_bpm)):.0f}%")
    
    print("\n4. FEATURE BENEFITS")
    print("-" * 80)
    print("✓ Frame-by-frame tempo estimates (not just aggregated)")
    print("✓ Instantaneous tempo for rapid change detection")
    print("✓ Variance calculation for stability analysis")
    print("✓ Tempo change event detection")
    print("✓ Better visualization of time-varying tempo")
    
    print("\n" + "="*80)


def main():
    if len(sys.argv) < 2:
        print("Usage: python demo_phase3_dynamic_tempo.py <audio_file>")
        print("\nRecommended test files:")
        print("  - tests/test_bpm_changes.wav (sudden tempo changes)")
        print("  - tests/test_bpm_gradual.wav (accelerando/ritardando)")
        sys.exit(1)
    
    audio_file = sys.argv[1]
    
    try:
        print("\n" + "="*80)
        print("PHASE 3: DYNAMIC TEMPO TRACKING DEMONSTRATION")
        print("="*80)
        
        # Analyze with both modes
        print("\n[1/2] Running static tempo tracking...")
        static_results = analyze_static_mode(audio_file)
        
        print("[2/2] Running dynamic tempo tracking (Phase 3)...")
        dynamic_results = analyze_with_dynamic_tempo(audio_file)
        
        # Print comparison
        print_comparison(static_results, dynamic_results)
        
        print("\nPHASE 3 APIS USED:")
        print("  - tempo.set_dynamic_tempo(1)")
        print("  - tempo.get_instantaneous_bpm()")
        print("  - tempo.get_tempo_variance()")
        print("\nSee python/docs/TEMPO_IMPROVEMENTS.md for full API documentation")
        
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
