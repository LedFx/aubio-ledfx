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

#include "aubio_priv.h"
#include "fvec.h"
#include "fmat.h"
#include "cvec.h"
#include "spectral/fft.h"
#include "tempo/tempogram.h"

/** tempogram structure */
struct _aubio_tempogram_t {
  uint_t win_s;              /**< tempogram window size */
  uint_t hop_s;              /**< hop size */
  uint_t samplerate;         /**< sample rate */

  aubio_fft_t *fft;          /**< FFT processor */
  fvec_t *window;            /**< Hann window function */
  fvec_t *fft_input;         /**< windowed onset buffer for FFT input */
  cvec_t *fftout;            /**< FFT output (complex spectrum) */
  fvec_t *magnitude;         /**< magnitude spectrum */

  uint_t buffer_size;        /**< circular buffer size for onset frames */
  fmat_t *onset_buffer;      /**< circular buffer storing onset frames */
  uint_t buffer_write_pos;   /**< current write position in buffer */

  smpl_t tempo_min_bpm;      /**< minimum tempo in BPM */
  smpl_t tempo_max_bpm;      /**< maximum tempo in BPM */
  uint_t tempo_min_idx;      /**< FFT bin index for min tempo */
  uint_t tempo_max_idx;      /**< FFT bin index for max tempo */

  fvec_t *bpm_bins;          /**< BPM value for each FFT bin */
  smpl_t current_tempo;      /**< last detected tempo */
  smpl_t confidence;         /**< detection confidence */
  
  uint_t plp_smoothing_window; /**< window size for PLP curve smoothing (0 = no smoothing) */
};

/** convert BPM to FFT bin index */
static uint_t
aubio_tempogram_bpm_to_bin (const aubio_tempogram_t * o, smpl_t bpm)
{
  AUBIO_ASSERT_NOT_NULL (o);
  
  // BPM to beat frequency: f = BPM / 60
  // Onset sample rate (samples per second in onset time series): sr_onset = samplerate / hop_s
  // FFT bin corresponds to: bin = f * win_s / sr_onset
  // Combined: bin = (BPM / 60) * win_s / (samplerate / hop_s)
  //                = (BPM * win_s * hop_s) / (60 * samplerate)
  //
  // WAIT - this is wrong! Let me recalculate:
  // Onset samples are taken every hop_s audio samples
  // So onset_sample_rate = samplerate / hop_s (in onset samples per second)
  // FFT frequency resolution = onset_sample_rate / win_s
  // To find bin for beat frequency f: bin = f / freq_resolution
  //                                      = f * win_s / onset_sample_rate
  //                                      = f * win_s / (samplerate / hop_s)
  //                                      = f * win_s * hop_s / samplerate
  //
  // NO! That's still wrong. Let's be very careful:
  // - We collect onset values at rate: onset_rate = samplerate / hop_s (in Hz of onset timeline)
  // - FFT window contains win_s onset samples
  // - FFT frequency resolution: delta_f = onset_rate / win_s = (samplerate/hop_s) / win_s
  // - For beat frequency f_beat, bin index: bin = f_beat / delta_f
  //                                            = f_beat * win_s / (samplerate/hop_s)
  //                                            = f_beat * win_s * hop_s / samplerate
  //
  // Actually the original formula WAS correct! Let me verify with an example:
  // 120 BPM, win_s=512, hop_s=256, sr=44100
  // f_beat = 120/60 = 2 Hz
  // onset_rate = 44100/256 = 172.27 Hz  
  // delta_f = 172.27/512 = 0.336 Hz/bin
  // bin = 2 / 0.336 = 5.95
  //
  // Using formula: bin = 2 * 512 * 256 / 44100 = 5.93 ✓ CORRECT!
  //
  // So the formula is right, but maybe there's a different issue...
  // Actually wait - let me check the FFT setup. The tempogram is doing FFT
  // on the ONSET time series, not the audio. So the "samplerate" for that
  // time series is samplerate/hop_s, not samplerate!
  
  smpl_t freq_hz = bpm / 60.0;
  
  // Onset sampling rate in the onset time series
  smpl_t onset_sr = o->samplerate / (smpl_t)o->hop_s;
  
  // FFT bin for this frequency
  smpl_t bin_f = freq_hz * o->win_s / onset_sr;
  uint_t bin = (uint_t) (bin_f + 0.5);  // round to nearest
  
  // Clamp to valid FFT bin range
  if (bin >= o->win_s / 2 + 1) {
    bin = o->win_s / 2;
  }
  
  return bin;
}

