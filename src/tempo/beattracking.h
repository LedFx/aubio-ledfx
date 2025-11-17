/*
  Copyright (C) 2003-2015 Matthew Davies and Paul Brossier <piem@aubio.org>

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

  Beat tracking using a context dependant model

  This file implements the causal beat tracking algorithm designed by Matthew
  Davies and described in the following articles:

  Matthew E. P. Davies and Mark D. Plumbley. Causal tempo tracking of audio.
  In Proceedings of the International Symposium on Music Information Retrieval
  (ISMIR), pages 164­169, Barcelona, Spain, 2004.

  Matthew E. P. Davies, Paul Brossier, and Mark D. Plumbley. Beat tracking
  towards automatic musical accompaniment. In Proceedings of the Audio
  Engineering Society 118th Convention, Barcelona, Spain, May 2005.

  \example tempo/test-beattracking.c

*/
#ifndef AUBIO_BEATTRACKING_H
#define AUBIO_BEATTRACKING_H

#ifdef __cplusplus
extern "C" {
#endif

/** beat tracking object */
typedef struct _aubio_beattracking_t aubio_beattracking_t;

/** create beat tracking object

  \param winlen length of the onset detection window
  \param hop_size number of onset detection samples [512]
  \param samplerate samplerate of the input signal

*/
aubio_beattracking_t * new_aubio_beattracking(uint_t winlen, uint_t hop_size,
    uint_t samplerate);

/** track the beat

  \param bt beat tracking object
  \param dfframes current input detection function frame, smoothed by
  adaptive median threshold.
  \param out stored detected beat locations

*/
void aubio_beattracking_do (aubio_beattracking_t * bt, const fvec_t * dfframes,
    fvec_t * out);

/** get current beat period in samples

  \param bt beat tracking object

  Returns the currently observed period, in samples, or 0 if no consistent
  value is found.

*/
smpl_t aubio_beattracking_get_period (const aubio_beattracking_t * bt);

/** get current beat period in seconds

  \param bt beat tracking object

  Returns the currently observed period, in seconds, or 0 if no consistent
  value is found.

*/
smpl_t aubio_beattracking_get_period_s (const aubio_beattracking_t * bt);

/** get current tempo in bpm

  \param bt beat tracking object

  Returns the currently observed tempo, in beats per minutes, or 0 if no
  consistent value is found.

*/
smpl_t aubio_beattracking_get_bpm(const aubio_beattracking_t * bt);

/** get current tempo confidence

  \param bt beat tracking object

  Returns the confidence with which the tempo has been observed, 0 if no
  consistent value is found.

*/
smpl_t aubio_beattracking_get_confidence(const aubio_beattracking_t * bt);

/** set tempo prior mean

  \param bt beat tracking object
  \param tempo_mean prior mean tempo in BPM (default: 120.0)

  \return `0` if successful, non-zero otherwise

*/
uint_t aubio_beattracking_set_tempo_prior_mean(aubio_beattracking_t * bt, smpl_t tempo_mean);

/** set tempo prior standard deviation

  \param bt beat tracking object
  \param tempo_std prior standard deviation in BPM (default: 1.0)

  \return `0` if successful, non-zero otherwise

*/
uint_t aubio_beattracking_set_tempo_prior_std(aubio_beattracking_t * bt, smpl_t tempo_std);

/** enable adaptive window sizing for faster response

  When enabled, the analysis window size is reduced when tempo is stable
  and high confidence, allowing faster response to tempo changes.

  \param bt beat tracking object
  \param enabled 1 to enable adaptive sizing, 0 to disable

  \return `0` if successful, non-zero otherwise

*/
uint_t aubio_beattracking_set_adaptive_winlen(aubio_beattracking_t * bt, uint_t enabled);

/** enable multi-octave tempo detection

  When enabled, the detector checks if the detected tempo might be half or
  double the actual tempo, and corrects it based on the tempo prior or
  heuristics. This improves detection of slow tempos (< 80 BPM) and very
  fast tempos (> 200 BPM).

  \param bt beat tracking object
  \param enabled 1 to enable multi-octave detection (default), 0 to disable

  \return `0` if successful, non-zero otherwise

*/
uint_t aubio_beattracking_set_multi_octave(aubio_beattracking_t * bt, uint_t enabled);

/** enable dynamic tempo tracking (Phase 3)

  When enabled, the beat tracker stores a history of instantaneous tempo
  estimates, allowing frame-by-frame tempo analysis and variance calculation.
  This enables detection of tempo changes and time-varying tempo tracking.

  \param bt beat tracking object
  \param enabled 1 to enable dynamic tempo tracking, 0 to disable (default)

  \return `0` if successful, non-zero otherwise

*/
uint_t aubio_beattracking_set_dynamic_tempo(aubio_beattracking_t * bt, uint_t enabled);

/** get instantaneous tempo estimate (Phase 3)

  Returns the current frame's tempo estimate before smoothing. Useful for
  detecting rapid tempo changes or analyzing time-varying tempo.

  \param bt beat tracking object

  \return instantaneous tempo in BPM, 0 if no tempo detected

*/
smpl_t aubio_beattracking_get_instantaneous_bpm(const aubio_beattracking_t * bt);

/** get tempo variance over recent history (Phase 3)

  Calculates variance of recent tempo estimates from the history buffer.
  Higher variance indicates unstable or changing tempo. Requires dynamic
  tempo tracking to be enabled.

  \param bt beat tracking object

  \return tempo variance in BPM², 0 if dynamic tempo disabled

*/
smpl_t aubio_beattracking_get_tempo_variance(const aubio_beattracking_t * bt);

/** enable FFT-based autocorrelation (Phase 3 Advanced)

  When enabled, uses FFT-based autocorrelation which is O(N log N) instead
  of O(N²) for direct computation. Significantly faster for large windows
  (> 512 samples). Enabled automatically for windows >= 512.

  \param bt beat tracking object
  \param enabled 1 to enable FFT autocorrelation, 0 for direct method

  \return `0` if successful, non-zero otherwise

*/
uint_t aubio_beattracking_set_fft_autocorr(aubio_beattracking_t * bt, uint_t enabled);

/** enable Fourier tempogram-based tempo detection

  When enabled, uses tempogram analysis instead of direct autocorrelation
  for tempo detection. Provides multi-resolution analysis and better
  time-varying tempo tracking.

  \param bt beat tracking object
  \param enabled 1 to enable tempogram, 0 to disable

  \return `0` if successful, non-zero otherwise

*/
uint_t aubio_beattracking_set_use_tempogram(aubio_beattracking_t * bt, uint_t enabled);

/** get autocorrelation function (for debugging/analysis)

  Returns the most recently computed autocorrelation function. Useful for
  advanced tempo analysis and visualization.

  \param bt beat tracking object
  \param acf output vector to store autocorrelation (will be resized if needed)

*/
void aubio_beattracking_get_acf(const aubio_beattracking_t * bt, fvec_t * acf);

/** delete beat tracking object

  \param p beat tracking object

*/
void del_aubio_beattracking(aubio_beattracking_t * p);

#ifdef __cplusplus
}
#endif

#endif /* AUBIO_BEATTRACKING_H */
