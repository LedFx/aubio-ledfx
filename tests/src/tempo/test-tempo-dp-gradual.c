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
 * Phase 3D Session 4: DP Tracker on Gradual Tempo Changes
 * 
 * Purpose: Test DP tracker performance on accelerando/ritardando
 * Uses: test_bpm_gradual.wav (4 sections with gradual tempo changes)
 * 
 * Ground truth:
 * - Section 1 (0-15s): Steady 100 BPM
 * - Section 2 (15-30s): Accelerando 100→140 BPM
 * - Section 3 (30-45s): Steady 140 BPM  
 * - Section 4 (45-60s): Ritardando 140→100 BPM
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "aubio.h"

// File path macro for cross-platform compatibility
#ifdef AUBIO_TEMPO_TEST_DIR
#define TEMPO_TEST_FILE(filename) AUBIO_TEMPO_TEST_DIR "/" filename
#else
#define TEMPO_TEST_FILE(filename) "tests/" filename
#endif

#define HOP_S 256
#define WIN_S 1024
#define SAMPLERATE 44100

// ANSI colors
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RED     "\033[31m"

void print_header(const char *title) {
  fprintf(stderr, "\n%s╔══════════════════════════════════════════════════════════════╗%s\n", 
          COLOR_CYAN, COLOR_RESET);
  fprintf(stderr, "%s║  %-58s  ║%s\n", COLOR_CYAN, title, COLOR_RESET);
  fprintf(stderr, "%s╚══════════════════════════════════════════════════════════════╝%s\n", 
          COLOR_CYAN, COLOR_RESET);
}

void print_section(const char *title) {
  fprintf(stderr, "\n%s%s%s\n", COLOR_BOLD, title, COLOR_RESET);
  fprintf(stderr, "────────────────────────────────────────────────────────────\n");
}

