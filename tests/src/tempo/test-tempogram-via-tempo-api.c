/*
  Test tempogram integration via tempo API
*/

#include "aubio_priv.h"
#include "aubio.h"
#include "utils_tests.h"
#include <stdio.h>

int main (void)
{
  fprintf(stderr, "\n=== TEMPOGRAM VIA TEMPO API TEST ===\n\n");

  uint_t samplerate = 44100;
  uint_t win_s = 1024;
  uint_t hop_s = 256;
  
  // Create tempo object
  aubio_tempo_t *tempo = new_aubio_tempo("default", win_s, hop_s, samplerate);
  assert(tempo != NULL);
  
  // Enable tempogram mode
  uint_t result = aubio_tempo_set_use_tempogram(tempo, 1);
  if (result != AUBIO_OK) {
    fprintf(stderr, "✗ Failed to enable tempogram\n");
    del_aubio_tempo(tempo);
    return 1;
  }
  fprintf(stderr, "✓ Tempogram mode enabled\n");
  
  // Create input/output buffers
  fvec_t *samples = new_fvec(hop_s);
  fvec_t *tempo_out = new_fvec(2);  // beat position output
  
  // Simulate 120 BPM = 2 beats per second
  // hop_time = 256/44100 = 5.8ms
  // beat period = 500ms = 86 hops
  uint_t hops_per_beat = 86;
  
  fprintf(stderr, "Simulating 120 BPM audio (beat every %u hops)...\n", hops_per_beat);
  
  // Generate sine wave at frequency corresponding to beat period
  smpl_t beat_freq = 120.0 / 60.0;  // 2 Hz
  uint_t total_hops = 1000;  // ~5.8 seconds
  
  for (uint_t hop = 0; hop < total_hops; hop++) {
    // Generate audio samples with amplitude modulation at beat frequency
    for (uint_t i = 0; i < hop_s; i++) {
      smpl_t t = (hop * hop_s + i) / (smpl_t)samplerate;
      // Modulate a tone at beat frequency
      smpl_t envelope = 0.5 + 0.5 * SIN(2.0 * M_PI * beat_freq * t);
      smpl_t tone = SIN(2.0 * M_PI * 440.0 * t);  // 440 Hz tone
      samples->data[i] = envelope * tone;
    }
    
    // Process with tempo
    aubio_tempo_do(tempo, samples, tempo_out);
    
    // Check BPM periodically
    if (hop % 100 == 99) {
      smpl_t bpm = aubio_tempo_get_bpm(tempo);
      smpl_t confidence = aubio_tempo_get_confidence(tempo);
      fprintf(stderr, "  Hop %4u: BPM=%.2f, confidence=%.3f\n", hop, bpm, confidence);
    }
  }
  
  // Final check
  smpl_t final_bpm = aubio_tempo_get_bpm(tempo);
  smpl_t final_conf = aubio_tempo_get_confidence(tempo);
  
  fprintf(stderr, "\nFinal results:\n");
  fprintf(stderr, "  Expected BPM: 120.0\n");
  fprintf(stderr, "  Detected BPM: %.2f\n", final_bpm);
  fprintf(stderr, "  Error: %.2f BPM\n", fabs(final_bpm - 120.0));
  fprintf(stderr, "  Confidence: %.3f\n", final_conf);
  
  uint_t passed = (fabs(final_bpm - 120.0) < 10.0);
  
  if (passed) {
    fprintf(stderr, "\n✓ Test PASSED\n");
  } else {
    fprintf(stderr, "\n✗ Test FAILED\n");
  }
  
  del_fvec(samples);
  del_fvec(tempo_out);
  del_aubio_tempo(tempo);
  
  return passed ? 0 : 1;
}
