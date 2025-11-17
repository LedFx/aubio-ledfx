/*
  Test tempogram basic functionality
*/

#include <aubio.h>
#include "tempo/tempogram.h"
#include "utils_tests.h"

int main (void)
{
  uint_t win_s = 512;     // Tempogram window size (must be power of 2 for FFT)
  uint_t hop_s = 256;     // Hop size
  uint_t samplerate = 44100;
  
  // Create tempogram
  aubio_tempogram_t *o = new_aubio_tempogram (win_s, hop_s, samplerate);
  assert (o != NULL);
  
  // Test basic properties
  assert (aubio_tempogram_get_confidence (o) >= 0.0);
  
  // Set tempo range
  assert (aubio_tempogram_set_tempo_min (o, 60.0) == 0);
  assert (aubio_tempogram_set_tempo_max (o, 180.0) == 0);
  
  // Test invalid ranges
  assert (aubio_tempogram_set_tempo_min (o, 10.0) == 1);  // Too low
  assert (aubio_tempogram_set_tempo_max (o, 500.0) == 1); // Too high
  assert (aubio_tempogram_set_tempo_min (o, 200.0) == 1); // > max
  
  // Create test input (single onset value)
  fvec_t *onset = new_fvec (1);
  assert (onset != NULL);
  
  // Create tempogram output matrix
  uint_t fft_bins = win_s / 2 + 1;  // Number of tempo bins
  fmat_t *tempogram = new_fmat (fft_bins, 1);
  assert (tempogram != NULL);
  
  // Process some onset values
  uint_t i;
  for (i = 0; i < 64; i++) {
    // Simulate onset strength envelope
    onset->data[0] = (i % 8 == 0) ? 1.0 : 0.1;  // Beat pattern
    
    aubio_tempogram_do (o, onset, tempogram);
    
    // Should not crash
    smpl_t tempo = aubio_tempogram_get_tempo (o, tempogram);
    assert (tempo > 0.0);
    assert (tempo >= 30.0 && tempo <= 300.0);
    
    smpl_t confidence = aubio_tempogram_get_confidence (o);
    assert (confidence >= 0.0);
  }
  
  // Test PLP at specific time
  fmat_t *multi_frame = new_fmat (fft_bins, 8);
  assert (multi_frame != NULL);
  
  // Fill with some data
  for (i = 0; i < 8; i++) {
    onset->data[0] = (i % 2 == 0) ? 1.0 : 0.2;
    aubio_tempogram_do (o, onset, tempogram);
    
    // Copy to multi-frame matrix
    uint_t j;
    for (j = 0; j < fft_bins; j++) {
      multi_frame->data[j][i] = tempogram->data[j][0];
    }
  }
  
  // Get PLP at each time
  for (i = 0; i < 8; i++) {
    smpl_t plp_tempo = aubio_tempogram_get_plp_at_time (o, multi_frame, i);
    assert (plp_tempo > 0.0);
  }
  
  // Test PLP curve extraction
  fvec_t *plp_curve = new_fvec (8);
  assert (plp_curve != NULL);
  
  aubio_tempogram_get_plp_curve (o, multi_frame, plp_curve);
  
  // Verify all values are valid
  for (i = 0; i < 8; i++) {
    assert (plp_curve->data[i] > 0.0);
    assert (plp_curve->data[i] >= 30.0 && plp_curve->data[i] <= 300.0);
  }
  
  // Cleanup
  del_fvec (plp_curve);
  del_fmat (multi_frame);
  del_fmat (tempogram);
  del_fvec (onset);
  del_aubio_tempogram (o);
  
  return 0;
}
