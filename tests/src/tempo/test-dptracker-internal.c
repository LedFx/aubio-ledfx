/*
  Session +4: Deep DP Investigation with Behavioral Analysis
  
  This test compares DP tracker behavior with synthetic vs real audio
  and tests parameter sensitivity to understand failure mode.
*/

#include <aubio.h>
#include "utils_tests.h"
#include <stdio.h>

#define TEST_ABS(x) ((x) < 0 ? -(x) : (x))

void test_synthetic_beats() {
  fprintf(stderr, "\n╔══════════════════════════════════════════════════════════╗\n");
  fprintf(stderr, "║  SYNTHETIC BEATS TEST                                     ║\n");
  fprintf(stderr, "╚══════════════════════════════════════════════════════════╝\n\n");
  
  uint_t win_s = 512;
  uint_t hop_s = 256;
  uint_t samplerate = 44100;
  
  aubio_dptracker_t *dp = new_aubio_dptracker(win_s, hop_s, samplerate);
  
  // Set tempo for 140 BPM
  aubio_dptracker_set_tempo(dp, 140.0, 20.0);
  
  // Feed synthetic beats at 140 BPM
  // 140 BPM → 74 frames per beat
  uint_t interval = 74;
  uint_t total_frames = 400;
  
  fprintf(stderr, "Feeding %u frames with beats every %u frames (140 BPM)...\n\n", total_frames, interval);
  
  for (uint_t i = 0; i < total_frames; i++) {
    smpl_t onset = (i % interval == 0) ? 1.0 : 0.0;
    aubio_dptracker_do(dp, onset);
  }
  
  // Extract beats ONCE at end
  fvec_t *beats = new_fvec(100);
  aubio_dptracker_get_beats(dp, beats);
  smpl_t bpm = aubio_dptracker_get_bpm(dp);
  
  fprintf(stderr, "Results:\n");
  fprintf(stderr, "  Detected BPM: %.2f (expected: 140.0)\n", bpm);
  fprintf(stderr, "  Status: %s\n", (bpm >= 135.0 && bpm <= 145.0) ? "✅ PASS" : "❌ FAIL");
  
  del_fvec(beats);
  del_aubio_dptracker(dp);
}

void test_real_audio() {
  fprintf(stderr, "\n╔══════════════════════════════════════════════════════════╗\n");
  fprintf(stderr, "║  REAL AUDIO TEST                                         ║\n");
  fprintf(stderr, "╚══════════════════════════════════════════════════════════╝\n\n");
  
  uint_t samplerate = 44100;
  uint_t win_s = 512;
  uint_t hop_s = 256;
  
  // Create tempo tracker with DP enabled
  aubio_tempo_t *tempo = new_aubio_tempo("default", win_s, hop_s, samplerate);
  aubio_tempo_set_use_dp(tempo, 1);
  
  // Open test file
  char_t *source_path = "/home/runner/work/aubio-ledfx/aubio-ledfx/tests/sounds/test_bpm_120.wav";
  aubio_source_t *source = new_aubio_source(source_path, samplerate, hop_s);
  
  if (!source) {
    fprintf(stderr, "ERROR: Could not open %s\n", source_path);
    del_aubio_tempo(tempo);
    return;
  }
  
  fprintf(stderr, "Processing: test_bpm_120.wav\n");
  fprintf(stderr, "Expected tempo: 120 BPM\n\n");
  
  fvec_t *in = new_fvec(hop_s);
  fvec_t *out = new_fvec(1);
  uint_t read = 0;
  uint_t frame_count = 0;
  
  smpl_t last_bpm = 0.0;
  
  // Process first 2 seconds
  uint_t max_frames = 350;
  
  do {
    aubio_source_do(source, in, &read);
    aubio_tempo_do(tempo, in, out);
    
    last_bpm = aubio_tempo_get_bpm(tempo);
    frame_count++;
  } while (read == hop_s && frame_count < max_frames);
  
  fprintf(stderr, "Results:\n");
  fprintf(stderr, "  Frames processed: %u\n", frame_count);
  fprintf(stderr, "  Final BPM: %.2f (expected: 120.0)\n", last_bpm);
  
  if (last_bpm >= 115.0 && last_bpm <= 125.0) {
    fprintf(stderr, "  Status: ✅ PASS\n");
  } else if (last_bpm >= 70.0 && last_bpm <= 85.0) {
    fprintf(stderr, "  Status: ❌ FAIL - STUCK AT ~%.0f BPM\n", last_bpm);
  } else {
    fprintf(stderr, "  Status: ❌ FAIL\n");
  }
  
  del_fvec(out);
  del_fvec(in);
  del_aubio_source(source);
  del_aubio_tempo(tempo);
}

