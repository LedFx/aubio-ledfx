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

#include "aubio_priv.h"
#include "fvec.h"
#include "lvec.h"
#include "mathutils.h"
#include "tempo/dptracker.h"

/** DP beat tracker structure */
struct _aubio_dptracker_t {
  uint_t win_s;              /**< DP window size (frames) */
  uint_t hop_s;              /**< hop size (samples) */
  uint_t samplerate;         /**< sample rate (Hz) */
  
  // DP state
  fvec_t *dp_score;          /**< DP[i] - best cumulative score to frame i */
  lvec_t *dp_backptr;        /**< PREV[i] - previous beat index for path */
  fvec_t *onset_buffer;      /**< circular buffer of onset values */
  uint_t buffer_pos;         /**< current position in circular buffer */
  uint_t frames_processed;   /**< total frames processed */
  
  // Tempo model
  smpl_t ideal_interval;     /**< δ̂ - ideal inter-beat interval (frames) */
  smpl_t tempo_mean_bpm;     /**< expected tempo (BPM) */
  smpl_t tempo_std_bpm;      /**< tempo uncertainty (BPM) */
  
  // Search bounds (efficiency)
  uint_t min_interval;       /**< minimum beat interval (frames) */
  uint_t max_interval;       /**< maximum beat interval (frames) */
  
  // Output
  lvec_t *beat_sequence;     /**< final beat positions (frame indices) */
  uint_t num_beats;          /**< number of beats in sequence */
  smpl_t confidence;         /**< path confidence score */
  smpl_t last_bpm;           /**< most recent tempo estimate */
};

/** compute tempo continuity penalty
 *
 * Penalty function: P_δ̂(δ) = -[log₂(δ/δ̂)]²
 * 
 * \param delta actual inter-beat interval
 * \param ideal_interval expected inter-beat interval δ̂
 * \return penalty value (0 when delta == ideal, negative otherwise)
 */
static smpl_t
compute_penalty(smpl_t delta, smpl_t ideal_interval)
{
  // Avoid division by zero
  if (ideal_interval <= 0.0 || delta <= 0.0) {
    return -100.0;  // Large penalty for invalid intervals
  }
  
  // Compute log₂(δ/δ̂)
  smpl_t log_ratio = LOG(delta / ideal_interval) / LOG(2.0);
  
  // Return -[log₂(δ/δ̂)]²
  return -(log_ratio * log_ratio);
}

aubio_dptracker_t *
new_aubio_dptracker(uint_t win_s, uint_t hop_s, uint_t samplerate)
{
  aubio_dptracker_t *dp = AUBIO_NEW(aubio_dptracker_t);
  
  // Validate inputs
  AUBIO_ASSERT_NOT_NULL(dp);
  AUBIO_ASSERT_RANGE(win_s, 64, 4096);
  AUBIO_ASSERT_RANGE(hop_s, 64, 8192);
  AUBIO_ASSERT_RANGE(samplerate, 8000, 192000);
  
  // Store parameters
  dp->win_s = win_s;
  dp->hop_s = hop_s;
  dp->samplerate = samplerate;
  dp->buffer_pos = 0;
  dp->frames_processed = 0;
  dp->num_beats = 0;
  dp->confidence = 0.0;
  dp->last_bpm = 0.0;
  
  // Allocate DP buffers
  dp->dp_score = new_fvec(win_s);
  if (!dp->dp_score) goto beach;
  
  dp->dp_backptr = new_lvec(win_s);
  if (!dp->dp_backptr) goto beach;
  
  dp->onset_buffer = new_fvec(win_s);
  if (!dp->onset_buffer) goto beach;
  
  dp->beat_sequence = new_lvec(win_s);
  if (!dp->beat_sequence) goto beach;
  
  // Set default tempo (120 BPM with ±10 BPM uncertainty)
  aubio_dptracker_set_tempo(dp, 120.0, 10.0);
  
  return dp;
  
beach:
  if (dp->dp_score) del_fvec(dp->dp_score);
  if (dp->dp_backptr) del_lvec(dp->dp_backptr);
  if (dp->onset_buffer) del_fvec(dp->onset_buffer);
  if (dp->beat_sequence) del_lvec(dp->beat_sequence);
  AUBIO_FREE(dp);
  return NULL;
}

