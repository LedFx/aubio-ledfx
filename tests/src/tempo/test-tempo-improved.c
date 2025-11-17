/* Test improved tempo tracking features */
#include <aubio.h>
#include "utils_tests.h"

int main (int argc, char **argv)
{
  uint_t win_size = 1024;
  uint_t hop_size = 256;
  uint_t samplerate = 44100;
  aubio_tempo_t *t;
  fvec_t* in, *out;
  uint_t i;

  PRINT_MSG("Testing improved tempo tracking features\n");

  t = new_aubio_tempo("default", win_size, hop_size, samplerate);
  if (!t) {
    PRINT_ERR("failed to create tempo object\n");
    return 1;
  }

  in = new_fvec(hop_size);
  out = new_fvec(1);

  PRINT_MSG("Testing tempo prior mean setter (140 BPM)\n");
  if (aubio_tempo_set_tempo_prior_mean(t, 140.0) != 0) {
    PRINT_ERR("failed to set tempo prior mean\n");
    return 1;
  }

  PRINT_MSG("Testing tempo prior std setter (2.0)\n");
  if (aubio_tempo_set_tempo_prior_std(t, 2.0) != 0) {
    PRINT_ERR("failed to set tempo prior std\n");
    return 1;
  }

  PRINT_MSG("Testing invalid tempo prior mean (negative)\n");
  if (aubio_tempo_set_tempo_prior_mean(t, -1.0) == 0) {
    PRINT_ERR("should have rejected negative tempo prior mean\n");
    return 1;
  }

  PRINT_MSG("Testing invalid tempo prior std (negative)\n");
  if (aubio_tempo_set_tempo_prior_std(t, -1.0) == 0) {
    PRINT_ERR("should have rejected negative tempo prior std\n");
    return 1;
  }

  /* Run a few frames to test functionality */
  for (i = 0; i < 10; i++) {
    aubio_tempo_do(t, in, out);
  }

  PRINT_MSG("All improved tempo tracking tests passed\n");

  del_aubio_tempo(t);
  del_fvec(in);
  del_fvec(out);

  return 0;
}
