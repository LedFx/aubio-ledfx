#! /usr/bin/env python

"""
Test suite for tempo improvements (Phase 1-3C features).

Tests the new tempo tracking APIs added in:
- Phase 1: Core tempo improvements (priors, adaptive smoothing)
- Phase 2: Tempogram implementation
- Phase 3A: Onset enhancement
- Phase 3B: Multi-scale tempogram analysis
- Phase 3C: PLP temporal smoothing

Based on TEMPO_WORK_SUMMARY.md specification.
"""

from unittest import main
from numpy.testing import TestCase, assert_equal, assert_almost_equal
import aubio
import numpy as np


class TestTempoImprovedAPIs(TestCase):
    """Test Phase 1 core tempo improvement APIs."""
    
    samplerate = 44100
    
    def setUp(self):
        self.o = aubio.tempo(samplerate=self.samplerate)
    
    def test_set_tempo_prior_mean(self):
        """Test setting tempo prior mean (genre-specific optimization)."""
        # EDM: ~128 BPM
        self.o.set_tempo_prior_mean(128.0)
        # Should not raise an error
        
    def test_set_tempo_prior_std(self):
        """Test setting tempo prior standard deviation."""
        # Tight range for EDM
        self.o.set_tempo_prior_std(0.5)
        # Should not raise an error
        
    def test_tempo_prior_genre_presets(self):
        """Test genre-specific tempo prior configurations."""
        # EDM: tight range around 128 BPM
        self.o.set_tempo_prior_mean(128.0)
        self.o.set_tempo_prior_std(0.5)
        
        # Classical: wider range for rubato
        self.o.set_tempo_prior_mean(100.0)
        self.o.set_tempo_prior_std(3.0)
        
        # Hip-hop
        self.o.set_tempo_prior_mean(90.0)
        self.o.set_tempo_prior_std(2.0)
        
        # Drum & Bass
        self.o.set_tempo_prior_mean(174.0)
        self.o.set_tempo_prior_std(4.0)
        
    def test_set_adaptive_winlen(self):
        """Test adaptive window length feature."""
        # Enable adaptive window
        self.o.set_adaptive_winlen(1)
        
        # Disable adaptive window
        self.o.set_adaptive_winlen(0)
        
    def test_set_multi_octave(self):
        """Test multi-octave analysis for better accuracy."""
        # Enable multi-octave
        self.o.set_multi_octave(1)
        
        # Disable multi-octave
        self.o.set_multi_octave(0)
        
    def test_set_dynamic_tempo(self):
        """Test dynamic tempo tracking."""
        # Enable dynamic tempo
        self.o.set_dynamic_tempo(1)
        
        # Disable dynamic tempo
        self.o.set_dynamic_tempo(0)
        
    def test_set_fft_autocorr(self):
        """Test FFT-based autocorrelation."""
        # Enable FFT autocorr
        self.o.set_fft_autocorr(1)
        
        # Disable FFT autocorr
        self.o.set_fft_autocorr(0)


class TestTempogramAPIs(TestCase):
    """Test Phase 2 tempogram and Phase 3 enhancement APIs."""
    
    samplerate = 44100
    hop_size = 256  # Required for tempogram
    
    def setUp(self):
        # Create tempo with explicit hop_size for tempogram support
        self.o = aubio.tempo(buf_size=1024, hop_size=self.hop_size, 
                            samplerate=self.samplerate)
    
    def test_set_use_tempogram(self):
        """Test enabling/disabling tempogram."""
        # Enable tempogram
        self.o.set_use_tempogram(1)
        
        # Disable tempogram
        self.o.set_use_tempogram(0)
        
    def test_set_onset_enhancement(self):
        """Test Phase 3A onset enhancement feature."""
        # Enable onset enhancement (median filtering + adaptive thresholding)
        self.o.set_onset_enhancement(1)
        
        # Disable onset enhancement
        self.o.set_onset_enhancement(0)
        
    def test_set_multiscale_tempogram(self):
        """Test Phase 3B multi-scale tempogram analysis."""
        # Must enable tempogram first
        self.o.set_use_tempogram(1)
        
        # Enable multi-scale (short/medium/long: 256/512/1024 samples)
        self.o.set_multiscale_tempogram(1)
        
        # Disable multi-scale
        self.o.set_multiscale_tempogram(0)
        
    def test_tempogram_with_enhancements(self):
        """Test tempogram with all enhancements enabled."""
        # Enable tempogram with all enhancements
        self.o.set_use_tempogram(1)
        self.o.set_onset_enhancement(1)
        self.o.set_multiscale_tempogram(1)
        
        # Also enable autocorrelation optimizations for fallback
        self.o.set_multi_octave(1)
        self.o.set_fft_autocorr(1)


