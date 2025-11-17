/*
  Comprehensive tempo tracking benchmark using tempogram
  Tests on both sudden changes (test_bpm_changes.wav) and gradual changes (test_bpm_gradual.wav)
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

// Test file 2: Gradual BPM changes (approximate sections)
static bpm_section_t gradual_sections[] = {
  {0.0, 10.0, 120.0},     // Stable start
  {10.0, 30.0, 135.0},    // Accelerando 120->150 (midpoint ~135)
  {30.0, 40.0, 150.0},    // Fast stable
  {40.0, 60.0, 120.0}     // Ritardando 150->90 (midpoint ~120)
};
static const uint_t num_gradual_sections = 4;

typedef struct {
  uint_t detected;
  smpl_t detected_bpm;
  smpl_t error;
  smpl_t response_time;
  smpl_t detection_time;
} section_result_t;

static void
run_benchmark(const char *filename, bpm_section_t *sections, uint_t num_sections,
              uint_t use_tempogram, const char *mode_name)
{
  uint_t win_s = 1024;
  uint_t hop_s = 256;
  uint_t samplerate = 0;
  uint_t read = 0;
  uint_t total_frames = 0;
  
  printf("\n=== TEMPO TRACKING BENCHMARK: %s ===\n", filename);
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
  
  // Enable tempogram if requested
  if (use_tempogram) {
    aubio_tempo_set_use_tempogram(tempo, 1);
  }
  
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
      if (current_time >= sections[i].start_time && 
          current_time < sections[i].end_time) {
        
        // Check if tempo is close to expected
        smpl_t error = fabs(current_bpm - sections[i].expected_bpm);
        
        if (!results[i].detected && error < 5.0) {
          // First detection in this section
          results[i].detected = 1;
          results[i].detected_bpm = current_bpm;
          results[i].error = error;
          results[i].detection_time = current_time;
          results[i].response_time = current_time - sections[i].start_time;
        } else if (results[i].detected && error < results[i].error) {
          // Better detection found
          results[i].detected_bpm = current_bpm;
          results[i].error = error;
        }
        break;
      }
    }
    
    total_frames++;
  } while (read == hop_s);
  
  // Print results
  printf("Section-by-Section Analysis:\n");
  printf("%-8s %-16s %-16s %-16s %-13s %-13s %s\n",
         "Section", "Time Range", "Expected BPM", "Detected BPM", 
         "Error", "Response", "Status");
  printf("%-8s %-16s %-16s %-16s %-13s %-13s %s\n",
         "-------", "----------", "------------", "------------",
         "-----", "--------", "------");
  
  uint_t total_detected = 0;
  smpl_t total_error = 0.0;
  smpl_t max_error = 0.0;
  smpl_t total_response = 0.0;
  smpl_t max_response = 0.0;
  uint_t response_count = 0;
  
  uint_t i;
  for (i = 0; i < num_sections; i++) {
    printf("%-8u %-5.1f-%-10.1f %-16.1f ",
           i + 1,
           sections[i].start_time,
           sections[i].end_time,
           sections[i].expected_bpm);
    
    if (results[i].detected) {
      printf("%-16.1f %-13.1f ",
             results[i].detected_bpm,
             results[i].error);
      
      if (i == 0) {
        printf("%-13s", "N/A");
      } else {
        printf("%-13.2f", results[i].response_time);
        total_response += results[i].response_time;
        if (results[i].response_time > max_response) {
          max_response = results[i].response_time;
        }
        response_count++;
      }
      
      printf(" ✓\n");
      
      total_detected++;
      total_error += results[i].error;
      if (results[i].error > max_error) {
        max_error = results[i].error;
      }
    } else {
      printf("%-16s %-13s %-13s ✗\n", "MISSED", "-", "-");
    }
  }
  
  // Summary statistics
  printf("\nOverall Metrics:\n");
  printf("  Sections Detected: %u / %u (%.1f%%)\n",
         total_detected, num_sections,
         100.0 * total_detected / num_sections);
  
  if (total_detected > 0) {
    smpl_t avg_error = total_error / total_detected;
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
  printf("║         COMPREHENSIVE TEMPO TRACKING BENCHMARK                ║\n");
  printf("║  Testing both sudden and gradual BPM changes                  ║\n");
  printf("╚════════════════════════════════════════════════════════════════╝\n");
  
  // Test 1: Sudden BPM changes - Autocorrelation mode
  run_benchmark("tests/test_bpm_changes.wav", sudden_sections, num_sudden_sections,
                0, "Autocorrelation (baseline)");
  
  // Test 2: Sudden BPM changes - Tempogram mode
  run_benchmark("tests/test_bpm_changes.wav", sudden_sections, num_sudden_sections,
                1, "Fourier Tempogram (new)");
  
  // Test 3: Gradual BPM changes - Autocorrelation mode
  run_benchmark("tests/test_bpm_gradual.wav", gradual_sections, num_gradual_sections,
                0, "Autocorrelation (baseline)");
  
  // Test 4: Gradual BPM changes - Tempogram mode  
  run_benchmark("tests/test_bpm_gradual.wav", gradual_sections, num_gradual_sections,
                1, "Fourier Tempogram (new)");
  
  printf("\n");
  printf("╔════════════════════════════════════════════════════════════════╗\n");
  printf("║                    BENCHMARK COMPLETE                          ║\n");
  printf("╚════════════════════════════════════════════════════════════════╝\n");
  printf("\n");
  
  return 0;
}
