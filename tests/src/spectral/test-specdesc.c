#include <aubio.h>
#include <stdio.h>

int main (void)
{
  uint_t win_s = 1024; // window size
  cvec_t *in = new_cvec (win_s); // input buffer
  fvec_t *out = new_fvec (1); // output spectral descriptor

  aubio_specdesc_t *o;

  o = new_aubio_specdesc ("energy", win_s);
  aubio_specdesc_do (o, in, out);
  del_aubio_specdesc (o);

  o = new_aubio_specdesc ("energy", win_s);
  aubio_specdesc_do (o, in, out);
  del_aubio_specdesc (o);

  o = new_aubio_specdesc ("hfc", win_s);
  aubio_specdesc_do (o, in, out);
  del_aubio_specdesc (o);

  o = new_aubio_specdesc ("complex", win_s);
  aubio_specdesc_do (o, in, out);
  del_aubio_specdesc (o);

  o = new_aubio_specdesc ("phase", win_s);
  aubio_specdesc_do (o, in, out);
  del_aubio_specdesc (o);

  o = new_aubio_specdesc ("kl", win_s);
  aubio_specdesc_do (o, in, out);
  del_aubio_specdesc (o);

  o = new_aubio_specdesc ("mkl", win_s);
  aubio_specdesc_do (o, in, out);
  del_aubio_specdesc (o);

  // rolloff is the number of bins below which 95% of the energy lies, so it
  // ranges over [1, length] and never reads past the last bin.
  o = new_aubio_specdesc ("rolloff", win_s);
  cvec_zeros(in);
  in->norm[0] = 1.0; // all energy in the first bin
  aubio_specdesc_do (o, in, out);
  if (out->data[0] != 1) {
    fprintf(stderr, "rolloff, energy in first bin: %f != 1\n", out->data[0]);
    return 1;
  }
  cvec_zeros(in);
  in->norm[in->length - 1] = 1.0; // all energy in the last bin
  aubio_specdesc_do (o, in, out);
  if (out->data[0] != in->length) {
    fprintf(stderr, "rolloff, energy in last bin: %f != %d\n", out->data[0], in->length);
    return 1;
  }
  del_aubio_specdesc (o);

  del_cvec (in);
  del_fvec (out);
  aubio_cleanup ();

  return 0;
}