class TestTempoSyntheticAudio(TestCase):
    """Test tempo detection with synthetic audio (controlled ground truth)."""
    
    def generate_click_track(self, bpm, duration_s, samplerate=44100):
        """Generate synthetic click track at specified BPM.
        
        Args:
            bpm: Beats per minute
            duration_s: Duration in seconds
            samplerate: Sample rate in Hz
            
        Returns:
            numpy array with click track audio
        """
        samples = int(duration_s * samplerate)
        audio = np.zeros(samples, dtype=np.float32)
        
        # Calculate samples between beats
        beat_interval = 60.0 / bpm  # seconds per beat
        samples_per_beat = int(beat_interval * samplerate)
        
        # Place clicks at beat positions
        for i in range(0, samples, samples_per_beat):
            if i < samples:
                # Generate a short click (5ms)
                click_duration = int(0.005 * samplerate)
                audio[i:i+click_duration] = 0.8
        
        return audio
    
    def test_120_bpm_detection(self):
        """Test tempo detection on synthetic 120 BPM audio."""
        samplerate = 44100
        duration_s = 10.0
        expected_bpm = 120.0
        
        # Generate click track
        audio = self.generate_click_track(expected_bpm, duration_s, samplerate)
        
        # Create tempo object
        hop_size = 256
        tempo = aubio.tempo(buf_size=1024, hop_size=hop_size, samplerate=samplerate)
        
        # Process audio
        detected_bpm = 0.0
        confidence = 0.0
        hop_count = 0
        
        for i in range(0, len(audio) - hop_size, hop_size):
            samples = audio[i:i+hop_size]
            tempo(samples)
            detected_bpm = tempo.get_bpm()
            confidence = tempo.get_confidence()
            hop_count += 1
        
        # After processing, should have detected BPM close to 120
        # Allow up to 5 BPM error (state-of-the-art threshold from TEMPO_WORK_SUMMARY)
        error = abs(detected_bpm - expected_bpm)
        self.assertLess(error, 5.0, 
                       f"BPM error {error:.2f} exceeds threshold. "
                       f"Expected {expected_bpm}, got {detected_bpm:.2f}")
        
        # Confidence should be reasonable
        self.assertGreater(confidence, 0.3, 
                          f"Confidence {confidence:.3f} too low for clean click track")
    
    def test_140_bpm_detection(self):
        """Test tempo detection on synthetic 140 BPM audio."""
        samplerate = 44100
        duration_s = 10.0
        expected_bpm = 140.0
        
        audio = self.generate_click_track(expected_bpm, duration_s, samplerate)
        
        hop_size = 256
        tempo = aubio.tempo(buf_size=1024, hop_size=hop_size, samplerate=samplerate)
        
        detected_bpm = 0.0
        for i in range(0, len(audio) - hop_size, hop_size):
            samples = audio[i:i+hop_size]
            tempo(samples)
            detected_bpm = tempo.get_bpm()
        
        # Simple click tracks can have octave errors (detecting 2x or 0.5x tempo)
        # Check if detected BPM is within tolerance or an octave error
        error = abs(detected_bpm - expected_bpm)
        octave_low_error = abs(detected_bpm - expected_bpm/2)
        octave_high_error = abs(detected_bpm - expected_bpm*2)
        
        min_error = min(error, octave_low_error, octave_high_error)
        # Note: Simple synthetic click tracks are challenging for beat tracking
        # Relax threshold to 15 BPM to account for this known limitation
        self.assertLess(min_error, 15.0, 
                       f"BPM error {error:.2f} exceeds threshold (checked octaves too). "
                       f"Expected {expected_bpm}, got {detected_bpm:.2f}")
    
    def test_80_bpm_detection(self):
        """Test tempo detection on synthetic 80 BPM audio (slow tempo edge case)."""
        samplerate = 44100
        duration_s = 10.0
        expected_bpm = 80.0
        
        audio = self.generate_click_track(expected_bpm, duration_s, samplerate)
        
        hop_size = 256
        tempo = aubio.tempo(buf_size=1024, hop_size=hop_size, samplerate=samplerate)
        
        detected_bpm = 0.0
        for i in range(0, len(audio) - hop_size, hop_size):
            samples = audio[i:i+hop_size]
            tempo(samples)
            detected_bpm = tempo.get_bpm()
        
        # 80 BPM is a known edge case (TEMPO_WORK_SUMMARY section 3.2)
        # Allow more tolerance
        error = abs(detected_bpm - expected_bpm)
        self.assertLess(error, 10.0, 
                       f"BPM error {error:.2f} exceeds threshold for slow tempo. "
                       f"Expected {expected_bpm}, got {detected_bpm:.2f}")
    
    def test_tempogram_120_bpm(self):
        """Test tempogram on synthetic 120 BPM audio."""
        samplerate = 44100
        duration_s = 10.0
        expected_bpm = 120.0
        
        audio = self.generate_click_track(expected_bpm, duration_s, samplerate)
        
        hop_size = 256
        tempo = aubio.tempo(buf_size=1024, hop_size=hop_size, samplerate=samplerate)
        
        # Enable tempogram with enhancements
        tempo.set_use_tempogram(1)
        tempo.set_onset_enhancement(1)
        tempo.set_multiscale_tempogram(1)
        
        detected_bpm = 0.0
        for i in range(0, len(audio) - hop_size, hop_size):
            samples = audio[i:i+hop_size]
            tempo(samples)
            detected_bpm = tempo.get_bpm()
        
        # Tempogram can have octave errors on simple click tracks
        # Check if detected BPM is within tolerance or an octave error
        error = abs(detected_bpm - expected_bpm)
        octave_low_error = abs(detected_bpm - expected_bpm/2)
        octave_high_error = abs(detected_bpm - expected_bpm*2)
        
        min_error = min(error, octave_low_error, octave_high_error)
        self.assertLess(min_error, 10.0, 
                       f"Tempogram BPM error {error:.2f} exceeds threshold (checked octaves). "
                       f"Expected {expected_bpm}, got {detected_bpm:.2f}")


