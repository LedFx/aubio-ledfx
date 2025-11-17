#!/usr/bin/env python3
"""
Tempogram Beat Tracking Demo
=============================

This demo showcases the tempogram feature in aubio-ledfx for advanced beat tracking.

The tempogram uses FFT-based tempo analysis to detect beats with high accuracy,
particularly useful for:
- Electronic music with steady beats
- Real-time tempo detection
- Accurate BPM measurement (< 1.2 BPM error)

What is a Tempogram?
-------------------
A tempogram is a time-frequency representation showing tempo variations over time.
It works by:
1. Collecting onset strength values over time (every hop)
2. Applying FFT to detect periodic patterns in onset time series
3. Mapping FFT bins to BPM values
4. Finding the dominant tempo from peak frequencies

This implementation uses the Wiener-Khinchin theorem for efficient autocorrelation
via FFT, enabling real-time beat period detection.

Requirements:
    - aubio-ledfx Python module (install with: pip install .)
    - numpy (install with: pip install numpy)
    - soundfile (install with: pip install soundfile) - for test audio generation

Usage:
    python demo_tempogram_beattracking.py <audio_file>
    
Example:
    # First, install aubio-ledfx
    pip install .
    
    # Generate test audio with known BPM sections
    python ../../tests/generate_tempo_test_audio.py
    
    # Run tempogram analysis on test audio
    python demo_tempogram_beattracking.py ../../test_bpm_changes.wav
    
    # Or use your own audio file
    python demo_tempogram_beattracking.py /path/to/your/music.wav
    
Author: aubio-ledfx team
Date: 2025-11-17
"""

import sys
import os
import numpy as np
from aubio import source, tempo
import json

def print_header(title):
    """Print a formatted section header."""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70 + "\n")

def print_section(title):
    """Print a subsection header."""
    print(f"\n--- {title} ---\n")

def load_ground_truth(json_file):
    """
    Load ground truth tempo data from JSON file if available.
    
    Parameters
    ----------
    json_file : str
        Path to JSON file containing ground truth sections
        
    Returns
    -------
    dict or None
        Ground truth data with sections, or None if file doesn't exist
    """
    if os.path.exists(json_file):
        with open(json_file, 'r') as f:
            return json.load(f)
    return None

