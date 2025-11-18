"""Tests for nanobind-based Python bindings.

These tests verify that the new nanobind bindings provide
the same API as the old C extension, ensuring compatibility.

Status: Phase 1 - Infrastructure only, no bindings yet.
Tests will be populated during Phase 2-4 of the migration.
"""

import pytest
import sys

# Try to import nanobind version
try:
    import aubio._aubio_nb as aubio_nb
    HAS_NANOBIND = True
except ImportError:
    HAS_NANOBIND = False
    aubio_nb = None

# Skip all tests if nanobind bindings not available
pytestmark = pytest.mark.skipif(
    not HAS_NANOBIND,
    reason="nanobind bindings not built (Phase 1: infrastructure only)"
)


class TestInfrastructure:
    """Test that nanobind infrastructure is working."""
    
    def test_can_import(self):
        """Test that we can import the nanobind module."""
        if HAS_NANOBIND:
            assert aubio_nb is not None
            assert hasattr(aubio_nb, '__version__')
        else:
            pytest.skip("nanobind not available yet")


# Phase 2: Proof of Concept tests will be added here
class TestFVec:
    """Test fvec type from nanobind bindings."""
    
    def test_create_fvec(self):
        """Test creating an fvec."""
        pytest.skip("Phase 2: Not implemented yet")
    
    def test_fvec_access(self):
        """Test accessing fvec elements."""
        pytest.skip("Phase 2: Not implemented yet")
    
    def test_fvec_numpy_interop(self):
        """Test NumPy interoperability."""
        pytest.skip("Phase 2: Not implemented yet")


class TestCVec:
    """Test cvec type from nanobind bindings."""
    
    def test_create_cvec(self):
        """Test creating a cvec."""
        pytest.skip("Phase 2: Not implemented yet")


# Phase 3: Core Types tests will be added here
class TestMusicUtils:
    """Test music utility functions."""
    
    def test_freqtomidi(self):
        """Test frequency to MIDI conversion."""
        pytest.skip("Phase 3: Not implemented yet")


class TestFFT:
    """Test FFT from nanobind bindings."""
    
    def test_create_fft(self):
        """Test creating FFT."""
        pytest.skip("Phase 3: Not implemented yet")


# Phase 4: Object tests will be added here
class TestOnset:
    """Test onset detection from nanobind bindings."""
    
    def test_create_onset(self):
        """Test creating onset detector."""
        pytest.skip("Phase 4: Not implemented yet")
    
    def test_onset_detection(self):
        """Test onset detection on synthetic signal."""
        pytest.skip("Phase 4: Not implemented yet")


class TestPitch:
    """Test pitch detection from nanobind bindings."""
    
    def test_create_pitch(self):
        """Test creating pitch detector."""
        pytest.skip("Phase 4: Not implemented yet")


# More test classes will be added for:
# - tempo
# - notes
# - mfcc
# - specdesc
# - tss
# - pitchshift
# - wavetable
# - sampler