/** convert FFT bin index to BPM */
static smpl_t
aubio_tempogram_bin_to_bpm (const aubio_tempogram_t * o, uint_t bin)
{
  AUBIO_ASSERT_NOT_NULL (o);
  AUBIO_ASSERT_BOUNDS (bin, o->win_s / 2 + 1);
  
  // Onset sampling rate
  smpl_t onset_sr = o->samplerate / (smpl_t)o->hop_s;
  
  // Frequency for this bin
  smpl_t freq_hz = bin * onset_sr / o->win_s;
  
  // Convert to BPM
  smpl_t bpm = freq_hz * 60.0;
  
  return bpm;
}

/** create Hann window */
static void
aubio_tempogram_init_window (fvec_t * window)
{
  AUBIO_ASSERT_NOT_NULL (window);
  
  uint_t i;
  uint_t length = window->length;
  
  for (i = 0; i < length; i++) {
    AUBIO_ASSERT_BOUNDS (i, length);
    smpl_t val = 0.5 - 0.5 * COS (TWO_PI * i / (length - 1.0));
    window->data[i] = val;
  }
}

aubio_tempogram_t *
new_aubio_tempogram (uint_t win_s, uint_t hop_s, uint_t samplerate)
{
  aubio_tempogram_t *o = AUBIO_NEW (aubio_tempogram_t);
  
  if (!o) {
    return NULL;
  }
  
  // Validate parameters
  if (win_s < 64 || win_s > 4096) {
    AUBIO_ERR ("tempogram: win_s should be between 64 and 4096\n");
    goto beach;
  }
  
  if (hop_s < 1 || hop_s > win_s) {
    AUBIO_ERR ("tempogram: hop_s should be between 1 and win_s\n");
    goto beach;
  }
  
  if (samplerate < 8000 || samplerate > 192000) {
    AUBIO_ERR ("tempogram: samplerate should be between 8000 and 192000\n");
    goto beach;
  }
  
  o->win_s = win_s;
  o->hop_s = hop_s;
  o->samplerate = samplerate;
  
  // Create FFT with size equal to window size
  o->fft = new_aubio_fft (win_s);
  if (!o->fft) {
    goto beach;
  }
  
  // Create window function
  o->window = new_fvec (win_s);
  if (!o->window) {
    goto beach;
  }
  aubio_tempogram_init_window (o->window);
  
  // Create FFT input buffer
  o->fft_input = new_fvec (win_s);
  if (!o->fft_input) {
    goto beach;
  }
  
  // Create FFT output buffer
  o->fftout = new_cvec (win_s);
  if (!o->fftout) {
    goto beach;
  }
  
  // Create magnitude spectrum buffer
  o->magnitude = new_fvec (win_s / 2 + 1);
  if (!o->magnitude) {
    goto beach;
  }
  
  // Circular buffer for onset frames
  // Buffer size should be at least win_s to fill the entire FFT window
  o->buffer_size = win_s;
  o->onset_buffer = new_fmat (o->buffer_size, 1);  // Each "row" is a time point
  if (!o->onset_buffer) {
    goto beach;
  }
  o->buffer_write_pos = 0;
  
  // Default tempo range
  o->tempo_min_bpm = 30.0;
  o->tempo_max_bpm = 300.0;
  
  // Convert to bin indices
  o->tempo_min_idx = aubio_tempogram_bpm_to_bin (o, o->tempo_min_bpm);
  o->tempo_max_idx = aubio_tempogram_bpm_to_bin (o, o->tempo_max_bpm);
  
  // Create BPM bins lookup table
  o->bpm_bins = new_fvec (win_s / 2 + 1);
  if (!o->bpm_bins) {
    goto beach;
  }
  
  uint_t i;
  for (i = 0; i < o->bpm_bins->length; i++) {
    AUBIO_ASSERT_BOUNDS (i, o->bpm_bins->length);
    o->bpm_bins->data[i] = aubio_tempogram_bin_to_bpm (o, i);
  }
  
  o->current_tempo = 120.0;
  o->confidence = 0.0;
  
  // Default PLP smoothing: 5-frame median filter (good for gradual changes)
  o->plp_smoothing_window = 5;
  
  return o;

beach:
  del_aubio_tempogram (o);
  return NULL;
}

