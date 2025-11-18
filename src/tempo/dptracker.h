/*
  Copyright (C) 2024 aubio-ledfx contributors

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

  Dynamic Programming Beat Tracker

  This file implements the Ellis (2007) dynamic programming beat tracking
  algorithm which finds the globally optimal beat sequence by balancing
  local onset strength with tempo continuity constraints.

  Reference:
  Ellis, D. P. W. (2007). "Beat Tracking by Dynamic Programming".
  Journal of New Music Research, 36(1), 51-60.

  \example tempo/test-dptracker-basic.c

*/

#ifndef AUBIO_DPTRACKER_H
#define AUBIO_DPTRACKER_H

#ifdef __cplusplus
extern "C" {
#endif

/** DP beat tracker object */
typedef struct _aubio_dptracker_t aubio_dptracker_t;

/** create DP beat tracker

  Creates a dynamic programming beat tracker that finds the optimal beat
  sequence by maximizing a global score combining onset strength and tempo
  continuity.

  \param win_s DP window size in frames (e.g., 512)
  \param hop_s hop size in samples (e.g., 256)
  \param samplerate sample rate of input signal (e.g., 44100)

  \return newly created ::aubio_dptracker_t

*/
aubio_dptracker_t *new_aubio_dptracker(uint_t win_s, uint_t hop_s,
                                        uint_t samplerate);

/** delete DP beat tracker

  \param dp DP tracker to delete

*/
void del_aubio_dptracker(aubio_dptracker_t *dp);

/** process one onset value through DP tracker

  Feeds a single onset value to the DP tracker and updates the dynamic
  programming trellis with the new observation.

  \param dp DP tracker object
  \param onset_value onset strength at current frame (typically 0.0 to 1.0)

*/
void aubio_dptracker_do(aubio_dptracker_t *dp, smpl_t onset_value);

/** get current beat sequence from DP path

  Extracts the beat sequence by backtracking through the DP trellis from
  the highest scoring end point. Uses Viterbi algorithm.

  \param dp DP tracker object
  \param beats output vector to store beat frame indices

*/
void aubio_dptracker_get_beats(aubio_dptracker_t *dp, fvec_t *beats);

/** get current tempo estimate from DP path

  Computes tempo in BPM from the average inter-beat interval in the
  optimal beat sequence.

  \param dp DP tracker object

  \return tempo in beats per minute, or 0 if insufficient beats detected

*/
smpl_t aubio_dptracker_get_bpm(const aubio_dptracker_t *dp);

/** get path confidence score

  Returns the confidence of the current beat sequence based on the
  cumulative DP score.

  \param dp DP tracker object

  \return confidence score (higher = more confident)

*/
smpl_t aubio_dptracker_get_confidence(const aubio_dptracker_t *dp);

/** set tempo prior for DP model

  Sets the expected tempo and uncertainty for the tempo continuity model.
  This affects the cost function penalty for deviating from expected
  inter-beat intervals.

  \param dp DP tracker object
  \param bpm expected tempo in beats per minute
  \param std_bpm tempo uncertainty (standard deviation in BPM)

  \return 0 on success, non-zero on error

*/
uint_t aubio_dptracker_set_tempo(aubio_dptracker_t *dp, smpl_t bpm,
                                  smpl_t std_bpm);

/** get current tempo prior mean

  \param dp DP tracker object

  \return current tempo prior mean in BPM

*/
smpl_t aubio_dptracker_get_tempo_mean(const aubio_dptracker_t *dp);

/** get current tempo prior standard deviation

  \param dp DP tracker object

  \return current tempo prior standard deviation in BPM

*/
smpl_t aubio_dptracker_get_tempo_std(const aubio_dptracker_t *dp);

/** get number of beats in current sequence

  \param dp DP tracker object

  \return number of beats detected in optimal path

*/
uint_t aubio_dptracker_get_num_beats(const aubio_dptracker_t *dp);

/** reset DP tracker state

  Clears all internal buffers and resets the DP tracker to initial state.
  Useful when processing a new audio file.

  \param dp DP tracker object

*/
void aubio_dptracker_reset(aubio_dptracker_t *dp);

#ifdef __cplusplus
}
#endif

#endif /* AUBIO_DPTRACKER_H */
