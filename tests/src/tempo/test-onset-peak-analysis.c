/*
  Session +3: Onset Peak Analysis Tool
  
  Purpose: Analyze onset stream to understand peak detection behavior
*/

#include <stdio.h>
#include <stdlib.h>
#include "aubio.h"

#define HOP_S 256
#define WIN_S 1024
#define SAMPLERATE 44100

#ifdef AUBIO_TEMPO_TEST_DIR
#define TEMPO_TEST_FILE(filename) AUBIO_TEMPO_TEST_DIR "/" filename
#else
#define TEMPO_TEST_FILE(filename) "tests/" filename
#endif

int main(void) {
  const char *test_file = TEMPO_TEST_FILE("test_bpm_changes.wav");
  
  aubio_source_t *source = new_aubio_source(test_file, SAMPLERATE, HOP_S);
  if (!source) {
    fprintf(stderr, "Failed to open %s\n", test_file);
    return 1;
  }
  
  aubio_tempo_t *tempo = new_aubio_tempo("default", WIN_S, HOP_S, SAMPLERATE);
  
  fvec_t *input = new_fvec(HOP_S);
  fvec_t *tempo_out = new_fvec(1);
  
  uint_t frame = 0;
  uint_t read = HOP_S;
  uint_t peak_count = 0;
  
  fprintf(stderr, "Analyzing onset peaks...\n");
  fprintf(stderr, "Frame,Time(s),Onset,IsPeak\n");
  
  smpl_t last_onset = 0.0;
  smpl_t threshold = 0.3;
  smpl_t alpha = 0.01;
  uint_t cooldown = 0;
  
  while (read == HOP_S && frame < 2000) {
    aubio_source_do(source, input, &read);
    aubio_tempo_do(tempo, input, tempo_out);
    
    smpl_t onset = tempo_out->data[0];
    smpl_t time_s = (frame * HOP_S) / (smpl_t)SAMPLERATE;
    
    // Simulate peak detection
    threshold = (1.0 - alpha) * threshold + alpha * onset;
    if (threshold < 0.2) threshold = 0.2;
    
    int is_peak = 0;
    if (cooldown > 0) {
      cooldown--;
    } else if (onset > threshold * 1.5 && onset > last_onset) {
      is_peak = 1;
      peak_count++;
      cooldown = 20;
    }
    
    if (frame % 10 == 0 || is_peak) {
      fprintf(stderr, "%u,%.3f,%.4f,%d\n", frame, time_s, onset, is_peak);
    }
    
    last_onset = onset;
    frame++;
  }
  
  fprintf(stderr, "\nTotal peaks detected: %u in %u frames\n", peak_count, frame);
  fprintf(stderr, "Peak frequency: %.2f peaks/sec\n", peak_count / ((frame * HOP_S) / (smpl_t)SAMPLERATE));
  fprintf(stderr, "Implied BPM if all peaks are beats: %.1f BPM\n", 
          60.0 * peak_count / ((frame * HOP_S) / (smpl_t)SAMPLERATE));
  
  del_fvec(input);
  del_fvec(tempo_out);
  del_aubio_tempo(tempo);
  del_aubio_source(source);
  
  return 0;
}