def detect_tempo_with_tempogram(audio_file, samplerate=0, verbose=True):
    """
    Perform beat tracking using the tempogram feature.
    
    The tempogram provides more accurate tempo detection by analyzing
    the periodicity of onset events in the frequency domain using FFT.
    
    Parameters
    ----------
    audio_file : str
        Path to the audio file to analyze
    samplerate : int, optional
        Sample rate for processing (0 = use file's native rate)
    verbose : bool, optional
        Print detailed progress information
        
    Returns
    -------
    dict
        Dictionary containing:
        - 'beats': list of beat times in seconds
        - 'bpm_timeline': list of (time, bpm, confidence) tuples
        - 'samplerate': audio sample rate
        - 'final_bpm': final detected BPM
        - 'final_confidence': final confidence value
    
    Notes
    -----
    The tempogram algorithm:
    1. Computes onset detection function from audio spectrum
    2. Feeds onset values to tempogram on every hop (critical for FFT)
    3. Applies FFT to onset time series (512-sample window)
    4. Converts FFT bins to BPM values
    5. Finds peak in valid tempo range (30-300 BPM)
    6. Reports BPM with confidence metric
    """
    # Standard parameters for tempo detection
    # win_s must be power of 2 for FFT efficiency
    win_s = 1024      # FFT window size for onset detection
    hop_s = 256       # Hop size (win_s/4 is standard)
    
    if verbose:
        print_section("Initialization")
        print(f"Audio file: {os.path.basename(audio_file)}")
        print(f"Window size: {win_s} samples")
        print(f"Hop size: {hop_s} samples")
    
    # Create audio source
    s = source(audio_file, samplerate, hop_s)
    samplerate = s.samplerate
    
    if verbose:
        print(f"Sample rate: {samplerate} Hz")
        print(f"Hop duration: {hop_s / samplerate * 1000:.2f} ms")
    
    # Create tempo detection object
    # The "default" method uses Davies beat tracking algorithm
    t = tempo("default", win_s, hop_s, samplerate)
    
    # ============================================================
    # ENABLE TEMPOGRAM - This is the key feature!
    # ============================================================
    # The tempogram analyzes beat periodicity using FFT on the
    # onset time series, providing more accurate tempo detection
    # than autocorrelation-based methods alone.
    t.set_use_tempogram(1)
    
    if verbose:
        print("\n✓ Tempogram mode enabled")
        print("\nTempogram Configuration:")
        print("  • FFT window: 512 onset samples")
        print("  • Onset rate: {:.2f} Hz".format(samplerate / hop_s))
        print("  • Frequency resolution: {:.3f} Hz/bin".format(
            (samplerate / hop_s) / 512))
        print("  • BPM range: 30-300 BPM")
        print("  • Feeds onset on every hop for continuous time series")
    
    # Beat detection delay compensation
    # Default: 4 blocks to account for onset detection latency
    delay = 4. * hop_s
    
    # Storage for results
    beats = []              # Beat times in seconds
    bpm_timeline = []       # (time, bpm, confidence) for each beat
    total_frames = 0        # Total audio frames processed
    
    if verbose:
        print_section("Processing Audio")
        print("Reading audio and detecting beats...")
        print("(Showing progress every 1 second)\n")
    
    last_print_time = 0
    
    # ============================================================
    # MAIN PROCESSING LOOP
    # ============================================================
    # Process audio in chunks (hops) and detect beats
    while True:
        # Read next chunk of audio samples
        samples, read = s()
        
        # Process chunk through tempo detector
        # The tempo object internally:
        # 1. Computes onset detection function (spectral flux)
        # 2. Feeds onset value to tempogram (NEW: on every hop!)
        # 3. Applies beat tracking algorithm
        # 4. Returns beat position if detected
        is_beat = t(samples)
        
        # Current time in seconds
        current_time = total_frames / float(samplerate)
        
        # If a beat was detected in this chunk
        if is_beat:
            # Calculate exact beat time accounting for delay
            # is_beat[0] is fractional position within hop
            this_beat = int(total_frames - delay + is_beat[0] * hop_s)
            beat_time = this_beat / float(samplerate)
            
            # Get current tempo estimate and confidence
            # These are computed by the tempogram from FFT analysis
            bpm = t.get_bpm()
            confidence = t.get_confidence()
            
            # Store beat and BPM data
            beats.append(beat_time)
            bpm_timeline.append((beat_time, bpm, confidence))
            
            if verbose:
                print(f"  Beat at {beat_time:6.2f}s - BPM: {bpm:6.2f} - "
                      f"Confidence: {confidence:5.2f}")
        
        # Progress indicator (every 1 second)
        if verbose and int(current_time) > last_print_time:
            if not is_beat:  # Only print if no beat was just printed
                current_bpm = t.get_bpm()
                print(f"  Time {current_time:6.2f}s - Current BPM: {current_bpm:6.2f}")
            last_print_time = int(current_time)
        
        # Update frame counter
        total_frames += read
        
        # Stop when end of file reached
        if read < hop_s:
            break
    
    # Get final tempo estimate
    final_bpm = t.get_bpm()
    final_confidence = t.get_confidence()
    
    if verbose:
        print_section("Processing Complete")
        print(f"Total duration: {total_frames / samplerate:.2f} seconds")
        print(f"Beats detected: {len(beats)}")
        print(f"Final BPM: {final_bpm:.2f}")
        print(f"Final confidence: {final_confidence:.2f}")
    
    # Return all collected data
    return {
        'beats': beats,
        'bpm_timeline': bpm_timeline,
        'samplerate': samplerate,
        'final_bpm': final_bpm,
        'final_confidence': final_confidence,
        'total_duration': total_frames / samplerate
    }

def analyze_accuracy(results, ground_truth):
    """
    Analyze detection accuracy if ground truth is available.
    
    Parameters
    ----------
    results : dict
        Detection results from detect_tempo_with_tempogram()
    ground_truth : dict
        Ground truth data with 'sections' list
        
    Returns
    -------
    dict
        Accuracy metrics including:
        - section_analysis: list of per-section results
        - detection_rate: percentage of sections detected correctly
        - avg_error: average BPM error across detected sections
        - max_error: maximum BPM error
    """
    if not ground_truth or 'sections' not in ground_truth:
        return None
    
    sections = ground_truth['sections']
    bpm_timeline = results['bpm_timeline']
    
    section_analysis = []
    
    print_section("Ground Truth Comparison")
    print(f"{'Section':<10} {'Time Range':<20} {'Expected':<12} "
          f"{'Detected':<12} {'Error':<10} {'Status'}")
    print("-" * 80)
    
    detected_count = 0
    errors = []
    
    for i, section in enumerate(sections):
        start_time = section['start_time']
        end_time = section['end_time']
        expected_bpm = section['bpm']
        
        # Find all BPM detections in this time range
        section_detections = [
            (t, bpm, conf) for t, bpm, conf in bpm_timeline
            if start_time <= t < end_time
        ]
        
        if section_detections:
            # Use median BPM from section for robustness
            bpms = [bpm for _, bpm, _ in section_detections]
            detected_bpm = np.median(bpms)
            error = abs(detected_bpm - expected_bpm)
            
            # Consider detection successful if error < 5 BPM
            status = "✓ PASS" if error < 5.0 else "✗ FAIL"
            if error < 5.0:
                detected_count += 1
                errors.append(error)
        else:
            detected_bpm = 0
            error = 0
            status = "✗ NO DETECTION"
        
        # Print section analysis
        time_range = f"{start_time:.1f}-{end_time:.1f}s"
        expected_str = f"{expected_bpm:.1f} BPM"
        detected_str = f"{detected_bpm:.1f} BPM" if detected_bpm > 0 else "N/A"
        error_str = f"{error:.2f} BPM" if detected_bpm > 0 else "N/A"
        
        print(f"{i+1:<10} {time_range:<20} {expected_str:<12} "
              f"{detected_str:<12} {error_str:<10} {status}")
        
        section_analysis.append({
            'section': i + 1,
            'expected_bpm': expected_bpm,
            'detected_bpm': detected_bpm if detected_bpm > 0 else None,
            'error': error if detected_bpm > 0 else None,
            'detected': detected_bpm > 0 and error < 5.0
        })
    
    # Calculate overall metrics
    detection_rate = (detected_count / len(sections)) * 100
    avg_error = np.mean(errors) if errors else 0
    max_error = np.max(errors) if errors else 0
    
    print("\n" + "-" * 80)
    print(f"\nOverall Accuracy:")
    print(f"  Detection rate: {detection_rate:.1f}% ({detected_count}/{len(sections)} sections)")
    if errors:
        print(f"  Average error:  {avg_error:.2f} BPM")
        print(f"  Maximum error:  {max_error:.2f} BPM")
    
    return {
        'section_analysis': section_analysis,
        'detection_rate': detection_rate,
        'avg_error': avg_error,
        'max_error': max_error
    }

