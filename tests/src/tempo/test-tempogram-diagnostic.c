/*
  Diagnostic test for Fourier tempogram implementation
  
  This test instruments the tempogram system to help debug detection issues.
*/

#include "aubio_priv.h"
#include "aubio.h"
#include "tempo/tempogram.h"
#include "utils_tests.h"
#include <stdio.h>
#include <math.h>

#define VERBOSE 1

void print_fvec_stats(const char *name, const fvec_t *v) {
  if (!v || v->length == 0) {
    fprintf(stderr, "%s: NULL or empty\n", name);
    return;
  }
  
  smpl_t min_val = v->data[0];
  smpl_t max_val = v->data[0];
  smpl_t sum = 0.0;
  
  for (uint_t i = 0; i < v->length; i++) {
    smpl_t val = v->data[i];
    if (val < min_val) min_val = val;
    if (val > max_val) max_val = val;
    sum += val;
  }
  
  smpl_t mean = sum / v->length;
  fprintf(stderr, "%s: len=%u, min=%.6f, max=%.6f, mean=%.6f\n", 
          name, v->length, min_val, max_val, mean);
}

void print_fmat_stats(const char *name, const fmat_t *m) {
  if (!m || m->height == 0 || m->length == 0) {
    fprintf(stderr, "%s: NULL or empty\n", name);
    return;
  }
  
  smpl_t min_val = m->data[0][0];
  smpl_t max_val = m->data[0][0];
  smpl_t sum = 0.0;
  uint_t total = m->height * m->length;
  
  for (uint_t i = 0; i < m->height; i++) {
    for (uint_t j = 0; j < m->length; j++) {
      smpl_t val = m->data[i][j];
      if (val < min_val) min_val = val;
      if (val > max_val) max_val = val;
      sum += val;
    }
  }
  
  smpl_t mean = sum / total;
  fprintf(stderr, "%s: %ux%u, min=%.6f, max=%.6f, mean=%.6f\n", 
          name, m->height, m->length, min_val, max_val, mean);
}