void
del_aubio_dptracker(aubio_dptracker_t *dp)
{
  AUBIO_ASSERT_NOT_NULL(dp);
  
  if (dp->dp_score) del_fvec(dp->dp_score);
  if (dp->dp_backptr) del_lvec(dp->dp_backptr);
  if (dp->onset_buffer) del_fvec(dp->onset_buffer);
  if (dp->beat_sequence) del_lvec(dp->beat_sequence);
  
  AUBIO_FREE(dp);
}

uint_t
aubio_dptracker_set_tempo(aubio_dptracker_t *dp, smpl_t bpm, smpl_t std_bpm)
{
  AUBIO_ASSERT_NOT_NULL(dp);
  AUBIO_ASSERT_RANGE(bpm, 30.0, 300.0);
  AUBIO_ASSERT_RANGE(std_bpm, 0.1, 50.0);
  
  dp->tempo_mean_bpm = bpm;
  dp->tempo_std_bpm = std_bpm;
  
  // Compute ideal inter-beat interval in frames
  // BPM = 60 * samplerate / (interval_samples)
  // interval_samples = 60 * samplerate / BPM
  // interval_frames = interval_samples / hop_s
  smpl_t interval_samples = 60.0 * dp->samplerate / bpm;
  dp->ideal_interval = interval_samples / dp->hop_s;
  
  // Compute search bounds based on tempo ± 2*std
  smpl_t min_bpm = bpm - 2.0 * std_bpm;
  smpl_t max_bpm = bpm + 2.0 * std_bpm;
  
  // Clamp to reasonable range
  if (min_bpm < 30.0) min_bpm = 30.0;
  if (max_bpm > 300.0) max_bpm = 300.0;
  
  // Compute interval bounds (note: min_bpm -> max_interval)
  smpl_t max_interval_samples = 60.0 * dp->samplerate / min_bpm;
  smpl_t min_interval_samples = 60.0 * dp->samplerate / max_bpm;
  
  dp->max_interval = (uint_t)(max_interval_samples / dp->hop_s);
  dp->min_interval = (uint_t)(min_interval_samples / dp->hop_s);
  
  // Ensure min_interval >= 1
  if (dp->min_interval < 1) dp->min_interval = 1;
  
  // Ensure max_interval <= win_s
  if (dp->max_interval > dp->win_s) dp->max_interval = dp->win_s;
  
  return AUBIO_OK;
}

void
aubio_dptracker_do(aubio_dptracker_t *dp, smpl_t onset_value)
{
  AUBIO_ASSERT_NOT_NULL(dp);
  
  uint_t i = dp->buffer_pos;
  AUBIO_ASSERT_BOUNDS(i, dp->win_s);
  
  // Store onset value
  dp->onset_buffer->data[i] = onset_value;
  
  // Initialize DP score for current frame
  dp->dp_score->data[i] = onset_value;
  dp->dp_backptr->data[i] = -1;  // No predecessor yet
  
  // Search for best predecessor
  smpl_t best_score = onset_value;
  sint_t best_prev = -1;
  
  // Determine search range
  // j ranges from (i - max_interval) to (i - min_interval)
  sint_t j_min, j_max;
  
  if (dp->frames_processed < dp->max_interval) {
    // Early in processing - can't look back max_interval frames
    j_min = 0;
  } else {
    j_min = i - dp->max_interval;
    if (j_min < 0) j_min += dp->win_s;  // Wrap around circular buffer
  }
  
  if (dp->frames_processed < dp->min_interval) {
    // Not enough frames for minimum interval
    j_max = -1;  // No valid predecessors
  } else {
    j_max = i - dp->min_interval;
    if (j_max < 0) j_max += dp->win_s;  // Wrap around circular buffer
  }
  
  // Search for best predecessor
  if (j_max >= 0) {
    uint_t j = j_min;
    while (1) {
      // Compute inter-beat interval
      sint_t delta_signed;
      if (j <= i) {
        delta_signed = i - j;
      } else {
        // Wrapped around circular buffer
        delta_signed = (dp->win_s - j) + i;
      }
      
      smpl_t delta = (smpl_t)delta_signed;
      
      // Compute tempo continuity penalty
      smpl_t penalty = compute_penalty(delta, dp->ideal_interval);
      
      // Compute cumulative score via this path
      smpl_t score = dp->dp_score->data[j] + onset_value + penalty;
      
      // Update if better
      if (score > best_score) {
        best_score = score;
        best_prev = j;
      }
      
      // Advance j
      if (j == j_max) break;
      j = (j + 1) % dp->win_s;
    }
  }
  
  // Store best path
  dp->dp_score->data[i] = best_score;
  dp->dp_backptr->data[i] = best_prev;
  
  // Advance buffer position
  dp->buffer_pos = (dp->buffer_pos + 1) % dp->win_s;
  dp->frames_processed++;
}