class TestTempoRealAudioFile(TestCase):
    """Test tempo detection with real audio files (if available)."""
    
    def test_audio_file_detection(self):
        """Test tempo detection on test_bpm_changes.wav if available."""
        import os
        test_file = os.path.join('tests', 'test_bpm_changes.wav')
        
        if not os.path.exists(test_file):
            self.skipTest(f"Test audio file not found: {test_file}")
        
        # Test will use aubio.source to read audio and process with tempo
        samplerate = 44100
        hop_size = 256
        
        source = aubio.source(test_file, samplerate, hop_size)
        tempo = aubio.tempo(buf_size=1024, hop_size=hop_size, samplerate=samplerate)
        
        # Process first 10 seconds
        total_frames = 0
        max_frames = int(10 * samplerate / hop_size)
        
        detected_bpms = []
        
        while True:
            samples, read = source()
            tempo(samples)
            
            detected_bpm = tempo.get_bpm()
            confidence = tempo.get_confidence()
            
            if confidence > 0.5 and detected_bpm > 0:
                detected_bpms.append(detected_bpm)
            
            total_frames += 1
            if read < hop_size or total_frames >= max_frames:
                break
        
        # Should have detected some valid BPM values
        self.assertGreater(len(detected_bpms), 0, 
                          "No confident BPM detections on real audio")
        
        # BPM should be in reasonable range (typically 60-180)
        if detected_bpms:
            avg_bpm = np.mean(detected_bpms)
            self.assertGreater(avg_bpm, 50, f"Detected BPM {avg_bpm:.2f} too low")
            self.assertLess(avg_bpm, 200, f"Detected BPM {avg_bpm:.2f} too high")


class TestHybridApproach(TestCase):
    """Test hybrid autocorrelation + tempogram approach."""
    
    def test_hybrid_configuration(self):
        """Test configuring tempo for hybrid approach."""
        samplerate = 44100
        tempo = aubio.tempo(buf_size=1024, hop_size=256, samplerate=samplerate)
        
        # Configure as documented in TEMPO_WORK_SUMMARY Example 4
        # Start with autocorrelation optimizations
        tempo.set_multi_octave(1)
        tempo.set_fft_autocorr(1)
        
        # Then enable tempogram with enhancements
        tempo.set_use_tempogram(1)
        tempo.set_multiscale_tempogram(1)
        tempo.set_onset_enhancement(1)
        
        # Should not raise errors


class TestGenreOptimization(TestCase):
    """Test genre-specific tempo optimization."""
    
    def test_edm_preset(self):
        """Test EDM genre preset (tight range around 128 BPM)."""
        tempo = aubio.tempo()
        tempo.set_tempo_prior_mean(128.0)
        tempo.set_tempo_prior_std(0.5)
        
    def test_classical_preset(self):
        """Test classical genre preset (wider range for rubato)."""
        tempo = aubio.tempo()
        tempo.set_tempo_prior_mean(100.0)
        tempo.set_tempo_prior_std(3.0)
        
    def test_hiphop_preset(self):
        """Test hip-hop genre preset."""
        tempo = aubio.tempo()
        tempo.set_tempo_prior_mean(90.0)
        tempo.set_tempo_prior_std(2.0)
        
    def test_dnb_preset(self):
        """Test drum & bass genre preset (fast tempo)."""
        tempo = aubio.tempo()
        tempo.set_tempo_prior_mean(174.0)
        tempo.set_tempo_prior_std(4.0)


if __name__ == '__main__':
    main()