int main (void)
{
  fprintf(stderr, "\n");
  fprintf(stderr, "╔══════════════════════════════════════════════════════════════╗\n");
  fprintf(stderr, "║        TEMPOGRAM DIAGNOSTIC TEST                             ║\n");
  fprintf(stderr, "╚══════════════════════════════════════════════════════════════╝\n\n");

  uint_t samplerate = 44100;
  uint_t win_s = 512;
  uint_t hop_s = 256;
  
  fprintf(stderr, "=== Test 1: Basic Tempogram Creation ===\n");
  aubio_tempogram_t *o = new_aubio_tempogram(win_s, hop_s, samplerate);
  assert(o != NULL);
  fprintf(stderr, "✓ Tempogram created successfully\n");
  
  fprintf(stderr, "\n=== Test 2: Set Tempo Range ===\n");
  uint_t result = aubio_tempogram_set_tempo_min(o, 60.0);
  assert(result == AUBIO_OK);
  fprintf(stderr, "✓ Set tempo_min = 60 BPM\n");
  
  result = aubio_tempogram_set_tempo_max(o, 180.0);
  assert(result == AUBIO_OK);
  fprintf(stderr, "✓ Set tempo_max = 180 BPM\n");
  
  fprintf(stderr, "\n=== Test 3: Create Tempogram Output Matrix ===\n");
  uint_t fft_bins = win_s / 2 + 1;
  fmat_t *tempogram = new_fmat(fft_bins, 1);
  assert(tempogram != NULL);
  fprintf(stderr, "✓ Created tempogram output: %u bins x 1 frame\n", fft_bins);
  
  fprintf(stderr, "\n=== Test 4: Simulated Onset Sequence (120 BPM) ===\n");
  // 120 BPM = 2 beats per second
  // With hop_size = 256 samples at 44100 Hz:
  // hop_time = 256 / 44100 = 0.0058 seconds = 5.8 ms
  // Beat period = 0.5 seconds = 500 ms
  // Hops per beat = 500 / 5.8 = ~86 hops
  
  smpl_t beat_period_s = 60.0 / 120.0;  // 0.5 seconds
  smpl_t hop_time_s = (smpl_t)hop_s / (smpl_t)samplerate;
  uint_t hops_per_beat = (uint_t)(beat_period_s / hop_time_s + 0.5);
  
  fprintf(stderr, "Beat period: %.3f s\n", beat_period_s);
  fprintf(stderr, "Hop time: %.6f s\n", hop_time_s);
  fprintf(stderr, "Hops per beat: %u\n", hops_per_beat);
  
  // Create onset detection function with regular beats
  fvec_t *onset = new_fvec(1);
  uint_t num_frames = 1000;  // About 5.8 seconds
  uint_t beat_count = 0;
  
  fprintf(stderr, "Processing %u frames...\n", num_frames);
  
  for (uint_t frame = 0; frame < num_frames; frame++) {
    // Strong onset every hops_per_beat frames
    if (frame % hops_per_beat == 0) {
      onset->data[0] = 1.0;
      beat_count++;
      if (VERBOSE && beat_count <= 10) {
        fprintf(stderr, "  Frame %4u: Beat #%u (onset=1.0)\n", frame, beat_count);
      }
    } else {
      onset->data[0] = 0.0;
    }
    
    // Process with tempogram
    aubio_tempogram_do(o, onset, tempogram);
    
    // Check output periodically
    if (frame % 100 == 99) {
      smpl_t tempo = aubio_tempogram_get_tempo(o, tempogram);
      smpl_t confidence = aubio_tempogram_get_confidence(o);
      
      if (VERBOSE) {
        fprintf(stderr, "  Frame %4u: tempo=%.2f BPM, confidence=%.3f\n", 
                frame, tempo, confidence);
      }
      
      if (frame == 499) {  // After ~2.9 seconds
        print_fmat_stats("tempogram", tempogram);
      }
    }
  }
  
  fprintf(stderr, "\nProcessed %u beats over %u frames\n", beat_count, num_frames);
  
  fprintf(stderr, "\n=== Test 5: Final Detection Results ===\n");
  smpl_t final_tempo = aubio_tempogram_get_tempo(o, tempogram);
  smpl_t final_confidence = aubio_tempogram_get_confidence(o);
  
  fprintf(stderr, "Expected tempo: 120.0 BPM\n");
  fprintf(stderr, "Detected tempo: %.2f BPM\n", final_tempo);
  fprintf(stderr, "Detection error: %.2f BPM\n", fabs(final_tempo - 120.0));
  fprintf(stderr, "Confidence: %.3f\n", final_confidence);
  
  if (fabs(final_tempo - 120.0) < 5.0 && final_confidence > 1.5) {
    fprintf(stderr, "✓ PASS: Tempo detected accurately\n");
  } else {
    fprintf(stderr, "✗ FAIL: Tempo detection issues\n");
    fprintf(stderr, "  Expected: 120 BPM (±5)\n");
    fprintf(stderr, "  Got: %.2f BPM, confidence=%.3f\n", final_tempo, final_confidence);
  }
  
  fprintf(stderr, "\n=== Test 6: Inspect Tempogram Spectrum ===\n");
  fprintf(stderr, "Checking energy distribution across tempo bins:\n");
  
  // Find BPM for key bins
  for (uint_t bin = 0; bin < MIN(20, fft_bins); bin++) {
    smpl_t bpm = (bin * 60.0 * samplerate) / (win_s * hop_s);
    smpl_t energy = tempogram->data[bin][0];
    fprintf(stderr, "  Bin %3u: %6.1f BPM, energy=%.6f\n", bin, bpm, energy);
  }
  
  fprintf(stderr, "...\n");
  
  // Check around 120 BPM
  smpl_t target_bpm = 120.0;
  uint_t target_bin = (uint_t)((target_bpm * win_s * hop_s) / (60.0 * samplerate) + 0.5);
  fprintf(stderr, "Target bin for 120 BPM: %u\n", target_bin);
  
  for (int offset = -3; offset <= 3; offset++) {
    uint_t bin = target_bin + offset;
    if (bin < fft_bins) {
      smpl_t bpm = (bin * 60.0 * samplerate) / (win_s * hop_s);
      smpl_t energy = tempogram->data[bin][0];
      fprintf(stderr, "  Bin %3u: %6.1f BPM, energy=%.6f %s\n", 
              bin, bpm, energy, (offset == 0) ? "<-- target" : "");
    }
  }
  
  fprintf(stderr, "\n=== Test 7: Vary Tempo and Test Detection ===\n");
  smpl_t test_tempos[] = {80.0, 100.0, 120.0, 140.0, 160.0};
  uint_t num_test_tempos = sizeof(test_tempos) / sizeof(test_tempos[0]);
  
  for (uint_t t = 0; t < num_test_tempos; t++) {
    smpl_t test_bpm = test_tempos[t];
    smpl_t test_period_s = 60.0 / test_bpm;
    uint_t test_hops_per_beat = (uint_t)(test_period_s / hop_time_s + 0.5);
    
    fprintf(stderr, "\nTesting %.0f BPM (period=%.3fs, hops/beat=%u):\n", 
            test_bpm, test_period_s, test_hops_per_beat);
    
    // Reset tempogram
    del_aubio_tempogram(o);
    o = new_aubio_tempogram(win_s, hop_s, samplerate);
    aubio_tempogram_set_tempo_min(o, 60.0);
    aubio_tempogram_set_tempo_max(o, 180.0);
    
    // Process beats
    for (uint_t frame = 0; frame < 500; frame++) {
      onset->data[0] = (frame % test_hops_per_beat == 0) ? 1.0 : 0.0;
      aubio_tempogram_do(o, onset, tempogram);
    }
    
    smpl_t detected = aubio_tempogram_get_tempo(o, tempogram);
    smpl_t conf = aubio_tempogram_get_confidence(o);
    smpl_t error = fabs(detected - test_bpm);
    
    fprintf(stderr, "  Expected: %.1f BPM\n", test_bpm);
    fprintf(stderr, "  Detected: %.1f BPM\n", detected);
    fprintf(stderr, "  Error: %.1f BPM\n", error);
    fprintf(stderr, "  Confidence: %.3f\n", conf);
    fprintf(stderr, "  Status: %s\n", (error < 5.0) ? "✓ PASS" : "✗ FAIL");
  }
  
  // Cleanup
  del_fvec(onset);
  del_fmat(tempogram);
  del_aubio_tempogram(o);
  
  fprintf(stderr, "\n");
  fprintf(stderr, "╔══════════════════════════════════════════════════════════════╗\n");
  fprintf(stderr, "║        DIAGNOSTIC TEST COMPLETE                              ║\n");
  fprintf(stderr, "╚══════════════════════════════════════════════════════════════╝\n");
  
  return 0;
}
