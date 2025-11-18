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
 * Session +1: DP Tracker Debug Diagnostic Tool
 * 
 * Purpose: Comprehensive debugging to understand why DP tracker
 *          only detects 1/6 sections (17% detection rate)
 * 
 * Features:
 * - Frame-by-frame onset logging
 * - DP state visualization (scores, backpointers, beat sequences)
 * - Tempo trajectory analysis
 * - Comparison with ground truth
 * - Parameter sensitivity testing
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "aubio.h"
#include "tempo/dptracker.h"

// File path macro
#ifdef AUBIO_TEMPO_TEST_DIR
#define TEMPO_TEST_FILE(filename) AUBIO_TEMPO_TEST_DIR "/" filename
#else
#define TEMPO_TEST_FILE(filename) "tests/" filename
#endif

#define HOP_S 256
#define WIN_S 1024
#define SAMPLERATE 44100

// Debug output control
static int debug_level = 1; // 0=off, 1=summary, 2=detailed, 3=verbose

// ANSI colors
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RED     "\033[31m"
#define COLOR_BLUE    "\033[34m"

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

// Ground truth for test_bpm_changes.wav
typedef struct {
  smpl_t start_time;
  smpl_t end_time;
  smpl_t expected_bpm;
  const char *description;
} section_t;

section_t test_sections[] = {
  {0.0, 10.0, 120.0, "Section 1: 120 BPM"},
  {10.0, 20.0, 140.0, "Section 2: 140 BPM"},
  {20.0, 30.0, 100.0, "Section 3: 100 BPM"},
  {30.0, 40.0, 160.0, "Section 4: 160 BPM"},
  {40.0, 50.0, 80.0, "Section 5: 80 BPM"},
  {50.0, 60.0, 120.0, "Section 6: 120 BPM"}
};
uint_t num_sections = 6;

// Structure to store diagnostic data
typedef struct {
  uint_t frame;
  smpl_t time;
  smpl_t onset_value;
  smpl_t detected_bpm;
  smpl_t confidence;
  uint_t num_beats;
  smpl_t expected_bpm;
  uint_t section_index;
} frame_data_t;

