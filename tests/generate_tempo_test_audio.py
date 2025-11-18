#!/usr/bin/env python3
"""
Generate test audio with varying BPM sections for tempo tracking benchmarks.

This script creates audio files with known tempo changes to test:
1. BPM detection accuracy
2. Response time to tempo changes
3. Stability during constant tempo sections
"""

import numpy as np
import wave
import struct
import sys

def generate_click_track(bpm, duration_sec, sample_rate=44100, click_duration=0.01):
    """Generate a click track at a specific BPM.
    
    Args:
        bpm: Beats per minute
        duration_sec: Duration in seconds
        sample_rate: Sample rate in Hz
        click_duration: Duration of each click in seconds
    
    Returns:
        numpy array of audio samples
    """
    num_samples = int(duration_sec * sample_rate)
    audio = np.zeros(num_samples, dtype=np.float32)
    
    # Calculate samples per beat
    samples_per_beat = int(60.0 * sample_rate / bpm)
    click_samples = int(click_duration * sample_rate)
    
    # Generate clicks
    beat_positions = []
    pos = 0
    while pos < num_samples:
        beat_positions.append(pos)
        # Generate click as a short sine wave burst at 1000 Hz
        click_end = min(pos + click_samples, num_samples)
        t = np.arange(click_end - pos) / sample_rate
        click = np.sin(2 * np.pi * 1000 * t) * np.exp(-t * 100)
        audio[pos:click_end] = click
        pos += samples_per_beat
    
    return audio, beat_positions

def generate_onset_pattern(bpm, duration_sec, sample_rate=44100, pattern='kick'):
    """Generate realistic drum pattern at a specific BPM.
    
    Args:
        bpm: Beats per minute
        duration_sec: Duration in seconds
        sample_rate: Sample rate in Hz
        pattern: Type of pattern ('kick', 'snare', 'hihat', 'full')
    
    Returns:
        numpy array of audio samples
    """
    num_samples = int(duration_sec * sample_rate)
    audio = np.zeros(num_samples, dtype=np.float32)
    
    samples_per_beat = int(60.0 * sample_rate / bpm)
    
    # Generate drum sounds
    def make_kick():
        """Synthesize kick drum sound"""
        dur = 0.3
        t = np.linspace(0, dur, int(dur * sample_rate))
        freq = 60 * np.exp(-10 * t)  # Pitch envelope
        kick = np.sin(2 * np.pi * freq * t) * np.exp(-8 * t)
        return kick * 0.8
    
    def make_snare():
        """Synthesize snare drum sound"""
        dur = 0.15
        t = np.linspace(0, dur, int(dur * sample_rate))
        # Noise component
        noise = np.random.randn(len(t)) * np.exp(-15 * t)
        # Tonal component
        tone = np.sin(2 * np.pi * 200 * t) * np.exp(-20 * t)
        snare = (noise * 0.7 + tone * 0.3)
        return snare * 0.6
    
    def make_hihat():
        """Synthesize hi-hat sound"""
        dur = 0.05
        t = np.linspace(0, dur, int(dur * sample_rate))
        hihat = np.random.randn(len(t)) * np.exp(-50 * t)
        # High-pass filter effect
        hihat = hihat * (1 + np.sin(2 * np.pi * 8000 * t))
        return hihat * 0.3
    
    kick = make_kick()
    snare = make_snare()
    hihat = make_hihat()
    
    beat_positions = []
    beat_num = 0
    pos = 0
    
    while pos < num_samples:
        beat_in_bar = beat_num % 4
        
        # Place sounds based on pattern
        if pattern == 'kick' or pattern == 'full':
            # Kick on beats 1 and 3
            if beat_in_bar in [0, 2]:
                end = min(pos + len(kick), num_samples)
                audio[pos:end] += kick[:end-pos]
                if beat_in_bar == 0:
                    beat_positions.append(pos)
        
        if pattern == 'snare' or pattern == 'full':
            # Snare on beats 2 and 4
            if beat_in_bar in [1, 3]:
                end = min(pos + len(snare), num_samples)
                audio[pos:end] += snare[:end-pos]
        
        if pattern == 'hihat' or pattern == 'full':
            # Hi-hat on every eighth note
            for eighth in range(2):
                hihat_pos = pos + eighth * samples_per_beat // 2
                if hihat_pos < num_samples:
                    end = min(hihat_pos + len(hihat), num_samples)
                    audio[hihat_pos:end] += hihat[:end-hihat_pos]
        
        pos += samples_per_beat
        beat_num += 1
    
    return audio, beat_positions

def write_wav(filename, audio, sample_rate=44100):
    """Write audio to WAV file.
    
    Args:
        filename: Output filename
        audio: Audio data as numpy array (float32, range -1 to 1)
        sample_rate: Sample rate in Hz
    """
    # Normalize and convert to int16
    audio = np.clip(audio, -1.0, 1.0)
    audio_int16 = (audio * 32767).astype(np.int16)
    
    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(1)  # Mono
        wav_file.setsampwidth(2)  # 16-bit
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(audio_int16.tobytes())