void test_parameter_sensitivity() {
  fprintf(stderr, "\n╔══════════════════════════════════════════════════════════╗\n");
  fprintf(stderr, "║  PARAMETER SENSITIVITY ANALYSIS                          ║\n");
  fprintf(stderr, "╚══════════════════════════════════════════════════════════╝\n\n");
  
  uint_t win_s = 512;
  uint_t hop_s = 256;
  uint_t samplerate = 44100;
  
  fprintf(stderr, "Testing different tempo priors with synthetic 140 BPM signal...\n\n");
  fprintf(stderr, "Prior Mean  Prior Std   Detected BPM   Error    Status\n");
  fprintf(stderr, "────────────────────────────────────────────────────────────\n");
  
  smpl_t test_means[] = {80.0, 100.0, 120.0, 140.0, 160.0, 180.0};
  smpl_t test_stds[] = {5.0, 10.0, 20.0, 40.0};
  
  for (uint_t m = 0; m < 6; m++) {
    for (uint_t s = 0; s < 4; s++) {
      aubio_dptracker_t *dp = new_aubio_dptracker(win_s, hop_s, samplerate);
      aubio_dptracker_set_tempo(dp, test_means[m], test_stds[s]);
      
      // Feed synthetic 140 BPM beats
      uint_t interval = 74;
      for (uint_t i = 0; i < 400; i++) {
        smpl_t onset = (i % interval == 0) ? 1.0 : 0.0;
        aubio_dptracker_do(dp, onset);
      }
      
      fvec_t *beats = new_fvec(100);
      aubio_dptracker_get_beats(dp, beats);
      smpl_t bpm = aubio_dptracker_get_bpm(dp);
      
      smpl_t error = TEST_ABS(bpm - 140.0);
      const char *status = (error < 5.0) ? "✅ PASS" : "❌ FAIL";
      
      fprintf(stderr, "%7.1f     %7.1f     %8.2f       %5.2f    %s\n",
              test_means[m], test_stds[s], bpm, error, status);
      
      del_fvec(beats);
      del_aubio_dptracker(dp);
    }
  }
}