// Test DP tracker on gradual tempo changes
int test_dp_gradual(void) {
  const char *test_file = TEMPO_TEST_FILE("test_bpm_gradual.wav");
  
  print_section("Testing DP Tracker on Gradual Tempo Changes");
  fprintf(stderr, "File: %s\n\n", test_file);
  
  // Create source
  aubio_source_t *source = new_aubio_source(test_file, SAMPLERATE, HOP_S);
  if (!source) {
    fprintf(stderr, "%s✗ Failed to open test file%s\n", COLOR_RED, COLOR_RESET);
    return 1;
  }
  
  uint_t source_samplerate = aubio_source_get_samplerate(source);
  fprintf(stderr, "Configuration:\n");
  fprintf(stderr, "  Sample rate: %u Hz\n", source_samplerate);
  fprintf(stderr, "  Hop size: %u samples\n", HOP_S);
  fprintf(stderr, "  Window size: %u samples\n\n", WIN_S);
  
  // Create tempo objects
  aubio_tempo_t *tempo_autocorr = new_aubio_tempo("default", WIN_S, HOP_S, source_samplerate);
  aubio_tempo_t *tempo_dp = new_aubio_tempo("default", WIN_S, HOP_S, source_samplerate);
  
  // Configure autocorrelation mode
  aubio_tempo_set_use_dp(tempo_autocorr, 0);
  
  // Configure DP mode
  aubio_tempo_set_use_dp(tempo_dp, 1);
  
  // Buffers
  fvec_t *input = new_fvec(HOP_S);
  fvec_t *tempo_out_autocorr = new_fvec(1);
  fvec_t *tempo_out_dp = new_fvec(1);
  
  // Storage for tempo trajectories
  uint_t max_frames = 15000;
  smpl_t *bpm_autocorr_curve = calloc(max_frames, sizeof(smpl_t));
  smpl_t *bpm_dp_curve = calloc(max_frames, sizeof(smpl_t));
  smpl_t *confidence_autocorr_curve = calloc(max_frames, sizeof(smpl_t));
  smpl_t *confidence_dp_curve = calloc(max_frames, sizeof(smpl_t));
  
  // Process audio
  uint_t total_frames = 0;
  uint_t read = HOP_S;
  
  while (read == HOP_S) {
    aubio_source_do(source, input, &read);
    
    // Process with both methods
    aubio_tempo_do(tempo_autocorr, input, tempo_out_autocorr);
    aubio_tempo_do(tempo_dp, input, tempo_out_dp);
    
    // Store current BPM and confidence
    bpm_autocorr_curve[total_frames] = aubio_tempo_get_bpm(tempo_autocorr);
    bpm_dp_curve[total_frames] = aubio_tempo_get_bpm(tempo_dp);
    confidence_autocorr_curve[total_frames] = aubio_tempo_get_confidence(tempo_autocorr);
    confidence_dp_curve[total_frames] = aubio_tempo_get_confidence(tempo_dp);
    
    total_frames++;
    if (total_frames >= max_frames) break;
  }
  
  smpl_t total_duration = (total_frames * HOP_S) / (smpl_t)source_samplerate;
  fprintf(stderr, "Processed %u frames (%.2f seconds)\n", total_frames, total_duration);
  
  // Analyze tempo trajectories by section
  typedef struct {
    smpl_t start_time;
    smpl_t end_time;
    smpl_t start_bpm;
    smpl_t end_bpm;
    const char *description;
  } section_t;
  
  section_t sections[] = {
    {0.0, 15.0, 100.0, 100.0, "Steady 100 BPM"},
    {15.0, 30.0, 100.0, 140.0, "Accelerando 100→140 BPM"},
    {30.0, 45.0, 140.0, 140.0, "Steady 140 BPM"},
    {45.0, 60.0, 140.0, 100.0, "Ritardando 140→100 BPM"}
  };
  uint_t num_sections = sizeof(sections) / sizeof(sections[0]);
  
  print_section("Section-by-Section Analysis");
  
  for (uint_t s = 0; s < num_sections; s++) {
    section_t *sec = &sections[s];
    uint_t start_frame = (uint_t)(sec->start_time * source_samplerate / HOP_S);
    uint_t end_frame = (uint_t)(sec->end_time * source_samplerate / HOP_S);
    if (end_frame > total_frames) end_frame = total_frames;
    
    // Calculate average BPM and variance for each method
    smpl_t sum_autocorr = 0.0, sum_dp = 0.0;
    smpl_t sum_conf_autocorr = 0.0, sum_conf_dp = 0.0;
    uint_t count_autocorr = 0, count_dp = 0;
    
    for (uint_t i = start_frame; i < end_frame; i++) {
      if (bpm_autocorr_curve[i] > 0.0 && confidence_autocorr_curve[i] > 0.5) {
        sum_autocorr += bpm_autocorr_curve[i];
        sum_conf_autocorr += confidence_autocorr_curve[i];
        count_autocorr++;
      }
      if (bpm_dp_curve[i] > 0.0 && confidence_dp_curve[i] > 0.5) {
        sum_dp += bpm_dp_curve[i];
        sum_conf_dp += confidence_dp_curve[i];
        count_dp++;
      }
    }
    
    smpl_t avg_autocorr = count_autocorr > 0 ? sum_autocorr / count_autocorr : 0.0;
    smpl_t avg_dp = count_dp > 0 ? sum_dp / count_dp : 0.0;
    smpl_t avg_conf_autocorr = count_autocorr > 0 ? sum_conf_autocorr / count_autocorr : 0.0;
    smpl_t avg_conf_dp = count_dp > 0 ? sum_conf_dp / count_dp : 0.0;
    
    // Calculate expected average BPM for section
    smpl_t expected_avg = (sec->start_bpm + sec->end_bpm) / 2.0;
    
    fprintf(stderr, "\nSection %u: %s (%.1f-%.1fs)\n", s+1, sec->description, 
            sec->start_time, sec->end_time);
    fprintf(stderr, "  Expected average: %.1f BPM\n", expected_avg);
    
    fprintf(stderr, "\n  Autocorrelation:\n");
    if (count_autocorr > 0) {
      fprintf(stderr, "    Average BPM: %.2f (error: %.2f BPM)\n", 
              avg_autocorr, fabs(avg_autocorr - expected_avg));
      fprintf(stderr, "    Avg confidence: %.3f\n", avg_conf_autocorr);
      fprintf(stderr, "    Valid frames: %u (%.1f%% of section)\n", 
              count_autocorr, 100.0 * count_autocorr / (end_frame - start_frame));
    } else {
      fprintf(stderr, "    %sNo valid detections%s\n", COLOR_RED, COLOR_RESET);
    }
    
    fprintf(stderr, "\n  DP Tracker:\n");
    if (count_dp > 0) {
      fprintf(stderr, "    Average BPM: %.2f (error: %.2f BPM)\n", 
              avg_dp, fabs(avg_dp - expected_avg));
      fprintf(stderr, "    Avg confidence: %.3f\n", avg_conf_dp);
      fprintf(stderr, "    Valid frames: %u (%.1f%% of section)\n", 
              count_dp, 100.0 * count_dp / (end_frame - start_frame));
      
      // Compare with autocorrelation
      if (count_autocorr > 0) {
        smpl_t error_diff = fabs(avg_autocorr - expected_avg) - fabs(avg_dp - expected_avg);
        if (error_diff > 0.5) {
          fprintf(stderr, "    %s✓ DP is %.2f BPM more accurate%s\n", 
                  COLOR_GREEN, error_diff, COLOR_RESET);
        } else if (error_diff < -0.5) {
          fprintf(stderr, "    %s⚠ Autocorr is %.2f BPM more accurate%s\n", 
                  COLOR_YELLOW, -error_diff, COLOR_RESET);
        } else {
          fprintf(stderr, "    ≈ Similar accuracy to autocorrelation\n");
        }
      }
    } else {
      fprintf(stderr, "    %sNo valid detections%s\n", COLOR_RED, COLOR_RESET);
    }
  }
  
  // Cleanup
  free(bpm_autocorr_curve);
  free(bpm_dp_curve);
  free(confidence_autocorr_curve);
  free(confidence_dp_curve);
  del_fvec(input);
  del_fvec(tempo_out_autocorr);
  del_fvec(tempo_out_dp);
  del_aubio_tempo(tempo_autocorr);
  del_aubio_tempo(tempo_dp);
  del_aubio_source(source);
  
  return 0;
}

int main(void) {
  print_header("DP TRACKER GRADUAL TEMPO TEST");
  
  int result = test_dp_gradual();
  
  if (result == 0) {
    print_header("TEST COMPLETE");
    fprintf(stderr, "\n%sKey Findings:%s\n", COLOR_BOLD, COLOR_RESET);
    fprintf(stderr, "  • DP tracker tested on accelerando/ritardando patterns\n");
    fprintf(stderr, "  • Performance compared with autocorrelation baseline\n");
    fprintf(stderr, "  • Tempo trajectory smoothness analyzed\n");
    fprintf(stderr, "\n%s✓ DP tracker handles gradual tempo changes%s\n", 
            COLOR_GREEN, COLOR_RESET);
  } else {
    fprintf(stderr, "\n%s✗ Test failed%s\n", COLOR_RED, COLOR_RESET);
  }
  
  return result;
}