def generate_bpm_change_test(output_file, sample_rate=44100):
    """Generate test audio with multiple BPM sections.
    
    Creates an audio file with the following structure:
    - Section 1 (0-10s): 120 BPM (stable baseline)
    - Section 2 (10-20s): 140 BPM (increase)
    - Section 3 (20-30s): 100 BPM (decrease)
    - Section 4 (30-40s): 160 BPM (fast)
    - Section 5 (40-50s): 80 BPM (slow)
    - Section 6 (50-60s): 120 BPM (return to baseline)
    
    Args:
        output_file: Output WAV filename
        sample_rate: Sample rate in Hz
    
    Returns:
        Dictionary with ground truth information
    """
    sections = [
        {'bpm': 120, 'duration': 10, 'pattern': 'full'},
        {'bpm': 140, 'duration': 10, 'pattern': 'full'},
        {'bpm': 100, 'duration': 10, 'pattern': 'full'},
        {'bpm': 160, 'duration': 10, 'pattern': 'full'},
        {'bpm': 80, 'duration': 10, 'pattern': 'full'},
        {'bpm': 120, 'duration': 10, 'pattern': 'full'},
    ]
    
    audio_segments = []
    ground_truth = {
        'sample_rate': sample_rate,
        'sections': []
    }
    
    current_time = 0.0
    for section in sections:
        audio, beat_positions = generate_onset_pattern(
            section['bpm'], 
            section['duration'],
            sample_rate,
            section['pattern']
        )
        audio_segments.append(audio)
        
        # Record ground truth
        ground_truth['sections'].append({
            'start_time': current_time,
            'end_time': current_time + section['duration'],
            'start_sample': int(current_time * sample_rate),
            'end_sample': int((current_time + section['duration']) * sample_rate),
            'bpm': section['bpm'],
            'beat_positions': [int(current_time * sample_rate) + bp for bp in beat_positions]
        })
        
        current_time += section['duration']
    
    # Concatenate all sections
    full_audio = np.concatenate(audio_segments)
    
    # Write to file
    write_wav(output_file, full_audio, sample_rate)
    
    return ground_truth

def generate_gradual_change_test(output_file, sample_rate=44100):
    """Generate test audio with gradual BPM changes.
    
    Creates an audio file with smooth tempo transitions:
    - 0-15s: 100 BPM (stable)
    - 15-30s: Accelerando from 100 to 140 BPM
    - 30-45s: 140 BPM (stable)
    - 45-60s: Ritardando from 140 to 100 BPM
    
    Args:
        output_file: Output WAV filename
        sample_rate: Sample rate in Hz
    
    Returns:
        Dictionary with ground truth information
    """
    ground_truth = {
        'sample_rate': sample_rate,
        'type': 'gradual_change',
        'sections': []
    }
    
    audio = np.array([], dtype=np.float32)
    current_sample = 0
    
    # Section 1: Stable 100 BPM
    duration = 15
    section_audio, _ = generate_onset_pattern(100, duration, sample_rate, 'full')
    audio = np.concatenate([audio, section_audio])
    ground_truth['sections'].append({
        'start_time': current_sample / sample_rate,
        'end_time': (current_sample + len(section_audio)) / sample_rate,
        'type': 'stable',
        'bpm': 100
    })
    current_sample += len(section_audio)
    
    # Section 2: Accelerando 100->140 BPM
    duration = 15
    num_beats = int(duration * 120 / 60)  # Approximate number of beats
    section_audio = np.array([], dtype=np.float32)
    for i in range(num_beats):
        # Linear interpolation of BPM
        progress = i / num_beats
        bpm = 100 + (140 - 100) * progress
        beat_audio, _ = generate_onset_pattern(bpm, 0.5, sample_rate, 'kick')
        section_audio = np.concatenate([section_audio, beat_audio])
    
    audio = np.concatenate([audio, section_audio])
    ground_truth['sections'].append({
        'start_time': current_sample / sample_rate,
        'end_time': (current_sample + len(section_audio)) / sample_rate,
        'type': 'accelerando',
        'bpm_start': 100,
        'bpm_end': 140
    })
    current_sample += len(section_audio)
    
    # Section 3: Stable 140 BPM
    duration = 15
    section_audio, _ = generate_onset_pattern(140, duration, sample_rate, 'full')
    audio = np.concatenate([audio, section_audio])
    ground_truth['sections'].append({
        'start_time': current_sample / sample_rate,
        'end_time': (current_sample + len(section_audio)) / sample_rate,
        'type': 'stable',
        'bpm': 140
    })
    current_sample += len(section_audio)
    
    # Section 4: Ritardando 140->100 BPM
    duration = 15
    num_beats = int(duration * 120 / 60)
    section_audio = np.array([], dtype=np.float32)
    for i in range(num_beats):
        progress = i / num_beats
        bpm = 140 - (140 - 100) * progress
        beat_audio, _ = generate_onset_pattern(bpm, 0.5, sample_rate, 'kick')
        section_audio = np.concatenate([section_audio, beat_audio])
    
    audio = np.concatenate([audio, section_audio])
    ground_truth['sections'].append({
        'start_time': current_sample / sample_rate,
        'end_time': (current_sample + len(section_audio)) / sample_rate,
        'type': 'ritardando',
        'bpm_start': 140,
        'bpm_end': 100
    })
    
    write_wav(output_file, audio, sample_rate)
    return ground_truth

def main():
    """Generate all test audio files."""
    import json
    
    print("Generating BPM change test audio...")
    ground_truth_changes = generate_bpm_change_test('test_bpm_changes.wav')
    with open('test_bpm_changes_ground_truth.json', 'w') as f:
        json.dump(ground_truth_changes, f, indent=2)
    print(f"  Created: test_bpm_changes.wav (60 seconds, 6 BPM sections)")
    
    print("Generating gradual BPM change test audio...")
    ground_truth_gradual = generate_gradual_change_test('test_bpm_gradual.wav')
    with open('test_bpm_gradual_ground_truth.json', 'w') as f:
        json.dump(ground_truth_gradual, f, indent=2)
    print(f"  Created: test_bpm_gradual.wav (60 seconds, accelerando/ritardando)")
    
    print("\nGround truth files:")
    print("  - test_bpm_changes_ground_truth.json")
    print("  - test_bpm_gradual_ground_truth.json")
    print("\nUse these files for benchmarking tempo tracking accuracy and responsiveness.")

if __name__ == '__main__':
    main()
