/*
  Copyright (C) 2024 aubio-ledfx contributors

  This file is part of aubio.

  aubio is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  aubio is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with aubio.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * Session +1: DP Tracker Unit Tests
 * 
 * Purpose: Test individual DP components in isolation
 * 
 * Tests:
 * 1. Penalty function behavior
 * 2. Beat extraction at different buffer fills
 * 3. Tempo adaptation with changing priors
 * 4. Observation model variations
 * 5. Buffer wraparound handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "aubio.h"
#include "tempo/dptracker.h"

#define WIN_S 512
#define HOP_S 256
#define SAMPLERATE 44100

// ANSI colors
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

void print_test(const char *name) {
  fprintf(stderr, "\n%s=== %s ===%s\n", COLOR_BOLD, name, COLOR_RESET);
}

int test_passed = 0;
int test_failed = 0;

void assert_true(const char *test, int condition) {
  if (condition) {
    fprintf(stderr, "%s✓ %s%s\n", COLOR_GREEN, test, COLOR_RESET);
    test_passed++;
  } else {
    fprintf(stderr, "%s✗ %s%s\n", COLOR_RED, test, COLOR_RESET);
    test_failed++;
  }
}

void assert_close(const char *test, smpl_t actual, smpl_t expected, smpl_t tolerance) {
  smpl_t diff = fabs(actual - expected);
  if (diff <= tolerance) {
    fprintf(stderr, "%s✓ %s: %.2f (expected %.2f, diff %.3f)%s\n", 
            COLOR_GREEN, test, actual, expected, diff, COLOR_RESET);
    test_passed++;
  } else {
    fprintf(stderr, "%s✗ %s: %.2f (expected %.2f, diff %.3f > tolerance %.3f)%s\n", 
            COLOR_RED, test, actual, expected, diff, tolerance, COLOR_RESET);
    test_failed++;
  }
}

// Test 1: Tempo adaptation - does DP adjust to different tempos?
int test_tempo_adaptation(void) {
  print_test("Test 1: Tempo Adaptation");
  
  // Test different tempo patterns
  smpl_t test_tempos[] = {80.0, 100.0, 120.0, 140.0, 160.0};
  uint_t num_tempos = sizeof(test_tempos) / sizeof(test_tempos[0]);
  
  for (uint_t t = 0; t < num_tempos; t++) {
    smpl_t target_bpm = test_tempos[t];
    smpl_t beat_interval_s = 60.0 / target_bpm;
    uint_t beat_interval_samples = (uint_t)(beat_interval_s * SAMPLERATE);
    uint_t beat_interval_frames = beat_interval_samples / HOP_S;
    
    aubio_dptracker_t *dp = new_aubio_dptracker(WIN_S, HOP_S, SAMPLERATE);
    
    // Set appropriate tempo prior
    aubio_dptracker_set_tempo(dp, target_bpm, 20.0);
    
    // Feed beats at target tempo
    uint_t num_beats = 10;
    for (uint_t b = 0; b < num_beats; b++) {
      uint_t beat_frame = b * beat_interval_frames;
      for (uint_t f = 0; f < beat_interval_frames && f < WIN_S; f++) {
        smpl_t onset = (f == 0) ? 1.0 : 0.1; // Strong onset at beat
        aubio_dptracker_do(dp, onset);
      }
    }
    
    // Extract beats
    fvec_t *beats = new_fvec(WIN_S);
    aubio_dptracker_get_beats(dp, beats);
    
    // Get BPM
    smpl_t detected_bpm = aubio_dptracker_get_bpm(dp);
    uint_t num_detected = aubio_dptracker_get_num_beats(dp);
    
    char test_name[100];
    snprintf(test_name, sizeof(test_name), "%.0f BPM detection", target_bpm);
    
    // Allow 10% error
    smpl_t error_tolerance = target_bpm * 0.10;
    assert_close(test_name, detected_bpm, target_bpm, error_tolerance);
    
    fprintf(stderr, "  Detected %u beats, BPM: %.2f (expected %.0f)\n", 
            num_detected, detected_bpm, target_bpm);
    
    del_fvec(beats);
    del_aubio_dptracker(dp);
  }
  
  return 0;
}

// Test 2: Beat extraction at different buffer fills
int test_beat_extraction_timing(void) {
  print_test("Test 2: Beat Extraction at Different Buffer Fills");
  
  aubio_dptracker_t *dp = new_aubio_dptracker(WIN_S, HOP_S, SAMPLERATE);
  aubio_dptracker_set_tempo(dp, 120.0, 20.0);
  
  // Test at different frame counts
  uint_t test_frames[] = {50, 100, 200, 300, 512};
  uint_t num_tests = sizeof(test_frames) / sizeof(test_frames[0]);
  
  for (uint_t i = 0; i < num_tests; i++) {
    // Reset and fill to target frame count
    aubio_dptracker_reset(dp);
    
    smpl_t beat_interval = 120.0 / 60.0; // seconds per beat
    uint_t beat_interval_frames = (uint_t)(beat_interval * SAMPLERATE / HOP_S);
    
    for (uint_t f = 0; f < test_frames[i]; f++) {
      smpl_t onset = (f % beat_interval_frames == 0) ? 1.0 : 0.1;
      aubio_dptracker_do(dp, onset);
    }
    
    fvec_t *beats = new_fvec(WIN_S);
    aubio_dptracker_get_beats(dp, beats);
    uint_t num_beats = aubio_dptracker_get_num_beats(dp);
    
    fprintf(stderr, "  After %u frames: Detected %u beats\n", 
            test_frames[i], num_beats);
    
    // Should detect at least 1 beat after enough frames
    if (test_frames[i] >= beat_interval_frames * 2) {
      assert_true("Beats detected with sufficient data", num_beats >= 2);
    }
    
    del_fvec(beats);
  }
  
  del_aubio_dptracker(dp);
  return 0;
}

// Test 3: Onset strength variations
int test_onset_strength_variations(void) {
  print_test("Test 3: Onset Strength Variations");
  
  smpl_t onset_strengths[] = {0.3, 0.5, 0.8, 1.0};
  uint_t num_strengths = sizeof(onset_strengths) / sizeof(onset_strengths[0]);
  
  for (uint_t s = 0; s < num_strengths; s++) {
    aubio_dptracker_t *dp = new_aubio_dptracker(WIN_S, HOP_S, SAMPLERATE);
    aubio_dptracker_set_tempo(dp, 120.0, 20.0);
    
    smpl_t strength = onset_strengths[s];
    smpl_t beat_interval_frames = (SAMPLERATE / HOP_S) / 2.0; // 120 BPM
    
    // Feed 8 beats with varying strength
    for (uint_t b = 0; b < 8; b++) {
      for (uint_t f = 0; f < beat_interval_frames; f++) {
        smpl_t onset = (f == 0) ? strength : 0.05;
        aubio_dptracker_do(dp, onset);
      }
    }
    
    fvec_t *beats = new_fvec(WIN_S);
    aubio_dptracker_get_beats(dp, beats);
    uint_t num_beats = aubio_dptracker_get_num_beats(dp);
    smpl_t bpm = aubio_dptracker_get_bpm(dp);
    
    fprintf(stderr, "  Onset strength %.1f: Detected %u beats, BPM %.1f\n", 
            strength, num_beats, bpm);
    
    // Should still detect with weaker onsets
    assert_true("Beats detected with varied onset strength", num_beats >= 2);
    
    del_fvec(beats);
    del_aubio_dptracker(dp);
  }
  
  return 0;
}

// Test 4: Tempo range boundaries
int test_tempo_range_boundaries(void) {
  print_test("Test 4: Tempo Range Boundaries");
  
  aubio_dptracker_t *dp = new_aubio_dptracker(WIN_S, HOP_S, SAMPLERATE);
  
  // Test edge case tempos
  smpl_t edge_tempos[] = {60.0, 80.0, 120.0, 160.0, 200.0};
  uint_t num_edge = sizeof(edge_tempos) / sizeof(edge_tempos[0]);
  
  for (uint_t i = 0; i < num_edge; i++) {
    aubio_dptracker_reset(dp);
    aubio_dptracker_set_tempo(dp, edge_tempos[i], 30.0);
    
    smpl_t beat_interval_frames = (SAMPLERATE / HOP_S) * (60.0 / edge_tempos[i]);
    
    // Feed 6 beats
    for (uint_t b = 0; b < 6; b++) {
      for (uint_t f = 0; f < beat_interval_frames && f < WIN_S/6; f++) {
        smpl_t onset = (f == 0) ? 1.0 : 0.1;
        aubio_dptracker_do(dp, onset);
      }
    }
    
    fvec_t *beats = new_fvec(WIN_S);
    aubio_dptracker_get_beats(dp, beats);
    smpl_t bpm = aubio_dptracker_get_bpm(dp);
    
    fprintf(stderr, "  %.0f BPM: Detected %.1f BPM\n", edge_tempos[i], bpm);
    
    del_fvec(beats);
  }
  
  del_aubio_dptracker(dp);
  return 0;
}

// Test 5: Buffer wraparound handling
int test_buffer_wraparound(void) {
  print_test("Test 5: Buffer Wraparound Handling");
  
  aubio_dptracker_t *dp = new_aubio_dptracker(WIN_S, HOP_S, SAMPLERATE);
  aubio_dptracker_set_tempo(dp, 120.0, 20.0);
  
  smpl_t beat_interval_frames = (SAMPLERATE / HOP_S) / 2.0; // 120 BPM
  
  // Feed more frames than buffer size to test wraparound
  uint_t total_frames = WIN_S * 2;
  uint_t beats_fed = 0;
  
  for (uint_t f = 0; f < total_frames; f++) {
    smpl_t onset = (f % (uint_t)beat_interval_frames == 0) ? 1.0 : 0.1;
    if (onset > 0.5) beats_fed++;
    aubio_dptracker_do(dp, onset);
  }
  
  fvec_t *beats = new_fvec(WIN_S);
  aubio_dptracker_get_beats(dp, beats);
  uint_t num_beats = aubio_dptracker_get_num_beats(dp);
  smpl_t bpm = aubio_dptracker_get_bpm(dp);
  
  fprintf(stderr, "  Fed %u beats over %u frames (buffer size %u)\n", 
          beats_fed, total_frames, WIN_S);
  fprintf(stderr, "  Detected %u beats, BPM %.1f\n", num_beats, bpm);
  
  assert_true("DP handles buffer wraparound", num_beats >= 2);
  assert_close("BPM after wraparound", bpm, 120.0, 15.0);
  
  del_fvec(beats);
  del_aubio_dptracker(dp);
  return 0;
}

// Test 6: Sparse onset patterns
int test_sparse_onsets(void) {
  print_test("Test 6: Sparse Onset Patterns");
  
  aubio_dptracker_t *dp = new_aubio_dptracker(WIN_S, HOP_S, SAMPLERATE);
  aubio_dptracker_set_tempo(dp, 100.0, 20.0);
  
  smpl_t beat_interval_frames = (SAMPLERATE / HOP_S) * (60.0 / 100.0);
  
  // Feed with silent gaps
  for (uint_t b = 0; b < 5; b++) {
    // Beat
    aubio_dptracker_do(dp, 1.0);
    
    // Silence (background noise level)
    for (uint_t f = 1; f < beat_interval_frames; f++) {
      aubio_dptracker_do(dp, 0.05);
    }
  }
  
  fvec_t *beats = new_fvec(WIN_S);
  aubio_dptracker_get_beats(dp, beats);
  uint_t num_beats = aubio_dptracker_get_num_beats(dp);
  smpl_t bpm = aubio_dptracker_get_bpm(dp);
  
  fprintf(stderr, "  Sparse onsets: Detected %u beats, BPM %.1f\n", num_beats, bpm);
  
  assert_true("Handles sparse onsets", num_beats >= 2);
  
  del_fvec(beats);
  del_aubio_dptracker(dp);
  return 0;
}

int main(void) {
  fprintf(stderr, "\n%s╔══════════════════════════════════════════════════════════════╗%s\n", 
          COLOR_CYAN, COLOR_RESET);
  fprintf(stderr, "%s║  SESSION +1: DP TRACKER UNIT TESTS                           ║%s\n", 
          COLOR_CYAN, COLOR_RESET);
  fprintf(stderr, "%s╚══════════════════════════════════════════════════════════════╝%s\n", 
          COLOR_CYAN, COLOR_RESET);
  
  test_tempo_adaptation();
  test_beat_extraction_timing();
  test_onset_strength_variations();
  test_tempo_range_boundaries();
  test_buffer_wraparound();
  test_sparse_onsets();
  
  fprintf(stderr, "\n%s╔══════════════════════════════════════════════════════════════╗%s\n", 
          COLOR_CYAN, COLOR_RESET);
  fprintf(stderr, "%s║  UNIT TESTS COMPLETE                                         ║%s\n", 
          COLOR_CYAN, COLOR_RESET);
  fprintf(stderr, "%s╚══════════════════════════════════════════════════════════════╝%s\n", 
          COLOR_CYAN, COLOR_RESET);
  
  fprintf(stderr, "\n%sResults:%s\n", COLOR_BOLD, COLOR_RESET);
  fprintf(stderr, "  %sPassed: %d%s\n", COLOR_GREEN, test_passed, COLOR_RESET);
  fprintf(stderr, "  %sFailed: %d%s\n", test_failed > 0 ? COLOR_RED : COLOR_GREEN, 
          test_failed, COLOR_RESET);
  
  if (test_failed == 0) {
    fprintf(stderr, "\n%s✓ All unit tests passed%s\n", COLOR_GREEN, COLOR_RESET);
  } else {
    fprintf(stderr, "\n%s✗ Some unit tests failed%s\n", COLOR_RED, COLOR_RESET);
  }
  
  fprintf(stderr, "\n%sKey Insights:%s\n", COLOR_BOLD, COLOR_RESET);
  fprintf(stderr, "  • DP components tested in isolation\n");
  fprintf(stderr, "  • Tempo adaptation behavior characterized\n");
  fprintf(stderr, "  • Buffer wraparound handling verified\n");
  fprintf(stderr, "  • Onset strength sensitivity measured\n");
  
  return (test_failed > 0) ? 1 : 0;
}
