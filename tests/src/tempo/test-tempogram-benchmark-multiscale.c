/*
  Comprehensive tempo tracking benchmark using multi-scale tempogram
  Tests Phase 3B: Multi-Scale Analysis on sudden tempo changes
*/

#include <aubio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
  smpl_t start_time;
  smpl_t end_time;
  smpl_t expected_bpm;
} bpm_section_t;

// Test file 1: Sudden BPM changes
static bpm_section_t sudden_sections[] = {
  {0.0, 10.0, 120.0},
  {10.0, 20.0, 140.0},
  {20.0, 30.0, 100.0},
  {30.0, 40.0, 160.0},
  {40.0, 50.0, 80.0},
  {50.0, 60.0, 120.0}
};
static const uint_t num_sudden_sections = 6;

typedef struct {
  uint_t detected;
  smpl_t detected_bpm;
  smpl_t error;
  smpl_t response_time;
  smpl_t detection_time;
} section_result_t;

static void
run_benchmark(const char *filename, bpm_section_t *sections, uint_t num_sections,
              const char *mode_name)
{
  uint_t win_s = 1024;
  uint_t hop_s = 256;
  uint_t samplerate = 0;
  uint_t read = 0;
  uint_t total_frames = 0;
  
  printf("\n=== MULTI-SCALE TEMPOGRAM BENCHMARK: %s ===\n", filename);
  printf("Mode: %s\n\n", mode_name);
  
  // Open source
  aubio_source_t *source = new_aubio_source(filename, samplerate, hop_s);
  if (!source) {
    fprintf(stderr, "Error: Could not open %s\n", filename);
    fprintf(stderr, "Please ensure test audio files exist in tests/ directory\n");
    return;
  }
  
  samplerate = aubio_source_get_samplerate(source);
  
  // Create tempo detector
  aubio_tempo_t *tempo = new_aubio_tempo("default", win_s, hop_s, samplerate);
  if (!tempo) {
    fprintf(stderr, "Error: Could not create tempo detector\n");
    del_aubio_source(source);
    return;
  }
  
  // Enable tempogram with multi-scale analysis
  aubio_tempo_set_use_tempogram(tempo, 1);
  aubio_tempo_set_multiscale_tempogram(tempo, 1);
  
  // Enable all optimizations for fair comparison
  aubio_tempo_set_multi_octave(tempo, 1);
  aubio_tempo_set_fft_autocorr(tempo, 1);
  aubio_tempo_set_dynamic_tempo(tempo, 1);
  
  fvec_t *samples = new_fvec(hop_s);
  fvec_t *tempo_out = new_fvec(1);  // Tempo output (beat detection)
  section_result_t *results = calloc(num_sections, sizeof(section_result_t));
  
  // Process audio
  do {
    aubio_source_do(source, samples, &read);
    aubio_tempo_do(tempo, samples, tempo_out);
    
    smpl_t current_time = (smpl_t)total_frames * hop_s / samplerate;
    smpl_t current_bpm = aubio_tempo_get_bpm(tempo);
    
    // Check which section we're in
    uint_t i;
    for (i = 0; i < num_sections; i++) {
      if (current_time >= sections[i].start_time && current_time < sections[i].end_time) {
        // We're in this section
        if (current_bpm > 0) {
          smpl_t error = fabs(current_bpm - sections[i].expected_bpm);
          
          // Consider detection valid if within 10 BPM and confidence > 0.5
          smpl_t confidence = aubio_tempo_get_confidence(tempo);
          if (error < 10.0 && confidence > 0.5) {
            // First detection or better detection in this section
            if (!results[i].detected || error < results[i].error) {
              results[i].detected = 1;
              results[i].detected_bpm = current_bpm;
              results[i].error = error;
              results[i].detection_time = current_time;
              
              // Calculate response time (time since section start)
              results[i].response_time = current_time - sections[i].start_time;
            }
          }
        }
        break;
      }
    }
    
    total_frames++;
  } while (read == hop_s);
  
  // Print results
  printf("Section-by-Section Analysis:\n");
  printf("Section  Time Range       Expected BPM     Detected BPM     Error         Response      Status\n");
  printf("-------  ----------       ------------     ------------     -----         --------      ------\n");
  
  uint_t detected_count = 0;
  smpl_t total_error = 0.0;
  smpl_t max_error = 0.0;
  smpl_t total_response = 0.0;
  smpl_t max_response = 0.0;
  
  uint_t i;
  for (i = 0; i < num_sections; i++) {
    printf("%-8u %-4.1f -%-6.1f %-16.1f ", i+1, sections[i].start_time, sections[i].end_time, sections[i].expected_bpm);
    
    if (results[i].detected) {
      printf("%-16.1f %-13.1f %-13.2f ✓\n", 
             results[i].detected_bpm, 
             results[i].error,
             results[i].response_time);
      detected_count++;
      total_error += results[i].error;
      if (results[i].error > max_error) {
        max_error = results[i].error;
      }
      total_response += results[i].response_time;
      if (results[i].response_time > max_response) {
        max_response = results[i].response_time;
      }
    } else {
      printf("MISSED           -             -             ✗\n");
    }
  }
  
  printf("\nOverall Metrics:\n");
  printf("  Sections Detected: %u / %u (%.1f%%)\n", detected_count, num_sections, 
         100.0 * detected_count / num_sections);
  
  if (detected_count > 0) {
    printf("  Average BPM Error: %.2f BPM\n", total_error / detected_count);
    printf("  Maximum BPM Error: %.2f BPM\n", max_error);
    printf("  Average Response Time: %.2f seconds\n", total_response / detected_count);
    printf("  Maximum Response Time: %.2f seconds\n", max_response);
  }
  
  // Cleanup
  free(results);
  del_fvec(samples);
  del_fvec(tempo_out);
  del_aubio_tempo(tempo);
  del_aubio_source(source);
}

int main(void)
{
  printf("╔════════════════════════════════════════════════════════════════╗\n");
  printf("║      MULTI-SCALE TEMPOGRAM BENCHMARK (Phase 3B)               ║\n");
  printf("║  Testing multi-resolution tempo analysis                      ║\n");
  printf("╚════════════════════════════════════════════════════════════════╝\n");
  
  run_benchmark("tests/test_bpm_changes.wav", sudden_sections, num_sudden_sections,
                "Multi-Scale Tempogram (256/512/1024 samples)");
  
  printf("\n╔════════════════════════════════════════════════════════════════╗\n");
  printf("║                    BENCHMARK COMPLETE                          ║\n");
  printf("╚════════════════════════════════════════════════════════════════╝\n");
  
  return 0;
}
