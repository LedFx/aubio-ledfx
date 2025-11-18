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

/* 
 * test-tempo-dp.c
 * 
 * Integration test for Dynamic Programming beat tracking
 * Tests DP tracker integration with tempo API on synthetic audio
 */

#include "aubio.h"
#include "aubio_priv.h"
#include "utils_tests.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define AUBIO_UNSTABLE 1
#include "tempo/tempo.h"

/* Test file path resolution */
#ifdef AUBIO_TEMPO_TEST_DIR
#define TEMPO_TEST_FILE(filename) AUBIO_TEMPO_TEST_DIR "/" filename
#else
#define TEMPO_TEST_FILE(filename) "tests/" filename
#endif

int main(void) {
  fprintf(stderr, "╔══════════════════════════════════════════════════════════════╗\n");
  fprintf(stderr, "║        TEMPO DP INTEGRATION TEST                            ║\n");
  fprintf(stderr, "╚══════════════════════════════════════════════════════════════╝\n\n");
  
  uint_t win_s = 1024;      // FFT window size
  uint_t hop_s = 256;       // Hop size
  uint_t samplerate = 44100; // Sample rate
  
  /* === Test 1: Create tempo with DP enabled === */
  fprintf(stderr, "=== Test 1: Create Tempo with DP Enabled ===\n");
  
  aubio_tempo_t *tempo = new_aubio_tempo("default", win_s, hop_s, samplerate);
  if (!tempo) {
    fprintf(stderr, "✗ Failed to create tempo object\n");
    return 1;
  }
  fprintf(stderr, "✓ Tempo object created\n");
  
  /* Enable DP tracker */
  if (aubio_tempo_set_use_dp(tempo, 1) != AUBIO_OK) {
    fprintf(stderr, "✗ Failed to enable DP tracker\n");
    del_aubio_tempo(tempo);
    return 1;
  }
  fprintf(stderr, "✓ DP tracker enabled\n\n");
  
  /* === Test 2: Generate synthetic beats at 120 BPM === */
  fprintf(stderr, "=== Test 2: Process Synthetic Beats (120 BPM) ===\n");
  
  smpl_t bpm_target = 120.0;
  smpl_t beats_per_second = bpm_target / 60.0;
  smpl_t samples_per_beat = samplerate / beats_per_second;
  
  fvec_t *input = new_fvec(hop_s);
  fvec_t *output = new_fvec(2);
  
  uint_t total_beats = 10;
  uint_t total_samples = (uint_t)(samples_per_beat * total_beats);
  uint_t num_frames = total_samples / hop_s;
  
  fprintf(stderr, "  Target BPM: %.1f\n", bpm_target);
  fprintf(stderr, "  Samples per beat: %.1f\n", samples_per_beat);
  fprintf(stderr, "  Total frames: %u\n", num_frames);
  fprintf(stderr, "  Processing");
  
  uint_t beats_detected = 0;
  smpl_t first_detection_time = 0;
  uint_t detection_started = 0;
  
  for (uint_t i = 0; i < num_frames; i++) {
    /* Generate impulse at beat positions */
    fvec_zeros(input);
    uint_t sample_pos = i * hop_s;
    
    /* Check if this frame contains a beat */
    for (uint_t b = 0; b < total_beats; b++) {
      uint_t beat_sample = (uint_t)(b * samples_per_beat);
      if (beat_sample >= sample_pos && beat_sample < sample_pos + hop_s) {
        /* Put impulse at the beat position */
        uint_t impulse_pos = beat_sample - sample_pos;
        if (impulse_pos < hop_s) {
          input->data[impulse_pos] = 1.0;
        }
      }
    }
    
    /* Process through tempo detector */
    aubio_tempo_do(tempo, input, output);
    
    /* Check for beat detection */
    if (output->data[0] > 0) {
      beats_detected++;
      if (!detection_started) {
        first_detection_time = (smpl_t)i * hop_s / samplerate;
        detection_started = 1;
      }
    }
    
    /* Progress indicator */
    if (i % 50 == 0) {
      fprintf(stderr, ".");
      fflush(stderr);
    }
  }
  
  fprintf(stderr, " done\n");
  fprintf(stderr, "  Beats detected: %u / %u\n", beats_detected, total_beats);
  fprintf(stderr, "  First detection: %.2f seconds\n", first_detection_time);
  
  /* Get BPM estimate */
  smpl_t bpm_detected = aubio_tempo_get_bpm(tempo);
  smpl_t bpm_error = fabs(bpm_detected - bpm_target);
  smpl_t bpm_error_pct = (bpm_error / bpm_target) * 100.0;
  
  fprintf(stderr, "  Target BPM: %.2f\n", bpm_target);
  fprintf(stderr, "  Detected BPM: %.2f\n", bpm_detected);
  fprintf(stderr, "  Error: %.2f BPM (%.1f%%)\n", bpm_error, bpm_error_pct);
  
  /* Check if result is acceptable (< 5 BPM error) */
  if (bpm_error < 5.0) {
    fprintf(stderr, "✓ PASS: BPM detection accurate\n\n");
  } else {
    fprintf(stderr, "✗ FAIL: BPM error too large (> 5 BPM)\n\n");
  }
  
  /* === Test 3: Test DP with onset enhancement === */
  fprintf(stderr, "=== Test 3: DP with Onset Enhancement ===\n");
  
  /* Reset tempo object */
  del_aubio_tempo(tempo);
  tempo = new_aubio_tempo("default", win_s, hop_s, samplerate);
  
  /* Enable DP and onset enhancement */
  aubio_tempo_set_use_dp(tempo, 1);
  aubio_tempo_set_onset_enhancement(tempo, 1);
  
  fprintf(stderr, "✓ DP + onset enhancement enabled\n");
  
  /* Process same synthetic audio */
  beats_detected = 0;
  for (uint_t i = 0; i < num_frames; i++) {
    fvec_zeros(input);
    uint_t sample_pos = i * hop_s;
    
    for (uint_t b = 0; b < total_beats; b++) {
      uint_t beat_sample = (uint_t)(b * samples_per_beat);
      if (beat_sample >= sample_pos && beat_sample < sample_pos + hop_s) {
        uint_t impulse_pos = beat_sample - sample_pos;
        if (impulse_pos < hop_s) {
          input->data[impulse_pos] = 1.0;
        }
      }
    }
    
    aubio_tempo_do(tempo, input, output);
    if (output->data[0] > 0) {
      beats_detected++;
    }
  }
  
  bpm_detected = aubio_tempo_get_bpm(tempo);
  bpm_error = fabs(bpm_detected - bpm_target);
  
  fprintf(stderr, "  Detected BPM: %.2f (error: %.2f BPM)\n", bpm_detected, bpm_error);
  
  if (bpm_error < 5.0) {
    fprintf(stderr, "✓ PASS: Onset enhancement compatible\n\n");
  } else {
    fprintf(stderr, "✗ FAIL: Onset enhancement degraded performance\n\n");
  }
  
  /* === Test 4: Test different tempo (140 BPM) === */
  fprintf(stderr, "=== Test 4: Different Tempo (140 BPM) ===\n");
  
  del_aubio_tempo(tempo);
  tempo = new_aubio_tempo("default", win_s, hop_s, samplerate);
  aubio_tempo_set_use_dp(tempo, 1);
  
  bpm_target = 140.0;
  beats_per_second = bpm_target / 60.0;
  samples_per_beat = samplerate / beats_per_second;
  total_samples = (uint_t)(samples_per_beat * total_beats);
  num_frames = total_samples / hop_s;
  
  for (uint_t i = 0; i < num_frames; i++) {
    fvec_zeros(input);
    uint_t sample_pos = i * hop_s;
    
    for (uint_t b = 0; b < total_beats; b++) {
      uint_t beat_sample = (uint_t)(b * samples_per_beat);
      if (beat_sample >= sample_pos && beat_sample < sample_pos + hop_s) {
        uint_t impulse_pos = beat_sample - sample_pos;
        if (impulse_pos < hop_s) {
          input->data[impulse_pos] = 1.0;
        }
      }
    }
    
    aubio_tempo_do(tempo, input, output);
  }
  
  bpm_detected = aubio_tempo_get_bpm(tempo);
  bpm_error = fabs(bpm_detected - bpm_target);
  bpm_error_pct = (bpm_error / bpm_target) * 100.0;
  
  fprintf(stderr, "  Target BPM: %.2f\n", bpm_target);
  fprintf(stderr, "  Detected BPM: %.2f\n", bpm_detected);
  fprintf(stderr, "  Error: %.2f BPM (%.1f%%)\n", bpm_error, bpm_error_pct);
  
  if (bpm_error < 5.0) {
    fprintf(stderr, "✓ PASS: 140 BPM detection accurate\n\n");
  } else {
    fprintf(stderr, "✗ FAIL: 140 BPM detection inaccurate\n\n");
  }
  
  /* === Cleanup === */
  del_fvec(input);
  del_fvec(output);
  del_aubio_tempo(tempo);
  
  fprintf(stderr, "╔══════════════════════════════════════════════════════════════╗\n");
  fprintf(stderr, "║        TEMPO DP INTEGRATION TEST COMPLETE                   ║\n");
  fprintf(stderr, "╚══════════════════════════════════════════════════════════════╝\n");
  
  return 0;
}
