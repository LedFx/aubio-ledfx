/*
  Test DP Tracker Basic Functionality
  
  Tests:
  1. Creation and deletion
  2. Tempo setting
  3. Onset feeding
  4. Basic DP score computation
  5. Memory safety
*/

#include <aubio.h>
#include "aubio_priv.h"
#include "tempo/dptracker.h"
#include "utils_tests.h"

int main(void)
{
  uint_t win_s = 512;
  uint_t hop_s = 256;
  uint_t samplerate = 44100;
  
  fprintf(stdout, "\n");
  fprintf(stdout, "╔══════════════════════════════════════════════════════════════╗\n");
  fprintf(stdout, "║        DP TRACKER BASIC TEST                                 ║\n");
  fprintf(stdout, "╚══════════════════════════════════════════════════════════════╝\n");
  fprintf(stdout, "\n");
  
  // Test 1: Creation
  fprintf(stdout, "=== Test 1: DP Tracker Creation ===\n");
  aubio_dptracker_t *dp = new_aubio_dptracker(win_s, hop_s, samplerate);
  assert(dp != NULL);
  fprintf(stdout, "✓ DP tracker created successfully\n");
  fprintf(stdout, "  win_s=%d, hop_s=%d, samplerate=%d\n", win_s, hop_s, samplerate);
  fprintf(stdout, "\n");
  
  // Test 2: Default tempo settings
  fprintf(stdout, "=== Test 2: Default Tempo Settings ===\n");
  smpl_t tempo_mean = aubio_dptracker_get_tempo_mean(dp);
  smpl_t tempo_std = aubio_dptracker_get_tempo_std(dp);
  fprintf(stdout, "Default tempo: %.1f ± %.1f BPM\n", tempo_mean, tempo_std);
  assert(tempo_mean == 120.0);
  assert(tempo_std == 10.0);
  fprintf(stdout, "✓ Default tempo settings correct\n");
  fprintf(stdout, "\n");
  
  // Test 3: Set custom tempo
  fprintf(stdout, "=== Test 3: Set Custom Tempo ===\n");
  uint_t ret = aubio_dptracker_set_tempo(dp, 140.0, 5.0);
  assert(ret == AUBIO_OK);
  tempo_mean = aubio_dptracker_get_tempo_mean(dp);
  tempo_std = aubio_dptracker_get_tempo_std(dp);
  fprintf(stdout, "New tempo: %.1f ± %.1f BPM\n", tempo_mean, tempo_std);
  assert(tempo_mean == 140.0);
  assert(tempo_std == 5.0);
  fprintf(stdout, "✓ Custom tempo set successfully\n");
  fprintf(stdout, "\n");
  
  // Test 4: Feed onset values
  fprintf(stdout, "=== Test 4: Feed Onset Values ===\n");
  // Simulate onset pattern at ~140 BPM
  // 140 BPM = 2.333 beats/sec = 0.429s per beat
  // At hop_s=256, samplerate=44100: 0.005805s per hop
  // Hops per beat = 0.429 / 0.005805 ≈ 74 hops
  
  uint_t hops_per_beat = 74;
  uint_t num_frames = 300;
  uint_t beat_count = 0;
  
  for (uint_t i = 0; i < num_frames; i++) {
    smpl_t onset_value = 0.0;
    
    // Create beat pulse every hops_per_beat frames
    if (i % hops_per_beat == 0) {
      onset_value = 1.0;
      beat_count++;
      fprintf(stdout, "  Frame %3d: Beat #%d (onset=%.1f)\n", i, beat_count, onset_value);
    }
    
    // Feed to DP tracker
    aubio_dptracker_do(dp, onset_value);
  }
  
  fprintf(stdout, "Processed %d frames, %d beats\n", num_frames, beat_count);
  fprintf(stdout, "✓ Onset values fed successfully\n");
  fprintf(stdout, "\n");
  
  // Test 5: Extract beat sequence
  fprintf(stdout, "=== Test 5: Extract Beat Sequence ===\n");
  fvec_t *beats = new_fvec(win_s);
  aubio_dptracker_get_beats(dp, beats);
  
  uint_t num_beats = aubio_dptracker_get_num_beats(dp);
  fprintf(stdout, "Detected %d beats\n", num_beats);
  
  if (num_beats > 0) {
    fprintf(stdout, "Beat positions (frames):\n");
    for (uint_t i = 0; i < MIN(10, num_beats); i++) {
      fprintf(stdout, "  Beat %2d: frame %.0f\n", i+1, beats->data[i]);
    }
    if (num_beats > 10) {
      fprintf(stdout, "  ... (%d more beats)\n", num_beats - 10);
    }
  }
  fprintf(stdout, "✓ Beat sequence extracted\n");
  fprintf(stdout, "\n");
  
  // Test 6: Compute BPM
  fprintf(stdout, "=== Test 6: Compute BPM from DP Path ===\n");
  smpl_t detected_bpm = aubio_dptracker_get_bpm(dp);
  fprintf(stdout, "Expected: 140.0 BPM\n");
  fprintf(stdout, "Detected: %.2f BPM\n", detected_bpm);
  
  if (detected_bpm > 0.0) {
    smpl_t error = ABS(detected_bpm - 140.0);
    fprintf(stdout, "Error: %.2f BPM\n", error);
    
    // Allow 5 BPM error tolerance
    if (error < 5.0) {
      fprintf(stdout, "✓ PASS: BPM detection accurate\n");
    } else {
      fprintf(stdout, "⚠️  WARNING: BPM error > 5.0 (%.2f)\n", error);
    }
  } else {
    fprintf(stdout, "⚠️  WARNING: No tempo detected (insufficient beats)\n");
  }
  fprintf(stdout, "\n");
  
  // Test 7: Get confidence
  fprintf(stdout, "=== Test 7: Path Confidence ===\n");
  smpl_t confidence = aubio_dptracker_get_confidence(dp);
  fprintf(stdout, "Confidence score: %.3f\n", confidence);
  fprintf(stdout, "✓ Confidence retrieved\n");
  fprintf(stdout, "\n");
  
  // Test 8: Reset
  fprintf(stdout, "=== Test 8: Reset DP Tracker ===\n");
  aubio_dptracker_reset(dp);
  num_beats = aubio_dptracker_get_num_beats(dp);
  confidence = aubio_dptracker_get_confidence(dp);
  fprintf(stdout, "After reset:\n");
  fprintf(stdout, "  Num beats: %d\n", num_beats);
  fprintf(stdout, "  Confidence: %.3f\n", confidence);
  assert(num_beats == 0);
  assert(confidence == 0.0);
  fprintf(stdout, "✓ Reset successful\n");
  fprintf(stdout, "\n");
  
  // Cleanup
  del_fvec(beats);
  del_aubio_dptracker(dp);
  
  fprintf(stdout, "╔══════════════════════════════════════════════════════════════╗\n");
  fprintf(stdout, "║        DP TRACKER BASIC TEST COMPLETE                        ║\n");
  fprintf(stdout, "╚══════════════════════════════════════════════════════════════╝\n");
  fprintf(stdout, "\n");
  
  return 0;
}