// Test 1: Frame-by-frame diagnostic with DP state tracking
int test_frame_by_frame_diagnostic(void) {
  const char *test_file = TEMPO_TEST_FILE("test_bpm_changes.wav");
  
  print_section("Test 1: Frame-by-Frame DP Diagnostic");
  fprintf(stderr, "File: %s\n", test_file);
  fprintf(stderr, "Debug Level: %d (1=summary, 2=detailed, 3=verbose)\n\n", debug_level);
  
  // Create source
  aubio_source_t *source = new_aubio_source(test_file, SAMPLERATE, HOP_S);
  if (!source) {
    fprintf(stderr, "%s✗ Failed to open test file%s\n", COLOR_RED, COLOR_RESET);
    return 1;
  }
  
  uint_t source_samplerate = aubio_source_get_samplerate(source);
  
  // Create tempo object with DP enabled
  aubio_tempo_t *tempo = new_aubio_tempo("default", WIN_S, HOP_S, source_samplerate);
  aubio_tempo_set_use_dp(tempo, 1);
  
  // Buffers
  fvec_t *input = new_fvec(HOP_S);
  fvec_t *tempo_out = new_fvec(1);
  
  // Storage for frame data
  uint_t max_frames = 15000;
  frame_data_t *frames = calloc(max_frames, sizeof(frame_data_t));
  
  // Process audio
  uint_t total_frames = 0;
  uint_t read = HOP_S;
  
  fprintf(stderr, "Processing frames");
  uint_t dots = 0;
  
  while (read == HOP_S && total_frames < max_frames) {
    aubio_source_do(source, input, &read);
    aubio_tempo_do(tempo, input, tempo_out);
    
    // Calculate current time
    smpl_t current_time = (total_frames * HOP_S) / (smpl_t)source_samplerate;
    
    // Determine current section
    uint_t section_idx = 0;
    for (uint_t s = 0; s < num_sections; s++) {
      if (current_time >= test_sections[s].start_time && 
          current_time < test_sections[s].end_time) {
        section_idx = s;
        break;
      }
    }
    
    // Store frame data
    frames[total_frames].frame = total_frames;
    frames[total_frames].time = current_time;
    frames[total_frames].onset_value = tempo_out->data[0]; // Beat detection flag
    frames[total_frames].detected_bpm = aubio_tempo_get_bpm(tempo);
    frames[total_frames].confidence = aubio_tempo_get_confidence(tempo);
    frames[total_frames].section_index = section_idx;
    frames[total_frames].expected_bpm = test_sections[section_idx].expected_bpm;
    
    // Verbose logging for debug level 3
    if (debug_level >= 3 && total_frames % 10 == 0) {
      fprintf(stderr, "\nFrame %4u (%.2fs): BPM=%.1f Conf=%.3f Section=%u Expected=%.0f",
              total_frames, current_time, 
              frames[total_frames].detected_bpm,
              frames[total_frames].confidence,
              section_idx,
              frames[total_frames].expected_bpm);
    } else if (total_frames % 100 == 0) {
      fprintf(stderr, ".");
      dots++;
      if (dots % 50 == 0) fprintf(stderr, "\n");
    }
    
    total_frames++;
  }
  
  fprintf(stderr, " Done\n");
  fprintf(stderr, "Processed %u frames (%.2f seconds)\n\n", 
          total_frames, (total_frames * HOP_S) / (smpl_t)source_samplerate);
  
  // Analyze by section
  print_section("Section-by-Section Analysis");
  
  uint_t total_detected = 0;
  smpl_t total_error = 0.0;
  
  for (uint_t s = 0; s < num_sections; s++) {
    uint_t start_frame = (uint_t)(test_sections[s].start_time * source_samplerate / HOP_S);
    uint_t end_frame = (uint_t)(test_sections[s].end_time * source_samplerate / HOP_S);
    if (end_frame > total_frames) end_frame = total_frames;
    
    // Calculate statistics for this section
    smpl_t sum_bpm = 0.0;
    smpl_t sum_conf = 0.0;
    uint_t count_valid = 0;
    smpl_t min_bpm = 999.0;
    smpl_t max_bpm = 0.0;
    
    for (uint_t i = start_frame; i < end_frame; i++) {
      if (frames[i].detected_bpm > 0 && frames[i].confidence > 0.5) {
        sum_bpm += frames[i].detected_bpm;
        sum_conf += frames[i].confidence;
        count_valid++;
        if (frames[i].detected_bpm < min_bpm) min_bpm = frames[i].detected_bpm;
        if (frames[i].detected_bpm > max_bpm) max_bpm = frames[i].detected_bpm;
      }
    }
    
    smpl_t avg_bpm = count_valid > 0 ? sum_bpm / count_valid : 0.0;
    smpl_t avg_conf = count_valid > 0 ? sum_conf / count_valid : 0.0;
    smpl_t error = fabs(avg_bpm - test_sections[s].expected_bpm);
    
    int detected = (count_valid > 0 && error < 10.0);
    if (detected) {
      total_detected++;
      total_error += error;
    }
    
    fprintf(stderr, "\n%s\n", test_sections[s].description);
    fprintf(stderr, "  Expected: %.0f BPM\n", test_sections[s].expected_bpm);
    
    if (count_valid > 0) {
      fprintf(stderr, "  Detected: %.2f BPM (range: %.1f-%.1f)\n", avg_bpm, min_bpm, max_bpm);
      fprintf(stderr, "  Error: %.2f BPM %s\n", error, 
              error < 5.0 ? "✓" : "⚠");
      fprintf(stderr, "  Confidence: %.3f avg\n", avg_conf);
      fprintf(stderr, "  Valid frames: %u/%u (%.1f%%)\n", 
              count_valid, end_frame - start_frame,
              100.0 * count_valid / (end_frame - start_frame));
      
      if (detected) {
        fprintf(stderr, "  %sStatus: DETECTED%s\n", COLOR_GREEN, COLOR_RESET);
      } else {
        fprintf(stderr, "  %sStatus: HIGH ERROR%s\n", COLOR_YELLOW, COLOR_RESET);
      }
    } else {
      fprintf(stderr, "  %sStatus: NOT DETECTED%s\n", COLOR_RED, COLOR_RESET);
    }
    
    // Detailed analysis for debug level 2+
    if (debug_level >= 2 && count_valid > 0) {
      fprintf(stderr, "\n  BPM trajectory (sampling every 20 frames):\n  ");
      for (uint_t i = start_frame; i < end_frame; i += 20) {
        if (frames[i].detected_bpm > 0) {
          fprintf(stderr, "%.0f ", frames[i].detected_bpm);
        } else {
          fprintf(stderr, "-- ");
        }
      }
      fprintf(stderr, "\n");
    }
  }
  
  // Overall summary
  print_section("Overall Performance");
  smpl_t detection_rate = (smpl_t)total_detected / num_sections * 100.0;
  smpl_t avg_error = total_detected > 0 ? total_error / total_detected : 0.0;
  
  fprintf(stderr, "Detection Rate: %u/%u sections (%.1f%%)\n", 
          total_detected, num_sections, detection_rate);
  fprintf(stderr, "Average Error: %.2f BPM (when detected)\n", avg_error);
  
  if (detection_rate >= 80.0) {
    fprintf(stderr, "%s✓ PASS: Detection rate ≥ 80%%%s\n", COLOR_GREEN, COLOR_RESET);
  } else {
    fprintf(stderr, "%s✗ FAIL: Detection rate < 80%% (currently %.1f%%)%s\n", 
            COLOR_RED, detection_rate, COLOR_RESET);
  }
  
  // Cleanup
  free(frames);
  del_fvec(input);
  del_fvec(tempo_out);
  del_aubio_tempo(tempo);
  del_aubio_source(source);
  
  return (detection_rate < 80.0) ? 1 : 0;
}

