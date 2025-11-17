/*
  Simple tempogram test to debug FFT issues
*/

#include "aubio_priv.h"
#include "aubio.h"
#include "tempo/tempogram.h"
#include "utils_tests.h"
#include <stdio.h>

int main (void)
{
  fprintf(stderr, "\n=== SIMPLE TEMPOGRAM DEBUG TEST ===\n\n");

  uint_t samplerate = 44100;
  uint_t win_s = 512;
  uint_t hop_s = 256;
  
  // Create tempogram
  aubio_tempogram_t *tg = new_aubio_tempogram(win_s, hop_s, samplerate);
  assert(tg != NULL);
  aubio_tempogram_set_tempo_min(tg, 60.0);
  aubio_tempogram_set_tempo_max(tg, 180.0);
  
  uint_t fft_bins = win_s / 2 + 1;
  fmat_t *tempogram_out = new_fmat(fft_bins, 1);
  
  // Create onset with a strong pulse
  fvec_t *onset = new_fvec(1);
  
  fprintf(stderr, "Feeding strong onset pulses...\n");
  
  // Feed 100 frames with alternating strong/weak onsets (simulating ~120 BPM)
  for (uint_t frame = 0; frame < 100; frame++) {
    if (frame % 10 == 0) {
      onset->data[0] = 1.0;  // Strong onset
      fprintf(stderr, "Frame %3u: onset=1.0\n", frame);
    } else {
      onset->data[0] = 0.0;
    }
    
    aubio_tempogram_do(tg, onset, tempogram_out);
    
    if (frame == 50) {
      // Check tempogram output
      fprintf(stderr, "\nAt frame 50, checking tempogram output:\n");
      smpl_t max_val = 0.0;
      uint_t max_bin = 0;
      uint_t non_zero = 0;
      
      for (uint_t i = 0; i < fft_bins; i++) {
        smpl_t val = tempogram_out->data[i][0];
        if (val != 0.0) non_zero++;
        if (val > max_val) {
          max_val = val;
          max_bin = i;
        }
      }
      
      fprintf(stderr, "  Total bins: %u\n", fft_bins);
      fprintf(stderr, "  Non-zero bins: %u\n", non_zero);
      fprintf(stderr, "  Max value: %.6f at bin %u\n", max_val, max_bin);
      
      if (max_val > 0.0) {
        // Calculate BPM for max bin
        smpl_t bpm = (max_bin * 60.0 * samplerate) / (win_s * hop_s);
        fprintf(stderr, "  Max bin corresponds to %.1f BPM\n", bpm);
      }
      
      // Show first 20 bins
      fprintf(stderr, "\n  First 20 bins:\n");
      for (uint_t i = 0; i < MIN(20, fft_bins); i++) {
        smpl_t bpm = (i * 60.0 * samplerate) / (win_s * hop_s);
        fprintf(stderr, "    Bin %3u (%6.1f BPM): %.6f\n", 
                i, bpm, tempogram_out->data[i][0]);
      }
    }
  }
  
  fprintf(stderr, "\nFinal detection:\n");
  smpl_t tempo = aubio_tempogram_get_tempo(tg, tempogram_out);
  smpl_t confidence = aubio_tempogram_get_confidence(tg);
  fprintf(stderr, "  Tempo: %.2f BPM\n", tempo);
  fprintf(stderr, "  Confidence: %.3f\n", confidence);
  
  if (confidence > 0.5) {
    fprintf(stderr, "\n✓ Test PASSED - tempogram detected tempo\n");
  } else {
    fprintf(stderr, "\n✗ Test FAILED - no tempo detected (confidence too low)\n");
  }
  
  del_fvec(onset);
  del_fmat(tempogram_out);
  del_aubio_tempogram(tg);
  
  return 0;
}
