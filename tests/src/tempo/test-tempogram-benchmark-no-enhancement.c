/*
  Test tempogram benchmark WITHOUT onset enhancement
  to measure baseline performance before Phase 3A
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
run_benchmark_no_enhancement(const char *filename, bpm_section_t *sections, uint_t num_sections)
{
  uint_t win_s = 1024;
  uint_t hop_s = 256;
  uint_t samplerate = 0;
  uint_t read = 0;
  uint_t total_frames = 0;
  
  printf("\n=== TEMPOGRAM BENCHMARK (NO ONSET ENHANCEMENT): %s ===\n\n", filename);
  
  // Open source
  aubio_source_t *source = new_aubio_source(filename, samplerate, hop_s);
  if (!source) {
    fprintf(stderr, "Error: Could not open %s\n", filename);
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
  
  // Enable tempogram
  aubio_tempo_set_use_tempogram(tempo, 1);
  
  // DISABLE onset enhancement to test baseline
  extern uint_t aubio_tempo_set_onset_enhancement(aubio_tempo_t *, uint_t);
  aubio_tempo_set_onset_enhancement(tempo, 0);
  
  fvec_t *samples = new_fvec(hop_s);
  fvec_t *tempo_out = new_fvec(1);
  section_result_t *results = calloc(num_sections, sizeof(section_result_t));
  
  // Process audio
  do {
    aubio_source_do(source, samples, &read);
    aubio_tempo_do(tempo, samples, tempo_out);
    
    smpl_t current_time = (smpl_t)total_frames * hop_s / samplerate;
    smpl_t current_bpm = aubio_tempo_get_bpm(tempo);
    smpl_t confidence = aubio_tempo_get_confidence(tempo);
    
    // Check which section we're in
    uint_t i;
    for (i = 0; i < num_sections; i++) {
      if (current_time >= sections[i].start_time && current_time < sections[i].end_time) {
        // Check if BPM is close to expected (within 10%)
        smpl_t error = fabs(current_bpm - sections[i].expected_bpm);
        smpl_t tolerance = sections[i].expected_bpm * 0.10;  // 10% tolerance
        
        if (error < tolerance && confidence > 0.3) {
          if (!results[i].detected) {
            // First detection in this section
            results[i].detected = 1;
            results[i].detected_bpm = current_bpm;
            results[i].error = error;
            results[i].detection_time = current_time;
            results[i].response_time = current_time - sections[i].start_time;
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
  smpl_t total_error = 0;
  smpl_t max_error = 0;
  smpl_t total_response = 0;
  smpl_t max_response = 0;
  uint_t response_count = 0;
  
  uint_t i;
  for (i = 0; i < num_sections; i++) {
    printf("%-8u %-4.1f -%-4.1f       %-12.1f     ",
           i + 1, sections[i].start_time, sections[i].end_time, sections[i].expected_bpm);
    
    if (results[i].detected) {
      printf("%-12.1f     %-5.1f         %-5.2f         ✓\n",
             results[i].detected_bpm, results[i].error, results[i].response_time);
      detected_count++;
      total_error += results[i].error;
      if (results[i].error > max_error) max_error = results[i].error;
      total_response += results[i].response_time;
      if (results[i].response_time > max_response) max_response = results[i].response_time;
      response_count++;
    } else {
      printf("%-12s     %-5s         %-5s         ✗\n", "MISSED", "-", "-");
    }
  }
  
  printf("\nOverall Metrics:\n");
  printf("  Sections Detected: %u / %u (%.1f%%)\n",
         detected_count, num_sections, 100.0 * detected_count / num_sections);
  
  if (detected_count > 0) {
    smpl_t avg_error = total_error / detected_count;
    printf("  Average BPM Error: %.2f BPM\n", avg_error);
    printf("  Maximum BPM Error: %.2f BPM\n", max_error);
  }
  
  if (response_count > 0) {
    smpl_t avg_response = total_response / response_count;
    printf("  Average Response Time: %.2f seconds\n", avg_response);
    printf("  Maximum Response Time: %.2f seconds\n", max_response);
  }
  
  // Cleanup
  free(results);
  del_fvec(tempo_out);
  del_fvec(samples);
  del_aubio_tempo(tempo);
  del_aubio_source(source);
}

int main(void)
{
  printf("\n");
  printf("╔════════════════════════════════════════════════════════════════╗\n");
  printf("║   TEMPOGRAM BASELINE (NO ONSET ENHANCEMENT)                   ║\n");
  printf("║   Measuring performance before Phase 3A                       ║\n");
  printf("╚════════════════════════════════════════════════════════════════╝\n");
  
  run_benchmark_no_enhancement("tests/test_bpm_changes.wav", sudden_sections, num_sudden_sections);
  
  printf("\n");
  printf("╔════════════════════════════════════════════════════════════════╗\n");
  printf("║                    BASELINE TEST COMPLETE                      ║\n");
  printf("╚════════════════════════════════════════════════════════════════╝\n");
  printf("\n");
  
  return 0;
}