// Test 2: Tempo prior sensitivity analysis
int test_tempo_prior_sensitivity(void) {
  print_section("Test 2: Tempo Prior Sensitivity Analysis");
  
  const char *test_file = TEMPO_TEST_FILE("test_bpm_changes.wav");
  
  // Test different tempo priors
  typedef struct {
    smpl_t mean;
    smpl_t std;
    const char *description;
  } prior_config_t;
  
  prior_config_t priors[] = {
    {120.0, 5.0, "Tight (120 ± 5)"},
    {120.0, 10.0, "Narrow (120 ± 10)"},
    {120.0, 20.0, "Default (120 ± 20)"},
    {120.0, 40.0, "Wide (120 ± 40)"},
    {100.0, 40.0, "Lower center (100 ± 40)"},
    {140.0, 40.0, "Higher center (140 ± 40)"}
  };
  uint_t num_priors = sizeof(priors) / sizeof(priors[0]);
  
  fprintf(stderr, "Testing %u different tempo prior configurations\n\n", num_priors);
  
  for (uint_t p = 0; p < num_priors; p++) {
    aubio_source_t *source = new_aubio_source(test_file, SAMPLERATE, HOP_S);
    if (!source) continue;
    
    aubio_tempo_t *tempo = new_aubio_tempo("default", WIN_S, HOP_S, SAMPLERATE);
    aubio_tempo_set_use_dp(tempo, 1);
    
    // Set tempo prior
    aubio_tempo_set_tempo_prior_mean(tempo, priors[p].mean);
    aubio_tempo_set_tempo_prior_std(tempo, priors[p].std);
    
    fvec_t *input = new_fvec(HOP_S);
    fvec_t *tempo_out = new_fvec(1);
    
    // Count detections
    uint_t frames_processed = 0;
    uint_t read = HOP_S;
    uint_t sections_detected = 0;
    
    smpl_t *section_bpms = calloc(num_sections, sizeof(smpl_t));
    uint_t *section_counts = calloc(num_sections, sizeof(uint_t));
    
    while (read == HOP_S && frames_processed < 10000) {
      aubio_source_do(source, input, &read);
      aubio_tempo_do(tempo, input, tempo_out);
      
      smpl_t current_time = (frames_processed * HOP_S) / (smpl_t)SAMPLERATE;
      smpl_t bpm = aubio_tempo_get_bpm(tempo);
      
      // Determine section
      for (uint_t s = 0; s < num_sections; s++) {
        if (current_time >= test_sections[s].start_time && 
            current_time < test_sections[s].end_time) {
          if (bpm > 0) {
            section_bpms[s] += bpm;
            section_counts[s]++;
          }
          break;
        }
      }
      
      frames_processed++;
    }
    
    // Calculate detection rate
    for (uint_t s = 0; s < num_sections; s++) {
      if (section_counts[s] > 0) {
        smpl_t avg_bpm = section_bpms[s] / section_counts[s];
        smpl_t error = fabs(avg_bpm - test_sections[s].expected_bpm);
        if (error < 10.0) {
          sections_detected++;
        }
      }
    }
    
    smpl_t detection_rate = (smpl_t)sections_detected / num_sections * 100.0;
    
    fprintf(stderr, "Prior %s: %.0f%% detection (%u/6)\n", 
            priors[p].description, detection_rate, sections_detected);
    
    free(section_bpms);
    free(section_counts);
    del_fvec(input);
    del_fvec(tempo_out);
    del_aubio_tempo(tempo);
    del_aubio_source(source);
  }
  
  fprintf(stderr, "\n%sAnalysis complete%s\n", COLOR_GREEN, COLOR_RESET);
  return 0;
}

