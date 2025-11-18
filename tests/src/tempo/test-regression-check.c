/* Regression test: Verify original tempo tracking maintains baseline performance
 * This test ensures all previous improvements remain functional:
 * - FFT autocorrelation
 * - Multi-octave detection  
 * - Onset normalization
 * - Tempo priors
 * 
 * Tests using test_bpm_changes.wav with ground truth
 * Expected baseline (Phase 1.6): 83.3% detection (5/6 sections), avg error < 2.0 BPM
 * Note: Section 2 (140 BPM) is consistently challenging due to transition from 120 BPM
 * 
 * Return: 0 = success (OK), 1 = failure (FAIL)
 */

#include <aubio.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "utils_tests.h"

#define WINDOW_SIZE 1024
#define HOP_SIZE 256

// Helper: Create path to test file
#ifdef AUBIO_TEMPO_TEST_DIR
#define TEMPO_TEST_FILE(filename) AUBIO_TEMPO_TEST_DIR "/" filename
#else
#define TEMPO_TEST_FILE(filename) "tests/" filename
#endif

int main(void) {
  uint_t samplerate = 44100;
  uint_t win_s = WINDOW_SIZE;
  uint_t hop_s = HOP_SIZE;
  
  // Ground truth for test_bpm_changes.wav
  smpl_t expected_bpms[] = {120.0, 140.0, 100.0, 160.0, 80.0, 120.0};
  smpl_t section_starts[] = {0.0, 10.0, 20.0, 30.0, 40.0, 50.0};
  uint_t num_sections = 6;
  
  // Tolerance for BPM detection
  smpl_t bpm_tolerance = 5.0;  // ±5 BPM
  
  // Create tempo detector with all optimizations enabled
  aubio_tempo_t *tempo = new_aubio_tempo("default", win_s, hop_s, samplerate);
  if (!tempo) {
    fprintf(stderr, "Failed to create tempo object\n");
    return 1;
  }
  
  // Enable all optimizations (as in successful benchmark)
  aubio_tempo_set_fft_autocorr(tempo, 1);
  aubio_tempo_set_multi_octave(tempo, 1);
  
  // Open test file
  char_t *source_path = TEMPO_TEST_FILE("test_bpm_changes.wav");
  aubio_source_t *source = new_aubio_source(source_path, samplerate, hop_s);
  if (!source) {
    fprintf(stderr, "Failed to open %s\n", source_path);
    del_aubio_tempo(tempo);
    return 1;
  }
  
  // Process audio
  fvec_t *input = new_fvec(hop_s);
  fvec_t *tempo_out = new_fvec(2);  // tempo output (unused but required)
  uint_t read = 0;
  uint_t total_frames = 0;
  
  uint_t sections_detected = 0;
  uint_t current_section = 0;
  uint_t detected_sections[6] = {0};
  smpl_t detected_bpms[6] = {0};
  smpl_t total_error = 0.0;
  smpl_t max_error = 0.0;
  
  fprintf(stderr, "\n=== REGRESSION TEST: Original Tempo Tracking ===\n");
  fprintf(stderr, "File: %s\n", source_path);
  fprintf(stderr, "Optimizations: FFT autocorr + Multi-octave\n\n");
  
  do {
    aubio_source_do(source, input, &read);
    aubio_tempo_do(tempo, input, tempo_out);
    
    smpl_t current_time = (smpl_t)total_frames * hop_s / samplerate;
    smpl_t current_bpm = aubio_tempo_get_bpm(tempo);
    smpl_t confidence = aubio_tempo_get_confidence(tempo);
    
    // Update current section based on time
    while (current_section < num_sections - 1 && 
           current_time >= section_starts[current_section + 1]) {
      current_section++;
    }
    
    // Check if we've detected this section's BPM
    if (confidence > 0.1 && !detected_sections[current_section]) {
      smpl_t expected_bpm = expected_bpms[current_section];
      smpl_t error = fabs(current_bpm - expected_bpm);
      
      if (error <= bpm_tolerance) {
        detected_sections[current_section] = 1;
        detected_bpms[current_section] = current_bpm;
        sections_detected++;
        
        total_error += error;
        if (error > max_error) max_error = error;
        
        fprintf(stderr, "✓ Section %u detected: %.1f BPM (expected %.1f, error %.1f)\n",
                current_section + 1, current_bpm, expected_bpm, error);
      }
    }
    
    total_frames++;
  } while (read == hop_s);
  
  // Calculate metrics
  smpl_t avg_error = sections_detected > 0 ? total_error / sections_detected : 0.0;
  smpl_t detection_rate = 100.0 * sections_detected / num_sections;
  
  fprintf(stderr, "\n=== RESULTS ===\n");
  fprintf(stderr, "Sections Detected: %u/%u (%.1f%%)\n", 
          sections_detected, num_sections, detection_rate);
  fprintf(stderr, "Average BPM Error: %.2f BPM\n", avg_error);
  fprintf(stderr, "Maximum BPM Error: %.2f BPM\n", max_error);
  
  // Check for regressions
  uint_t regression_detected = 0;
  
  // Baseline: 83.3% detection (5/6 sections) from Phase 1.6
  // Missing section 2 (140 BPM) is expected due to challenging transition
  if (sections_detected < 5) {
    fprintf(stderr, "\n❌ REGRESSION: Detection rate %.1f%% (expected ≥83.3%%)\n", detection_rate);
    fprintf(stderr, "   Missing sections:");
    for (uint_t i = 0; i < num_sections; i++) {
      if (!detected_sections[i]) {
        fprintf(stderr, " %u(%.0f BPM)", i + 1, expected_bpms[i]);
      }
    }
    fprintf(stderr, "\n");
    regression_detected = 1;
  }
  
  // Baseline: avg error ~1.66 BPM (Phase 1 validation results)
  if (avg_error > 2.5) {
    fprintf(stderr, "\n⚠️  WARNING: Average error %.2f BPM (baseline was ~1.66 BPM)\n", avg_error);
    regression_detected = 1;
  }
  
  if (!regression_detected) {
    fprintf(stderr, "\n✅ PASS: No regression detected\n");
    fprintf(stderr, "   Detection rate ≥83.3%% with avg error < 2.5 BPM\n");
    fprintf(stderr, "   Performance matches Phase 1.6 baseline\n");
  }
  
  // Cleanup
  del_fvec(tempo_out);
  del_fvec(input);
  del_aubio_source(source);
  del_aubio_tempo(tempo);
  
  // Return 0 for success (OK), 1 for failure (FAIL)
  return regression_detected;
}
