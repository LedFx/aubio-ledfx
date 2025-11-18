/*
  Integration test using real WAV files and ground truth
  
  This test processes actual audio to debug tempogram detection
*/

#include "aubio_priv.h"
#include "aubio.h"
#include "tempo/tempogram.h"
#include "utils_tests.h"
#include <stdio.h>
#include <string.h>

#define VERBOSE 1

// Helper: Create path to test file
#ifdef AUBIO_TEMPO_TEST_DIR
#define TEMPO_TEST_FILE(filename) AUBIO_TEMPO_TEST_DIR "/" filename
#else
#define TEMPO_TEST_FILE(filename) "tests/" filename
#endif

int main(int argc, char **argv)
{
  fprintf(stderr, "\n╔══════════════════════════════════════════════════════════════╗\n");
  fprintf(stderr, "║   TEMPOGRAM REAL AUDIO INTEGRATION TEST                      ║\n");
  fprintf(stderr, "╚══════════════════════════════════════════════════════════════╝\n\n");

  const char *test_file = TEMPO_TEST_FILE("test_bpm_changes.wav");
  
  if (argc > 1) {
    test_file = argv[1];
  }
  
  fprintf(stderr, "Test file: %s\n\n", test_file);
  
  // Open audio file
  aubio_source_t *source = new_aubio_source(test_file, 44100, 256);
  if (!source) {
    fprintf(stderr, "✗ Failed to open %s\n", test_file);
    fprintf(stderr, "   Make sure test audio files are available\n");
    return 1;
  }
  
  uint_t samplerate = aubio_source_get_samplerate(source);
  uint_t hop_s = 256;
  fprintf(stderr, "✓ Audio loaded: samplerate=%u, hop_size=%u\n", samplerate, hop_s);
  
  // Create tempo object
  uint_t win_s = 1024;
  aubio_tempo_t *tempo = new_aubio_tempo("default", win_s, hop_s, samplerate);
  if (!tempo) {
    fprintf(stderr, "✗ Failed to create tempo object\n");
    del_aubio_source(source);
    return 1;
  }
  fprintf(stderr, "✓ Tempo object created\n");
  
  // Enable tempogram mode
  uint_t result = aubio_tempo_set_use_tempogram(tempo, 1);
  if (result != AUBIO_OK) {
    fprintf(stderr, "✗ Failed to enable tempogram\n");
    del_aubio_tempo(tempo);
    del_aubio_source(source);
    return 1;
  }
  fprintf(stderr, "✓ Tempogram mode enabled\n\n");
  
  // Create buffers
  fvec_t *samples = new_fvec(hop_s);
  fvec_t *tempo_out = new_fvec(2);
  
  // Process audio
  uint_t read = 0;
  uint_t total_frames = 0;
  uint_t detection_count = 0;
  smpl_t last_bpm = 0.0;
  
  fprintf(stderr, "Processing audio...\n");
  fprintf(stderr, "%-8s %-10s %-12s %-12s %-12s\n", 
          "Frame", "Time(s)", "BPM", "Confidence", "Status");
  fprintf(stderr, "------------------------------------------------------------\n");
  
  // Expected sections from ground truth
  typedef struct {
    smpl_t start_time;
    smpl_t end_time;
    smpl_t expected_bpm;
  } section_t;
  
  section_t sections[] = {
    {0.0, 10.0, 120.0},
    {10.0, 20.0, 140.0},
    {20.0, 30.0, 100.0},
    {30.0, 40.0, 160.0},
    {40.0, 50.0, 80.0},
    {50.0, 60.0, 120.0}
  };
  uint_t num_sections = sizeof(sections) / sizeof(sections[0]);
  
  uint_t detected_sections[6] = {0};
  
  do {
    aubio_source_do(source, samples, &read);
    aubio_tempo_do(tempo, samples, tempo_out);
    
    smpl_t current_time = (smpl_t)total_frames * hop_s / samplerate;
    smpl_t current_bpm = aubio_tempo_get_bpm(tempo);
    smpl_t confidence = aubio_tempo_get_confidence(tempo);
    
    // Find current section
    uint_t current_section = 0;
    for (uint_t i = 0; i < num_sections; i++) {
      if (current_time >= sections[i].start_time && 
          current_time < sections[i].end_time) {
        current_section = i;
        break;
      }
    }
    
    // Check if we have a good detection
    if (current_bpm > 0 && current_section < num_sections) {
      smpl_t expected = sections[current_section].expected_bpm;
      smpl_t error = fabs(current_bpm - expected);
      
      if (error < 5.0 && !detected_sections[current_section]) {
        detected_sections[current_section] = 1;
        detection_count++;
        fprintf(stderr, "%-8u %-10.2f %-12.2f %-12.3f ✓ Section %u detected\n",
                total_frames, current_time, current_bpm, confidence, current_section + 1);
      }
    }
    
    // Log every 200 frames (~1.16 seconds) to see what's happening
    if (total_frames % 200 == 0) {
      const char *status = "";
      if (current_section < num_sections) {
        smpl_t expected = sections[current_section].expected_bpm;
        smpl_t error = fabs(current_bpm - expected);
        if (error < 5.0) {
          status = "✓";
        } else if (current_bpm > 0) {
          status = "✗";
        } else {
          status = "-";
        }
      }
      
      fprintf(stderr, "%-8u %-10.2f %-12.2f %-12.3f %s\n",
              total_frames, current_time, current_bpm, confidence, status);
    }
    
    // Track BPM changes
    if (current_bpm != last_bpm && current_bpm > 0) {
      if (VERBOSE && total_frames > 100) {  // Skip initial transient
        fprintf(stderr, "  >> BPM changed: %.2f → %.2f at frame %u (%.2fs)\n",
                last_bpm, current_bpm, total_frames, current_time);
      }
      last_bpm = current_bpm;
    }
    
    total_frames++;
  } while (read == hop_s);
  
  fprintf(stderr, "\n");
  fprintf(stderr, "═══════════════════════════════════════════════════════════════\n");
  fprintf(stderr, "RESULTS\n");
  fprintf(stderr, "═══════════════════════════════════════════════════════════════\n");
  fprintf(stderr, "Total frames processed: %u (%.2f seconds)\n", 
          total_frames, (smpl_t)total_frames * hop_s / samplerate);
  fprintf(stderr, "Sections detected: %u / %u (%.1f%%)\n", 
          detection_count, num_sections, 100.0 * detection_count / num_sections);
  
  fprintf(stderr, "\nSection-by-section:\n");
  for (uint_t i = 0; i < num_sections; i++) {
    fprintf(stderr, "  Section %u (%.0f BPM): %s\n", 
            i + 1, sections[i].expected_bpm, 
            detected_sections[i] ? "✓ DETECTED" : "✗ MISSED");
  }
  
  // Cleanup
  del_fvec(samples);
  del_fvec(tempo_out);
  del_aubio_tempo(tempo);
  del_aubio_source(source);
  
  fprintf(stderr, "\n");
  if (detection_count > 0) {
    fprintf(stderr, "✓ Test PASSED (at least one section detected)\n");
    return 0;
  } else {
    fprintf(stderr, "✗ Test FAILED (no sections detected)\n");
    return 1;
  }
}