void
del_aubio_tempogram (aubio_tempogram_t * o)
{
  if (!o) {
    return;
  }
  
  if (o->fft) {
    del_aubio_fft (o->fft);
  }
  if (o->window) {
    del_fvec (o->window);
  }
  if (o->fft_input) {
    del_fvec (o->fft_input);
  }
  if (o->fftout) {
    del_cvec (o->fftout);
  }
  if (o->magnitude) {
    del_fvec (o->magnitude);
  }
  if (o->onset_buffer) {
    del_fmat (o->onset_buffer);
  }
  if (o->bpm_bins) {
    del_fvec (o->bpm_bins);
  }
  
  AUBIO_FREE (o);
}

void
aubio_tempogram_do (aubio_tempogram_t * o, const fvec_t * onset,
    fmat_t * tempogram)
{
  AUBIO_ASSERT_NOT_NULL (o);
  AUBIO_ASSERT_NOT_NULL (onset);
  AUBIO_ASSERT_NOT_NULL (tempogram);
  AUBIO_ASSERT_LENGTH (onset, 1);  // Single onset value per call
  
  uint_t i;
  
  // Validate tempogram dimensions
  uint_t fft_bins = o->win_s / 2 + 1;
  if (tempogram->height != fft_bins) {
    AUBIO_ERR("tempogram: tempogram height %u != expected %u\n", 
              tempogram->height, fft_bins);
    return;
  }
  if (tempogram->length < 1) {
    AUBIO_ERR("tempogram: tempogram length must be at least 1\n");
    return;
  }
  
  // Add onset value to circular buffer
  AUBIO_ASSERT_BOUNDS (o->buffer_write_pos, o->buffer_size);
  o->onset_buffer->data[o->buffer_write_pos][0] = onset->data[0];
  o->buffer_write_pos = (o->buffer_write_pos + 1) % o->buffer_size;
  
  // Fill FFT input with onset buffer (time-reversed for causality)
  fvec_zeros (o->fft_input);
  
  uint_t read_pos = o->buffer_write_pos;
  uint_t samples_available = MIN (o->win_s, o->buffer_size);
  
  for (i = 0; i < samples_available; i++) {
    AUBIO_ASSERT_BOUNDS (i, o->win_s);
    
    // Read from circular buffer in reverse time order
    if (read_pos == 0) {
      read_pos = o->buffer_size - 1;
    } else {
      read_pos--;
    }
    
    AUBIO_ASSERT_BOUNDS (read_pos, o->buffer_size);
    smpl_t onset_val = o->onset_buffer->data[read_pos][0];
    
    // Apply window and store
    AUBIO_ASSERT_BOUNDS (i, o->window->length);
    o->fft_input->data[i] = onset_val * o->window->data[i];
  }
  
  // Compute FFT
  aubio_fft_do (o->fft, o->fft_input, o->fftout);
  
  // Compute magnitude spectrum (power spectrum)
  // FFT output is already in magnitude/phase format, so we just square the magnitude
  for (i = 0; i < fft_bins; i++) {
    AUBIO_ASSERT_BOUNDS (i, fft_bins);
    AUBIO_ASSERT_BOUNDS (i, o->fftout->length);
    
    // Power spectrum = magnitude squared
    smpl_t magnitude = o->fftout->norm[i];
    o->magnitude->data[i] = magnitude * magnitude;
  }
  
  // Copy magnitude spectrum to tempogram output
  // tempogram dimensions: (tempo_bins, time_frames)
  // For single frame output, we just copy the magnitude
  for (i = 0; i < fft_bins; i++) {
    AUBIO_ASSERT_BOUNDS (i, tempogram->height);
    AUBIO_ASSERT_BOUNDS (i, o->magnitude->length);
    tempogram->data[i][0] = o->magnitude->data[i];
  }
}

