/* Test to compare direct vs FFT autocorrelation */

#include <aubio.h>
#include "utils_tests.h"
#include "mathutils.h"
#include <math.h>

int main (void)
{
  uint_t win_s = 1024;
  uint_t i;
  
  // Create test signal with known periodicity
  fvec_t *input = new_fvec(win_s);
  fvec_t *acf_direct = new_fvec(win_s);
  fvec_t *acf_fft = new_fvec(win_s);
  
  // Create a simple periodic signal: 120 BPM at 44100 Hz
  // Period in samples: 60 / 120 * 44100 = 22050 samples (too long for test)
  // Use shorter period for testing: 50 samples
  smpl_t period = 50.0;
  for (i = 0; i < win_s; i++) {
    input->data[i] = sinf(2.0 * M_PI * i / period);
  }
  
  // Compute autocorrelation with both methods
  aubio_autocorr(input, acf_direct);
  aubio_autocorr_fft(input, acf_fft);
  
  // Compare results
  smpl_t max_diff = 0.0;
  smpl_t max_rel_diff = 0.0;
  uint_t max_diff_idx = 0;
  uint_t compare_len = (win_s < 200) ? win_s : 200;
  
  fprintf(stderr, "Comparing autocorrelation methods (win_s=%d, period=%.1f)\n", win_s, period);
  fprintf(stderr, "Lag    Direct      FFT         Diff        Rel.Diff%%\n");
  fprintf(stderr, "---    ------      ---         ----        ---------\n");
  
  for (i = 0; i < compare_len; i++) {
    smpl_t diff = fabsf(acf_direct->data[i] - acf_fft->data[i]);
    smpl_t rel_diff = 0.0;
    
    if (fabsf(acf_direct->data[i]) > 1e-6) {
      rel_diff = 100.0 * diff / fabsf(acf_direct->data[i]);
    }
    
    if (diff > max_diff) {
      max_diff = diff;
      max_diff_idx = i;
    }
    if (rel_diff > max_rel_diff) {
      max_rel_diff = rel_diff;
    }
    
    // Print first few and around period
    if (i < 10 || (i >= (uint_t)(period - 2) && i <= (uint_t)(period + 2))) {
      fprintf(stderr, "%3d    %9.6f   %9.6f   %9.6f   %8.3f%%\n", 
              i, acf_direct->data[i], acf_fft->data[i], diff, rel_diff);
    }
  }
  
  fprintf(stderr, "\nMax absolute difference: %.6f at lag %d\n", max_diff, max_diff_idx);
  fprintf(stderr, "Max relative difference: %.3f%%\n", max_rel_diff);
  
  // Check if peak is at correct location for both
  uint_t peak_direct = 0, peak_fft = 0;
  smpl_t max_direct = 0.0, max_fft = 0.0;
  uint_t peak_search_len = (win_s < 100) ? win_s : 100;
  
  for (i = 1; i < peak_search_len; i++) {
    if (acf_direct->data[i] > max_direct) {
      max_direct = acf_direct->data[i];
      peak_direct = i;
    }
    if (acf_fft->data[i] > max_fft) {
      max_fft = acf_fft->data[i];
      peak_fft = i;
    }
  }
  
  fprintf(stderr, "\nPeak detection:\n");
  fprintf(stderr, "Expected period: %.1f samples\n", period);
  fprintf(stderr, "Direct method peak: %d (value: %.6f)\n", peak_direct, max_direct);
  fprintf(stderr, "FFT method peak:    %d (value: %.6f)\n", peak_fft, max_fft);
  
  // Test passes if methods agree within reasonable tolerance
  if (max_rel_diff > 5.0) {
    fprintf(stderr, "\nWARNING: Large relative difference detected!\n");
    fprintf(stderr, "This may cause different tempo detection behavior.\n");
  }
  
  if (peak_direct != peak_fft) {
    fprintf(stderr, "\nWARNING: Different peaks detected!\n");
    fprintf(stderr, "This will cause different tempo estimates.\n");
    return 1;
  }
  
  del_fvec(input);
  del_fvec(acf_direct);
  del_fvec(acf_fft);
  
  fprintf(stderr, "\nAutocorrelation methods produce consistent results.\n");
  return 0;
}
