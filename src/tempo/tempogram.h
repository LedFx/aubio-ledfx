/*
  Copyright (C) 2024 The aubio-ledfx team

  This file is part of aubio.

  aubio is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  aubio is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with aubio.  If not, see <http://www.gnu.org/licenses/>.

*/

/** \file

  Fourier tempogram

  Computes a time-frequency representation of tempo using Short-Time Fourier
  Transform (STFT) on the onset strength envelope. This provides multi-resolution
  tempo analysis and enables time-varying tempo tracking.

  The tempogram represents tempo energy across time using frequency bins that
  correspond to different tempo (BPM) values.

  Reference: Grosche, P., & Müller, M. (2011). "Extracting predominant local
  pulse information from music recordings."

  \example tempo/test-tempogram-basic.c

*/

#ifndef AUBIO_TEMPOGRAM_H
#define AUBIO_TEMPOGRAM_H

#ifdef __cplusplus
extern "C" {
#endif

/** tempogram object */
typedef struct _aubio_tempogram_t aubio_tempogram_t;

/** create tempogram object

  \param win_s window size for tempogram analysis (e.g., 384 samples)
  \param hop_s hop size (samples between consecutive onset measurements)
  \param samplerate sampling rate of the input signal

*/
aubio_tempogram_t *new_aubio_tempogram (uint_t win_s, uint_t hop_s,
    uint_t samplerate);

/** delete tempogram object

  \param o tempogram object to delete

*/
void del_aubio_tempogram (aubio_tempogram_t * o);

/** execute tempogram computation on onset strength buffer

  Computes the Fourier tempogram from a buffer of onset strength values.
  The output is a 2D matrix where rows represent tempo bins (BPM) and
  columns represent time frames.

  \param o tempogram object
  \param onset input buffer of onset strength values
  \param tempogram output tempogram matrix (rows = tempo bins, cols = time)

*/
void aubio_tempogram_do (aubio_tempogram_t * o, const fvec_t * onset,
    fmat_t * tempogram);

/** get dominant tempo from tempogram

  Extracts the most prominent tempo from the tempogram by finding the
  frequency bin with maximum energy.

  \param o tempogram object
  \param tempogram tempogram matrix to analyze

  \return dominant tempo in BPM

*/
smpl_t aubio_tempogram_get_tempo (aubio_tempogram_t * o,
    const fmat_t * tempogram);

/** get tempo confidence

  Returns the confidence of the last tempo detection, normalized between 0 and 1.

  \param o tempogram object

  \return confidence value (0.0 to 1.0)

*/
smpl_t aubio_tempogram_get_confidence (const aubio_tempogram_t * o);

/** set minimum tempo for analysis

  \param o tempogram object
  \param tempo_min minimum tempo in BPM (default: 30)

  \return 0 on success, 1 on error

*/
uint_t aubio_tempogram_set_tempo_min (aubio_tempogram_t * o, smpl_t tempo_min);

/** set maximum tempo for analysis

  \param o tempogram object
  \param tempo_max maximum tempo in BPM (default: 300)

  \return 0 on success, 1 on error

*/
uint_t aubio_tempogram_set_tempo_max (aubio_tempogram_t * o, smpl_t tempo_max);

/** get predominant local pulse (PLP) at specific time

  Extracts the dominant tempo at a specific time frame in the tempogram.
  This enables time-varying tempo tracking.

  \param o tempogram object
  \param tempogram tempogram matrix
  \param time_idx time frame index

  \return tempo in BPM at the specified time

*/
smpl_t aubio_tempogram_get_plp_at_time (aubio_tempogram_t * o,
    const fmat_t * tempogram, uint_t time_idx);

/** get predominant local pulse (PLP) curve

  Extracts the time-varying tempo curve from the tempogram using the
  Predominant Local Pulse method. This provides a smooth tempo trajectory
  that adapts to tempo changes.

  \param o tempogram object
  \param tempogram tempogram matrix
  \param plp_curve output vector for tempo curve (one value per time frame)

*/
void aubio_tempogram_get_plp_curve (aubio_tempogram_t * o,
    const fmat_t * tempogram, fvec_t * plp_curve);

#ifdef __cplusplus
}
#endif

#endif /* AUBIO_TEMPOGRAM_H */