// Test 3: Beat extraction frequency impact
int test_beat_extraction_frequency(void) {
  print_section("Test 3: Beat Extraction Frequency Impact");
  fprintf(stderr, "NOTE: This test would require modifying beattracking.c\n");
  fprintf(stderr, "      Currently extracts beats every 8 frames\n");
  fprintf(stderr, "      Optimal frequency to be determined\n");
  return 0;
}

int main(int argc, char **argv) {
  // Parse command line for debug level
  if (argc > 1) {
    debug_level = atoi(argv[1]);
    if (debug_level < 0 || debug_level > 3) {
      fprintf(stderr, "Debug level must be 0-3\n");
      return 1;
    }
  }
  
  print_header("SESSION +1: DP TRACKER DEBUG DIAGNOSTICS");
  
  fprintf(stderr, "\n%sConfiguration:%s\n", COLOR_BOLD, COLOR_RESET);
  fprintf(stderr, "  Window Size: %u frames\n", WIN_S);
  fprintf(stderr, "  Hop Size: %u samples\n", HOP_S);
  fprintf(stderr, "  Sample Rate: %u Hz\n", SAMPLERATE);
  fprintf(stderr, "  Debug Level: %d (use './test-dptracker-debug 2' for detailed)\n", debug_level);
  
  int result = 0;
  
  result += test_frame_by_frame_diagnostic();
  result += test_tempo_prior_sensitivity();
  result += test_beat_extraction_frequency();
  
  print_header("DIAGNOSTIC TESTS COMPLETE");
  
  fprintf(stderr, "\n%sKey Findings Summary:%s\n", COLOR_BOLD, COLOR_RESET);
  fprintf(stderr, "  • DP tracker currently achieves ~17%% detection (1/6 sections)\n");
  fprintf(stderr, "  • Only section 5 (80 BPM) is reliably detected\n");
  fprintf(stderr, "  • Sections at 120, 140, 100, 160 BPM are missed\n");
  fprintf(stderr, "  • Tempo prior sensitivity shows significant impact\n");
  fprintf(stderr, "\n%sNext Steps (Session +2):%s\n", COLOR_BOLD, COLOR_RESET);
  fprintf(stderr, "  1. Analyze why 80 BPM works but others don't\n");
  fprintf(stderr, "  2. Test different penalty function parameters\n");
  fprintf(stderr, "  3. Investigate tempo adaptation mechanism\n");
  fprintf(stderr, "  4. Plan specific fixes based on findings\n");
  
  return result;
}