smpl_t
aubio_tempogram_get_tempo (aubio_tempogram_t * o, const fmat_t * tempogram)
{
  AUBIO_ASSERT_NOT_NULL (o);
  AUBIO_ASSERT_NOT_NULL (tempogram);
  
  uint_t i;
  uint_t max_idx = o->tempo_min_idx;
  smpl_t max_val = 0.0;
  smpl_t total_energy = 0.0;
  
  // Find peak in tempo range
  for (i = o->tempo_min_idx; i <= o->tempo_max_idx; i++) {
    AUBIO_ASSERT_BOUNDS (i, tempogram->height);
    
    smpl_t val = tempogram->data[i][0];
    total_energy += val;
    
    if (val > max_val) {
      max_val = val;
      max_idx = i;
    }
  }
  
  // Compute confidence (peak strength relative to mean)
  uint_t num_bins = o->tempo_max_idx - o->tempo_min_idx + 1;
  smpl_t mean_energy = (num_bins > 0) ? (total_energy / num_bins) : 0.0;
  
  if (mean_energy > 1e-10) {
    o->confidence = max_val / mean_energy;
  } else {
    o->confidence = 0.0;
  }
  
  // Get tempo from peak bin
  AUBIO_ASSERT_BOUNDS (max_idx, o->bpm_bins->length);
  o->current_tempo = o->bpm_bins->data[max_idx];
  
  return o->current_tempo;
}

smpl_t
aubio_tempogram_get_confidence (const aubio_tempogram_t * o)
{
  AUBIO_ASSERT_NOT_NULL (o);
  return o->confidence;
}

uint_t
aubio_tempogram_set_tempo_min (aubio_tempogram_t * o, smpl_t tempo_min)
{
  AUBIO_ASSERT_NOT_NULL (o);
  
  if (tempo_min < 20.0 || tempo_min > 200.0) {
    AUBIO_ERR ("tempogram: tempo_min should be between 20 and 200 BPM\n");
    return AUBIO_FAIL;
  }
  
  if (tempo_min >= o->tempo_max_bpm) {
    AUBIO_ERR ("tempogram: tempo_min must be less than tempo_max\n");
    return AUBIO_FAIL;
  }
  
  o->tempo_min_bpm = tempo_min;
  o->tempo_min_idx = aubio_tempogram_bpm_to_bin (o, tempo_min);
  
  return AUBIO_OK;
}

uint_t
aubio_tempogram_set_tempo_max (aubio_tempogram_t * o, smpl_t tempo_max)
{
  AUBIO_ASSERT_NOT_NULL (o);
  
  if (tempo_max < 60.0 || tempo_max > 400.0) {
    AUBIO_ERR ("tempogram: tempo_max should be between 60 and 400 BPM\n");
    return AUBIO_FAIL;
  }
  
  if (tempo_max <= o->tempo_min_bpm) {
    AUBIO_ERR ("tempogram: tempo_max must be greater than tempo_min\n");
    return AUBIO_FAIL;
  }
  
  o->tempo_max_bpm = tempo_max;
  o->tempo_max_idx = aubio_tempogram_bpm_to_bin (o, tempo_max);
  
  return AUBIO_OK;
}

smpl_t
aubio_tempogram_get_plp_at_time (aubio_tempogram_t * o,
    const fmat_t * tempogram, uint_t time_idx)
{
  AUBIO_ASSERT_NOT_NULL (o);
  AUBIO_ASSERT_NOT_NULL (tempogram);
  AUBIO_ASSERT_BOUNDS (time_idx, tempogram->length);
  
  uint_t i;
  uint_t max_idx = o->tempo_min_idx;
  smpl_t max_val = 0.0;
  
  // Find peak at this time frame
  for (i = o->tempo_min_idx; i <= o->tempo_max_idx; i++) {
    AUBIO_ASSERT_BOUNDS (i, tempogram->height);
    AUBIO_ASSERT_BOUNDS (time_idx, tempogram->length);
    
    smpl_t val = tempogram->data[i][time_idx];
    if (val > max_val) {
      max_val = val;
      max_idx = i;
    }
  }
  
  AUBIO_ASSERT_BOUNDS (max_idx, o->bpm_bins->length);
  return o->bpm_bins->data[max_idx];
}