void test_extraction_timing() {
  fprintf(stderr, "\n╔══════════════════════════════════════════════════════════╗\n");
  fprintf(stderr, "║  EXTRACTION TIMING ANALYSIS                              ║\n");
  fprintf(stderr, "╚══════════════════════════════════════════════════════════╝\n\n");
  
  uint_t win_s = 512;
  uint_t hop_s = 256;
  uint_t samplerate = 44100;
  
  fprintf(stderr, "Testing effect of extraction timing on synthetic 140 BPM signal...\n\n");
  fprintf(stderr, "Extraction Type           Detected BPM   Error    Status\n");
  fprintf(stderr, "────────────────────────────────────────────────────────────\n");
  
  // Test 1: Single extraction at end
  {
    aubio_dptracker_t *dp = new_aubio_dptracker(win_s, hop_s, samplerate);
    aubio_dptracker_set_tempo(dp, 140.0, 20.0);
    
    uint_t interval = 74;
    for (uint_t i = 0; i < 400; i++) {
      smpl_t onset = (i % interval == 0) ? 1.0 : 0.0;
      aubio_dptracker_do(dp, onset);
    }
    
    fvec_t *beats = new_fvec(100);
    aubio_dptracker_get_beats(dp, beats);
    smpl_t bpm = aubio_dptracker_get_bpm(dp);
    
    smpl_t error = TEST_ABS(bpm - 140.0);
    fprintf(stderr, "Single (at end)           %8.2f       %5.2f    %s\n",
            bpm, error, (error < 5.0) ? "✅ PASS" : "❌ FAIL");
    
    del_fvec(beats);
    del_aubio_dptracker(dp);
  }
  
  // Test 2: Extract every 64 frames
  {
    aubio_dptracker_t *dp = new_aubio_dptracker(win_s, hop_s, samplerate);
    aubio_dptracker_set_tempo(dp, 140.0, 20.0);
    
    uint_t interval = 74;
    fvec_t *beats = new_fvec(100);
    smpl_t last_bpm = 0.0;
    
    for (uint_t i = 0; i < 400; i++) {
      smpl_t onset = (i % interval == 0) ? 1.0 : 0.0;
      aubio_dptracker_do(dp, onset);
      
      if (i % 64 == 0 && i > 0) {
        aubio_dptracker_get_beats(dp, beats);
        last_bpm = aubio_dptracker_get_bpm(dp);
      }
    }
    
    smpl_t error = TEST_ABS(last_bpm - 140.0);
    fprintf(stderr, "Every 64 frames           %8.2f       %5.2f    %s\n",
            last_bpm, error, (error < 5.0) ? "✅ PASS" : "❌ FAIL");
    
    del_fvec(beats);
    del_aubio_dptracker(dp);
  }
  
  // Test 3: Extract every 8 frames
  {
    aubio_dptracker_t *dp = new_aubio_dptracker(win_s, hop_s, samplerate);
    aubio_dptracker_set_tempo(dp, 140.0, 20.0);
    
    uint_t interval = 74;
    fvec_t *beats = new_fvec(100);
    smpl_t last_bpm = 0.0;
    
    for (uint_t i = 0; i < 400; i++) {
      smpl_t onset = (i % interval == 0) ? 1.0 : 0.0;
      aubio_dptracker_do(dp, onset);
      
      if (i % 8 == 0 && i > 0) {
        aubio_dptracker_get_beats(dp, beats);
        last_bpm = aubio_dptracker_get_bpm(dp);
      }
    }
    
    smpl_t error = TEST_ABS(last_bpm - 140.0);
    fprintf(stderr, "Every 8 frames            %8.2f       %5.2f    %s\n",
            last_bpm, error, (error < 5.0) ? "✅ PASS" : "❌ FAIL");
    
    del_fvec(beats);
    del_aubio_dptracker(dp);
  }
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  
  fprintf(stderr, "\n");
  fprintf(stderr, "╔══════════════════════════════════════════════════════════════╗\n");
  fprintf(stderr, "║  SESSION +4: DEEP DP INVESTIGATION                          ║\n");
  fprintf(stderr, "║  Behavioral Analysis and Parameter Testing                  ║\n");
  fprintf(stderr, "╚══════════════════════════════════════════════════════════════╝\n");
  
  // Task 1: Synthetic beats
  test_synthetic_beats();
  
  // Task 2: Real audio
  test_real_audio();
  
  // Task 3: Parameter sensitivity
  test_parameter_sensitivity();
  
  // Task 4: Extraction timing analysis
  test_extraction_timing();
  
  fprintf(stderr, "\n╔══════════════════════════════════════════════════════════════╗\n");
  fprintf(stderr, "║  SESSION +4 INVESTIGATION COMPLETE                          ║\n");
  fprintf(stderr, "╚══════════════════════════════════════════════════════════════╝\n\n");
  
  return 0;
}