void
aubio_dptracker_get_beats(aubio_dptracker_t *dp, fvec_t *beats)
{
  AUBIO_ASSERT_NOT_NULL(dp);
  AUBIO_ASSERT_NOT_NULL(beats);
  
  // Find highest score in DP table
  uint_t best_end = 0;
  smpl_t best_score = dp->dp_score->data[0];
  
  for (uint_t i = 1; i < dp->win_s; i++) {
    if (dp->dp_score->data[i] > best_score) {
      best_score = dp->dp_score->data[i];
      best_end = i;
    }
  }
  
  // Store confidence
  dp->confidence = best_score;
  
  // Backtrack to recover beat sequence
  uint_t max_beats = MIN(dp->beat_sequence->length, beats->length);
  uint_t num_beats = 0;
  sint_t idx = best_end;
  
  while (idx >= 0 && num_beats < max_beats) {
    AUBIO_ASSERT_BOUNDS(idx, dp->win_s);
    dp->beat_sequence->data[num_beats] = idx;
    num_beats++;
    idx = dp->dp_backptr->data[idx];
  }
  
  dp->num_beats = num_beats;
  
  // Reverse sequence (was built backwards) and copy to output
  uint_t out_len = MIN(num_beats, beats->length);
  for (uint_t i = 0; i < out_len; i++) {
    beats->data[i] = (smpl_t)dp->beat_sequence->data[num_beats - 1 - i];
  }
  
  // Clear remaining positions
  for (uint_t i = out_len; i < beats->length; i++) {
    beats->data[i] = 0.0;
  }
}

smpl_t
aubio_dptracker_get_bpm(const aubio_dptracker_t *dp)
{
  AUBIO_ASSERT_NOT_NULL(dp);
  
  if (dp->num_beats < 2) {
    return 0.0;  // Not enough beats
  }
  
  // Compute average inter-beat interval
  // Note: beat_sequence is stored in reverse order (newest to oldest)
  // So we need to reverse the subtraction
  smpl_t total_interval = 0.0;
  for (uint_t i = 1; i < dp->num_beats; i++) {
    // Since sequence is reversed, compute (older - newer) for positive interval
    smpl_t delta = (smpl_t)(dp->beat_sequence->data[i-1] - 
                             dp->beat_sequence->data[i]);
    total_interval += ABS(delta);  // Use absolute value to handle any edge cases
  }
  
  smpl_t avg_interval_frames = total_interval / (dp->num_beats - 1);
  
  // Convert to samples
  smpl_t avg_interval_samples = avg_interval_frames * dp->hop_s;
  
  // Convert to BPM
  smpl_t bpm = 60.0 * dp->samplerate / avg_interval_samples;
  
  return bpm;
}

smpl_t
aubio_dptracker_get_confidence(const aubio_dptracker_t *dp)
{
  AUBIO_ASSERT_NOT_NULL(dp);
  return dp->confidence;
}

smpl_t
aubio_dptracker_get_tempo_mean(const aubio_dptracker_t *dp)
{
  AUBIO_ASSERT_NOT_NULL(dp);
  return dp->tempo_mean_bpm;
}

smpl_t
aubio_dptracker_get_tempo_std(const aubio_dptracker_t *dp)
{
  AUBIO_ASSERT_NOT_NULL(dp);
  return dp->tempo_std_bpm;
}

uint_t
aubio_dptracker_get_num_beats(const aubio_dptracker_t *dp)
{
  AUBIO_ASSERT_NOT_NULL(dp);
  return dp->num_beats;
}

void
aubio_dptracker_reset(aubio_dptracker_t *dp)
{
  AUBIO_ASSERT_NOT_NULL(dp);
  
  // Clear all buffers
  fvec_zeros(dp->dp_score);
  lvec_set_all(dp->dp_backptr, -1);
  fvec_zeros(dp->onset_buffer);
  lvec_set_all(dp->beat_sequence, 0);
  
  // Reset state
  dp->buffer_pos = 0;
  dp->frames_processed = 0;
  dp->num_beats = 0;
  dp->confidence = 0.0;
  dp->last_bpm = 0.0;
}