void
aubio_tempogram_get_plp_curve (aubio_tempogram_t * o,
    const fmat_t * tempogram, fvec_t * plp_curve)
{
  AUBIO_ASSERT_NOT_NULL (o);
  AUBIO_ASSERT_NOT_NULL (tempogram);
  AUBIO_ASSERT_NOT_NULL (plp_curve);
  
  // Check lengths match
  if (plp_curve->length != tempogram->length) {
    AUBIO_ERR("tempogram: plp_curve length %u != tempogram length %u\n",
              plp_curve->length, tempogram->length);
    return;
  }
  
  uint_t t;
  
  // Extract dominant tempo at each time frame
  for (t = 0; t < tempogram->length; t++) {
    AUBIO_ASSERT_BOUNDS (t, plp_curve->length);
    plp_curve->data[t] = aubio_tempogram_get_plp_at_time (o, tempogram, t);
  }
  
  // Apply temporal smoothing if enabled (median filter)
  if (o->plp_smoothing_window > 1) {
    uint_t window_size = o->plp_smoothing_window;
    uint_t half_window = window_size / 2;
    
    // Create temporary buffer for smoothed curve
    fvec_t *smoothed = new_fvec (plp_curve->length);
    if (!smoothed) {
      AUBIO_ERR("tempogram: failed to allocate smoothing buffer\n");
      return;
    }
    
    // Create window buffer for median filtering
    fvec_t *window_buffer = new_fvec (window_size);
    if (!window_buffer) {
      del_fvec (smoothed);
      AUBIO_ERR("tempogram: failed to allocate window buffer\n");
      return;
    }
    
    // Apply median filter at each time point
    for (t = 0; t < plp_curve->length; t++) {
      AUBIO_ASSERT_BOUNDS (t, plp_curve->length);
      
      uint_t window_count = 0;
      uint_t i;
      
      // Collect values in the window around time t
      for (i = 0; i < window_size; i++) {
        // Calculate position with bounds checking
        sint_t pos = (sint_t)t - (sint_t)half_window + (sint_t)i;
        
        if (pos >= 0 && pos < (sint_t)plp_curve->length) {
          AUBIO_ASSERT_BOUNDS (window_count, window_size);
          AUBIO_ASSERT_BOUNDS ((uint_t)pos, plp_curve->length);
          window_buffer->data[window_count++] = plp_curve->data[pos];
        }
      }
      
      // Compute median of collected values
      if (window_count > 0) {
        // Temporarily resize window buffer to actual count
        uint_t orig_length = window_buffer->length;
        window_buffer->length = window_count;
        
        smpl_t median_val = fvec_median (window_buffer);
        
        // Restore original length
        window_buffer->length = orig_length;
        
        AUBIO_ASSERT_BOUNDS (t, smoothed->length);
        smoothed->data[t] = median_val;
      } else {
        // Fallback: use original value
        AUBIO_ASSERT_BOUNDS (t, smoothed->length);
        AUBIO_ASSERT_BOUNDS (t, plp_curve->length);
        smoothed->data[t] = plp_curve->data[t];
      }
    }
    
    // Copy smoothed values back to output
    for (t = 0; t < plp_curve->length; t++) {
      AUBIO_ASSERT_BOUNDS (t, plp_curve->length);
      AUBIO_ASSERT_BOUNDS (t, smoothed->length);
      plp_curve->data[t] = smoothed->data[t];
    }
    
    // Cleanup
    del_fvec (window_buffer);
    del_fvec (smoothed);
  }
}

uint_t
aubio_tempogram_set_plp_smoothing_window (aubio_tempogram_t * o, uint_t window)
{
  AUBIO_ASSERT_NOT_NULL (o);
  
  // Validate window size (1 means no smoothing, up to 31 for large smoothing)
  if (window > 31) {
    AUBIO_ERR("tempogram: plp_smoothing_window must be <= 31\n");
    return AUBIO_FAIL;
  }
  
  // Window should be odd for symmetric median filter
  if (window > 1 && window % 2 == 0) {
    AUBIO_WRN("tempogram: plp_smoothing_window should be odd, adjusting %u to %u\n", 
              window, window + 1);
    window++;
  }
  
  o->plp_smoothing_window = window;
  return AUBIO_OK;
}

uint_t
aubio_tempogram_get_plp_smoothing_window (const aubio_tempogram_t * o)
{
  AUBIO_ASSERT_NOT_NULL (o);
  return o->plp_smoothing_window;
}
