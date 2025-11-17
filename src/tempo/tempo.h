/*
  Copyright (C) 2006-2013 Paul Brossier <piem@aubio.org>

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

  Tempo detection object

  This object stores all the memory required for tempo detection algorithm
  and returns the estimated beat locations.

  \example tempo/test-tempo.c
  \example examples/aubiotrack.c

*/

#ifndef AUBIO_TEMPO_H
#define AUBIO_TEMPO_H

#ifdef __cplusplus
extern "C" {
#endif

/** tempo detection structure */
typedef struct _aubio_tempo_t aubio_tempo_t;

/** create tempo detection object

  \param method beat tracking method, unused for now (use "default")
  \param buf_size length of FFT
  \param hop_size number of frames between two consecutive runs
  \param samplerate sampling rate of the signal to analyze

  \return newly created ::aubio_tempo_t if successful, `NULL` otherwise

*/
aubio_tempo_t * new_aubio_tempo (const char_t * method,
    uint_t buf_size, uint_t hop_size, uint_t samplerate);

/** execute tempo detection

  \param o beat tracking object
  \param input new samples
  \param tempo output beats

*/
void aubio_tempo_do (aubio_tempo_t *o, const fvec_t * input, fvec_t * tempo);

/** get the time of the latest beat detected, in samples

  \param o tempo detection object as returned by ::new_aubio_tempo

*/
uint_t aubio_tempo_get_last (aubio_tempo_t *o);

/** get the time of the latest beat detected, in seconds

  \param o tempo detection object as returned by ::new_aubio_tempo

*/
smpl_t aubio_tempo_get_last_s (aubio_tempo_t *o);

/** get the time of the latest beat detected, in milliseconds

  \param o tempo detection object as returned by ::new_aubio_tempo

*/
smpl_t aubio_tempo_get_last_ms (aubio_tempo_t *o);

/** set tempo detection silence threshold

  \param o beat tracking object
  \param silence new silence threshold, in dB

  \return `0` if successful, non-zero otherwise

*/
uint_t aubio_tempo_set_silence(aubio_tempo_t * o, smpl_t silence);

/** get tempo detection silence threshold

  \param o tempo detection object as returned by new_aubio_tempo()

  \return current silence threshold

*/
smpl_t aubio_tempo_get_silence(aubio_tempo_t * o);

/** set tempo detection peak picking threshold

  \param o beat tracking object
  \param threshold new threshold

  \return `0` if successful, non-zero otherwise

*/
uint_t aubio_tempo_set_threshold(aubio_tempo_t * o, smpl_t threshold);

/** get tempo peak picking threshold

  \param o tempo detection object as returned by new_aubio_tempo()

  \return current tempo detection threshold

*/
smpl_t aubio_tempo_get_threshold(aubio_tempo_t * o);

/** get current beat period in samples

  \param bt beat tracking object

  Returns the currently observed period, in samples, or 0 if no consistent
  value is found.

*/
smpl_t aubio_tempo_get_period (aubio_tempo_t * bt);

/** get current beat period in seconds

  \param bt beat tracking object

  Returns the currently observed period, in seconds, or 0 if no consistent
  value is found.

*/
smpl_t aubio_tempo_get_period_s (aubio_tempo_t * bt);

/** get current tempo

  \param o beat tracking object

  \return the currently observed tempo, or `0` if no consistent value is found

*/
smpl_t aubio_tempo_get_bpm(aubio_tempo_t * o);

/** get current tempo confidence

  \param o beat tracking object

  \return confidence with which the tempo has been observed, the higher the
  more confidence, `0` if no consistent value is found.

*/
smpl_t aubio_tempo_get_confidence(aubio_tempo_t * o);

/** set number of tatum per beat

   \param o beat tracking object
   \param signature number of tatum per beat (between 1 and 64)

*/
uint_t aubio_tempo_set_tatum_signature(aubio_tempo_t *o, uint_t signature);

/** check whether a tatum was detected in the current frame

   \param o beat tracking object

   \return 2 if a beat was detected, 1 if a tatum was detected, 0 otherwise

*/
uint_t aubio_tempo_was_tatum(aubio_tempo_t *o);

/** get position of last_tatum, in samples

   \param o beat tracking object

*/
smpl_t aubio_tempo_get_last_tatum(aubio_tempo_t *o);

/** get current delay

  \param o beat tracking object

  \return current delay, in samples

 */
uint_t aubio_tempo_get_delay(aubio_tempo_t * o);

/** get current delay in seconds

  \param o beat tracking object

  \return current delay, in seconds

 */
smpl_t aubio_tempo_get_delay_s(aubio_tempo_t * o);

/** get current delay in ms

  \param o beat tracking object

  \return current delay, in milliseconds

 */
smpl_t aubio_tempo_get_delay_ms(aubio_tempo_t * o);

/** set current delay

  \param o beat tracking object
  \param delay delay to set tempo to, in samples

  \return `0` if successful, non-zero otherwise

 */
uint_t aubio_tempo_set_delay(aubio_tempo_t * o, sint_t delay);

/** set current delay in seconds

  \param o beat tracking object
  \param delay delay to set tempo to, in seconds

  \return `0` if successful, non-zero otherwise

 */
uint_t aubio_tempo_set_delay_s(aubio_tempo_t * o, smpl_t delay);

/** set current delay

  \param o beat tracking object
  \param delay delay to set tempo to, in samples

  \return `0` if successful, non-zero otherwise

 */
uint_t aubio_tempo_set_delay_ms(aubio_tempo_t * o, smpl_t delay);

/** set tempo prior mean

  \param o beat tracking object
  \param tempo_mean prior mean tempo in BPM (default: 120.0)

  \return `0` if successful, non-zero otherwise

 */
uint_t aubio_tempo_set_tempo_prior_mean(aubio_tempo_t * o, smpl_t tempo_mean);

/** set tempo prior standard deviation

  \param o beat tracking object
  \param tempo_std prior standard deviation in BPM (default: 1.0)

  \return `0` if successful, non-zero otherwise

 */
uint_t aubio_tempo_set_tempo_prior_std(aubio_tempo_t * o, smpl_t tempo_std);

/** enable adaptive window sizing for faster response

  When enabled, effective analysis is reduced when tempo is stable,
  allowing faster response to tempo changes (2-3s instead of 6s).

  \param o beat tracking object  
  \param enabled 1 to enable adaptive sizing, 0 to disable

  \return `0` if successful, non-zero otherwise

 */
uint_t aubio_tempo_set_adaptive_winlen(aubio_tempo_t * o, uint_t enabled);

/** enable multi-octave tempo detection

  When enabled, checks if detected tempo might be half or double the actual
  tempo, correcting octave errors. Improves detection of slow (< 80 BPM) and
  fast (> 200 BPM) tempos. Enabled by default.

  \param o tempo object
  \param enabled 1 to enable multi-octave detection, 0 to disable

  \return `0` if successful, non-zero otherwise

 */
uint_t aubio_tempo_set_multi_octave(aubio_tempo_t * o, uint_t enabled);

/** enable dynamic tempo tracking (Phase 3)

  When enabled, stores history of instantaneous tempo estimates for frame-by-
  frame analysis. Allows detection of tempo changes and time-varying tempo.

  \param o tempo object
  \param enabled 1 to enable dynamic tracking, 0 to disable (default)

  \return `0` if successful, non-zero otherwise

 */
uint_t aubio_tempo_set_dynamic_tempo(aubio_tempo_t * o, uint_t enabled);

/** get instantaneous tempo estimate (Phase 3)

  Returns current frame's tempo before smoothing. Useful for detecting rapid
  tempo changes. Requires dynamic tempo tracking enabled.

  \param o tempo object

  \return instantaneous tempo in BPM, 0 if no tempo detected

 */
smpl_t aubio_tempo_get_instantaneous_bpm(const aubio_tempo_t * o);

/** get tempo variance over recent history (Phase 3)

  Calculates variance of recent tempo estimates. Higher variance indicates
  unstable or changing tempo. Requires dynamic tempo tracking enabled.

  \param o tempo object

  \return tempo variance in BPM², 0 if dynamic tempo disabled

 */
smpl_t aubio_tempo_get_tempo_variance(const aubio_tempo_t * o);

/** enable FFT-based autocorrelation (Phase 3 Advanced)

  When enabled, uses FFT-based autocorrelation which is O(N log N) instead of
  O(N²). Significantly faster for large windows (> 512 samples). Auto-enabled
  for windows >= 512.

  \param o tempo object
  \param enabled 1 to enable FFT autocorrelation, 0 for direct method

  \return `0` if successful, non-zero otherwise

 */
uint_t aubio_tempo_set_fft_autocorr(aubio_tempo_t * o, uint_t enabled);

/** enable Fourier tempogram-based tempo detection

  When enabled, uses Short-Time Fourier Transform (STFT) on the onset
  strength envelope to compute a time-frequency tempo representation.
  This provides multi-resolution analysis and better time-varying tempo tracking.

  \param o tempo detection object
  \param enabled 1 to enable tempogram method, 0 for autocorrelation

  \return `0` if successful, non-zero otherwise

 */
uint_t aubio_tempo_set_use_tempogram(aubio_tempo_t * o, uint_t enabled);

/** enable onset enhancement for tempogram (Phase 3A)

  When enabled, applies median filtering and adaptive thresholding to onset
  signals before feeding to tempogram. Improves beat detection on polyphonic
  music by reducing noise from overlapping drum sounds. Enabled by default
  when tempogram is enabled.

  \param o tempo detection object
  \param enabled 1 to enable onset enhancement (default), 0 to disable

  \return `0` if successful, non-zero otherwise

 */
uint_t aubio_tempo_set_onset_enhancement(aubio_tempo_t * o, uint_t enabled);

/** delete tempo detection object

  \param o beat tracking object

*/
void del_aubio_tempo(aubio_tempo_t * o);

#ifdef __cplusplus
}
#endif

#endif /* AUBIO_TEMPO_H */