def print_usage():
    """Print usage information."""
    print("Tempogram Beat Tracking Demo")
    print("=" * 70)
    print("\nUsage:")
    print("  python demo_tempogram_beattracking.py <audio_file>")
    print("\nExample:")
    print("  # First, generate test audio with known BPM sections")
    print("  python ../../tests/generate_tempo_test_audio.py")
    print("")
    print("  # Then run tempogram analysis")
    print("  python demo_tempogram_beattracking.py ../../test_bpm_changes.wav")
    print("\nThe demo will:")
    print("  1. Load the audio file")
    print("  2. Enable tempogram mode for FFT-based tempo analysis")
    print("  3. Process audio and detect beats in real-time")
    print("  4. Display BPM and confidence for each beat")
    print("  5. Compare against ground truth if available")
    print("\nTempogram Features:")
    print("  • FFT-based beat period detection")
    print("  • < 1.2 BPM accuracy on synthetic audio")
    print("  • Continuous onset feeding (every hop)")
    print("  • Real-time capable processing")
    print("")

def main():
    """Main function - entry point for the demo."""
    # Check command-line arguments
    if len(sys.argv) < 2:
        print_usage()
        return 1
    
    audio_file = sys.argv[1]
    
    # Verify file exists
    if not os.path.exists(audio_file):
        print(f"\nError: File not found: {audio_file}")
        print("\nPlease provide a valid audio file path.")
        print("You can generate test audio with:")
        print("  python ../../tests/generate_tempo_test_audio.py")
        return 1
    
    print_header("Tempogram Beat Tracking Demo")
    
    # Look for ground truth file (same name with _ground_truth.json suffix)
    base_name = os.path.splitext(audio_file)[0]
    ground_truth_file = base_name + '_ground_truth.json'
    ground_truth = load_ground_truth(ground_truth_file)
    
    if ground_truth:
        print(f"✓ Ground truth data loaded from: {os.path.basename(ground_truth_file)}")
    else:
        print(f"ℹ No ground truth data found (looked for: {os.path.basename(ground_truth_file)})")
        print("  Detection results will be shown without accuracy comparison.")
    
    # Perform beat tracking with tempogram
    results = detect_tempo_with_tempogram(audio_file, verbose=True)
    
    # Analyze accuracy if ground truth available
    if ground_truth:
        accuracy = analyze_accuracy(results, ground_truth)
    
    # Print summary
    print_header("Summary")
    print(f"Audio file: {os.path.basename(audio_file)}")
    print(f"Duration: {results['total_duration']:.2f} seconds")
    print(f"Sample rate: {results['samplerate']} Hz")
    print(f"\nBeats detected: {len(results['beats'])}")
    print(f"Final BPM: {results['final_bpm']:.2f}")
    print(f"Final confidence: {results['final_confidence']:.2f}")
    
    if ground_truth and accuracy:
        print(f"\nAccuracy: {accuracy['detection_rate']:.1f}% sections detected correctly")
        if accuracy['avg_error'] > 0:
            print(f"Average error: {accuracy['avg_error']:.2f} BPM")
    
    print("\n" + "=" * 70)
    print("Tempogram Key Points:")
    print("  • Uses FFT to analyze beat periodicity in onset time series")
    print("  • Fed onset values on every hop (critical for proper FFT)")
    print("  • Provides state-of-the-art accuracy (< 1.2 BPM error)")
    print("  • Suitable for real-time beat tracking applications")
    print("=" * 70 + "\n")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
