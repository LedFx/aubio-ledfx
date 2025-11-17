/*
  Copyright (C) 2005-2009 Matthew Davies and Paul Brossier <piem@aubio.org>

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
#include "mathutils.h"
#include "tempo/beattracking.h"
#include "tempo/tempogram.h"

/** define to 1 to print out tracking difficulties */
#define AUBIO_BEAT_WARNINGS 0

uint_t fvec_gettimesig (fvec_t * acf, uint_t acflen, uint_t gp);
void aubio_beattracking_checkstate (aubio_beattracking_t * bt);
static void aubio_beattracking_update_confidence (aubio_beattracking_t * bt);

struct _aubio_beattracking_t
{
  uint_t hop_size;       /** length of one tempo detection function sample, in audio samples */
  uint_t samplerate;     /** samplerate of the original signal */
  fvec_t *rwv;           /** rayleigh weighting for beat period in general model */
  fvec_t *dfwv;          /** exponential weighting for beat alignment in general model */
  fvec_t *gwv;           /** gaussian weighting for beat period in context dependant model */
  fvec_t *phwv;          /** gaussian weighting for beat alignment in context dependant model */
  fvec_t *dfrev;         /** reversed onset detection function */
  fvec_t *acf;           /** vector for autocorrelation function (of current detection function frame) */
  fvec_t *acfout;        /** store result of passing acf through s.i.c.f.b. */
  fvec_t *phout;
  uint_t timesig;        /** time signature of input, set to zero until context dependent model activated */
  uint_t step;
  uint_t rayparam;       /** Rayleigh parameter */
  smpl_t lastbeat;
  sint_t counter;
  uint_t flagstep;
  smpl_t g_var;
  smpl_t gp;
  smpl_t bp;
  smpl_t rp;
  smpl_t rp1;
  smpl_t rp2;
  smpl_t onset_std;      /** standard deviation of onset strength for normalization */
  smpl_t tempo_prior_mean; /** mean of tempo prior distribution (BPM) */
  smpl_t tempo_prior_std;  /** standard deviation of tempo prior distribution (BPM) */
  smpl_t prev_tempo;     /** previous tempo estimate for smoothing */
  smpl_t tempo_confidence; /** confidence of current tempo estimate */
  uint_t adaptive_winlen; /** adaptive window length (0 = use default) */
  smpl_t stability_count; /** number of frames with stable tempo */
  uint_t enable_multi_octave; /** enable multi-octave tempo detection for slow tempos */
  smpl_t tempo_change_threshold; /** threshold for detecting tempo changes */
  
  /* Phase 3: Dynamic tempo tracking */
  uint_t enable_dynamic_tempo; /** enable frame-by-frame tempo estimation */
  fvec_t *tempo_history;  /** circular buffer of recent tempo estimates */
  uint_t tempo_history_pos; /** current position in tempo history buffer */
  smpl_t instantaneous_tempo; /** current frame tempo estimate (before smoothing) */
  
  /* Phase 3: Advanced autocorrelation and tempogram */
  uint_t use_fft_autocorr; /** use FFT-based autocorrelation (faster for large windows) */
  aubio_tempogram_t *tempogram_obj;  /** Fourier tempogram analyzer */
  uint_t use_tempogram;   /** enable tempogram-based tempo detection */
  fmat_t *tempogram_out;  /** Tempogram output matrix */
  
  /* Phase 3A: Onset enhancement for tempogram */
  fvec_t *onset_history;  /** circular buffer of recent onset values for median filtering */
  uint_t onset_history_pos; /** current position in onset history buffer */
  uint_t onset_enhancement; /** enable onset preprocessing (median filter, thresholding) */
};

aubio_beattracking_t *
new_aubio_beattracking (uint_t winlen, uint_t hop_size, uint_t samplerate)
{

  aubio_beattracking_t *p = AUBIO_NEW (aubio_beattracking_t);
  
  if (!p) {
    return NULL;
  }

  uint_t i = 0;
  /* default value for rayleigh weighting - sets preferred tempo to 120bpm
   * Widen the Rayleigh distribution (1.4x) to better support extreme tempos (60-240 BPM) */
  smpl_t rayparam = 1.4 * 60. * samplerate / 120. / hop_size;
  smpl_t dfwvnorm = EXP ((LOG (2.0) / rayparam) * (winlen + 2));
  /* length over which beat period is found [128] */
  uint_t laglen = winlen / 4;
  /* step increment - both in detection function samples -i.e. 11.6ms or
   * 1 onset frame - REDUCED from winlen/4 to winlen/8 for 2x faster response */
  uint_t step = winlen / 8;     /* 0.75 seconds instead of 1.5 seconds */

  p->hop_size = hop_size;
  p->samplerate = samplerate;
  p->lastbeat = 0;
  p->counter = 0;
  p->flagstep = 0;
  p->g_var = 3.901;             // constthresh empirically derived!
  p->rp = 1;
  p->gp = 0;

  p->rayparam = rayparam;
  p->step = step;
  p->rwv = new_fvec (laglen);
  p->gwv = new_fvec (laglen);
  p->dfwv = new_fvec (winlen);
  p->dfrev = new_fvec (winlen);
  p->acf = new_fvec (winlen);
  p->acfout = new_fvec (laglen);
  p->phwv = new_fvec (2 * laglen);
  p->phout = new_fvec (winlen);

  p->timesig = 0;
  
  /* Initialize onset normalization and tempo prior parameters */
  p->onset_std = 0.0;
  p->tempo_prior_mean = 120.0;  /* Default to 120 BPM */
  p->tempo_prior_std = 1.0;     /* Default std deviation */
  p->prev_tempo = 0.0;          /* No previous tempo yet */
  p->tempo_confidence = 0.0;    /* No confidence yet */
  p->adaptive_winlen = 0;       /* 0 = use default winlen */
  p->stability_count = 0.0;     /* No stable tempo yet */
  p->enable_multi_octave = 1;   /* Enable by default for better slow tempo detection */
  p->tempo_change_threshold = 0.15;  /* 15% change triggers re-analysis */
  
  /* Phase 3: Dynamic tempo tracking */
  p->enable_dynamic_tempo = 0;  /* Disabled by default for backward compatibility */
  p->tempo_history = new_fvec(16);  /* Keep last 16 tempo estimates (~24 seconds at default hop) */
  p->tempo_history_pos = 0;
  p->instantaneous_tempo = 0.0;
  
  /* Phase 3: FFT-based autocorrelation */
  p->use_fft_autocorr = (winlen >= 512) ? 1 : 0;  /* Use FFT for windows >= 512 samples */
  
  /* Phase 3: Tempogram-based tempo detection */
  p->use_tempogram = 0;  /* Disabled by default, opt-in feature */
  p->tempogram_obj = NULL;  /* Lazy initialization when enabled */
  p->tempogram_out = NULL;
  
  /* Phase 3A: Onset enhancement for tempogram */
  p->onset_history = new_fvec(7);  /* 7-sample median filter window (increased from 5) */
  p->onset_history_pos = 0;
  p->onset_enhancement = 1;  /* Enabled by default for better real-world performance */

  /* exponential weighting, dfwv = 0.5 when i =  43 */
  for (i = 0; i < winlen; i++) {
    p->dfwv->data[i] = (EXP ((LOG (2.0) / rayparam) * (i + 1)))
        / dfwvnorm;
  }

  for (i = 0; i < (laglen); i++) {
    p->rwv->data[i] = ((smpl_t) (i + 1.) / SQR ((smpl_t) rayparam)) *
        EXP ((-SQR ((smpl_t) (i + 1.)) / (2. * SQR ((smpl_t) rayparam))));
  }

  return p;

}

void
del_aubio_beattracking (aubio_beattracking_t * p)
{
  del_fvec (p->rwv);
  del_fvec (p->gwv);
  del_fvec (p->dfwv);
  del_fvec (p->dfrev);
  del_fvec (p->acf);
  del_fvec (p->acfout);
  del_fvec (p->phwv);
  del_fvec (p->phout);
  if (p->tempo_history) {
    del_fvec (p->tempo_history);
  }
  if (p->onset_history) {
    del_fvec (p->onset_history);
  }
  if (p->tempogram_obj) {
    del_aubio_tempogram (p->tempogram_obj);
  }
  if (p->tempogram_out) {
    del_fmat (p->tempogram_out);
  }
  AUBIO_FREE (p);
}

/* Normalize onset detection function by standard deviation (librosa-inspired) */
static void
aubio_beattracking_normalize_dfframe (const fvec_t * dfframe, fvec_t * normalized)
{
  AUBIO_ASSERT_NOT_NULL(dfframe);
  AUBIO_ASSERT_NOT_NULL(normalized);
  AUBIO_ASSERT_LENGTH(normalized, dfframe->length);
  
  smpl_t std_val = fvec_stddev((fvec_t *)dfframe);
  uint_t i;
  
  /* Avoid division by zero - if std is very small, just copy */
  if (std_val > 1e-10) {
    for (i = 0; i < dfframe->length; i++) {
      AUBIO_ASSERT_BOUNDS(i, dfframe->length);
      AUBIO_ASSERT_BOUNDS(i, normalized->length);
      normalized->data[i] = dfframe->data[i] / std_val;
    }
  } else {
    fvec_copy(dfframe, normalized);
  }
}


void
aubio_beattracking_do (aubio_beattracking_t * bt, const fvec_t * dfframe,
    fvec_t * output)
{

  uint_t i, k;
  uint_t step = bt->step;
  uint_t laglen = bt->rwv->length;
  uint_t winlen = bt->dfwv->length;
  uint_t maxindex = 0;
  //number of harmonics in shift invariant comb filterbank
  uint_t numelem = 4;

  smpl_t phase;                 // beat alignment (step - lastbeat)
  smpl_t beat;                  // beat position
  smpl_t bp;                    // beat period
  uint_t a, b;                  // used to build shift invariant comb filterbank
  uint_t kmax;                  // number of elements used to find beat phase
  
  fvec_t *normalized_df = new_fvec(dfframe->length);
  if (!normalized_df) {
    fvec_zeros(output);
    return;
  }

  /* Normalize onset detection function by standard deviation for robustness */
  aubio_beattracking_normalize_dfframe(dfframe, normalized_df);

  /* copy normalized dfframe, apply detection function weighting, and revert */
  fvec_copy (normalized_df, bt->dfrev);
  fvec_weight (bt->dfrev, bt->dfwv);
  fvec_rev (bt->dfrev);

  /* compute autocorrelation function on normalized data */
  if (bt->use_fft_autocorr) {
    aubio_autocorr_fft (normalized_df, bt->acf);
  } else {
    aubio_autocorr (normalized_df, bt->acf);
  }
  
  del_fvec(normalized_df);

  /* if timesig is unknown, use metrically unbiased version of filterbank */
  if (!bt->timesig) {
    numelem = 4;
  } else {
    numelem = bt->timesig;
  }

  /* first and last output values are left intentionally as zero */
  fvec_zeros (bt->acfout);

  /* compute shift invariant comb filterbank */
  for (i = 1; i < laglen - 1; i++) {
    for (a = 1; a <= numelem; a++) {
      for (b = 1; b < 2 * a; b++) {
        bt->acfout->data[i] += bt->acf->data[i * a + b - 1]
            * 1. / (2. * a - 1.);
      }
    }
  }
  
  /* Phase 3: Enhanced multi-octave detection
   * Check peaks at half and double the detected period to catch slow/fast tempos */
  if (bt->enable_multi_octave) {
    fvec_t *acfout_enhanced = new_fvec(bt->acfout->length);
    if (acfout_enhanced) {
      fvec_copy(bt->acfout, acfout_enhanced);
      
      /* Boost autocorrelation at half-period (for slow tempos like 80 BPM)
       * Increased boost factor from 0.5 to 0.75 for better slow tempo detection */
      for (i = 1; i < laglen / 2 - 1; i++) {
        uint_t double_idx = i * 2;
        if (double_idx < laglen - 1) {
          AUBIO_ASSERT_BOUNDS(i, acfout_enhanced->length);
          AUBIO_ASSERT_BOUNDS(double_idx, bt->acfout->length);
          acfout_enhanced->data[i] += 0.75 * bt->acfout->data[double_idx];
        }
      }
      
      /* Boost autocorrelation at double-period (for fast tempos like 160 BPM)
       * Increased boost factor from 0.5 to 0.75 for better fast tempo detection */
      for (i = laglen / 2; i < laglen - 1; i++) {
        uint_t half_idx = i / 2;
        if (half_idx > 0) {
          AUBIO_ASSERT_BOUNDS(i, acfout_enhanced->length);
          AUBIO_ASSERT_BOUNDS(half_idx, bt->acfout->length);
          acfout_enhanced->data[i] += 0.75 * bt->acfout->data[half_idx];
        }
      }
      
      /* Extra boost for very fast tempos (140-200 BPM range: frames 43-74)
       * This helps detect 160 BPM which is at ~65 frames */
      for (i = 43; i < 75 && i < laglen - 1; i++) {
        AUBIO_ASSERT_BOUNDS(i, acfout_enhanced->length);
        /* Additional 20% boost for very fast tempo range */
        acfout_enhanced->data[i] *= 1.2;
      }
      
      fvec_copy(acfout_enhanced, bt->acfout);
      del_fvec(acfout_enhanced);
    }
  }
  
  /* apply Rayleigh weight */
  fvec_weight (bt->acfout, bt->rwv);

  /* find non-zero Rayleigh period */
  maxindex = fvec_max_elem (bt->acfout);
  if (maxindex > 0 && maxindex < bt->acfout->length - 1) {
    bt->rp = fvec_quadratic_peak_pos (bt->acfout, maxindex);
  } else {
    bt->rp = bt->rayparam;
  }

  /* activate biased filterbank */
  aubio_beattracking_checkstate (bt);
#if 0                           // debug metronome mode
  bt->bp = 36.9142;
#endif
  bp = bt->bp;
  /* end of biased filterbank */

  if (bp == 0) {
    fvec_zeros(output);
    return;
  }

  /* deliberate integer operation, could be set to 3 max eventually */
  kmax = FLOOR (winlen / bp);

  /* initialize output */
  fvec_zeros (bt->phout);
  for (i = 0; i < bp; i++) {
    for (k = 0; k < kmax; k++) {
      uint_t idx = i + (uint_t) ROUND (bp * k);
      if (idx < bt->dfrev->length)
        bt->phout->data[i] += bt->dfrev->data[idx];
#if AUBIO_BEAT_WARNINGS
      else
        AUBIO_WRN ("[tempo] out of bounds index %d", idx);
#endif
    }
  }
  fvec_weight (bt->phout, bt->phwv);

  /* find Rayleigh period */
  maxindex = fvec_max_elem (bt->phout);
  if (maxindex >= winlen - 1) {
#if AUBIO_BEAT_WARNINGS
    AUBIO_WRN ("no idea what this groove's phase is\n");
#endif /* AUBIO_BEAT_WARNINGS */
    phase = step - bt->lastbeat;
  } else {
    phase = fvec_quadratic_peak_pos (bt->phout, maxindex);
  }
  /* take back one frame delay */
  phase += 1.;
#if 0                           // debug metronome mode
  phase = step - bt->lastbeat;
#endif

  /* reset output */
  fvec_zeros (output);

  i = 1;
  beat = bp - phase;

  // AUBIO_DBG ("bp: %f, phase: %f, lastbeat: %f, step: %d, winlen: %d\n",
  //    bp, phase, bt->lastbeat, step, winlen);

  /* the next beat will be earlier than 60% of the tempo period
    skip this one */
  if ( ( step - bt->lastbeat - phase ) < -0.40 * bp ) {
#if AUBIO_BEAT_WARNINGS
    AUBIO_WRN ("back off-beat error, skipping this beat\n");
#endif /* AUBIO_BEAT_WARNINGS */
    beat += bp;
  }

  /* start counting the beats */
  while (beat + bp < 0) {
    beat += bp;
  }

  if (beat >= 0) {
    //AUBIO_DBG ("beat: %d, %f, %f\n", i, bp, beat);
    output->data[i] = beat;
    i++;
  }

  while (beat + bp <= step) {
    beat += bp;
    //AUBIO_DBG ("beat: %d, %f, %f\n", i, bp, beat);
    output->data[i] = beat;
    i++;
  }

  bt->lastbeat = beat;
  /* store the number of beats in this frame as the first element */
  output->data[0] = i;
}

uint_t
fvec_gettimesig (fvec_t * acf, uint_t acflen, uint_t gp)
{
  sint_t k = 0;
  smpl_t three_energy = 0., four_energy = 0.;
  if (gp < 2) return 4;
  if (acflen > 6 * gp + 2) {
    for (k = -2; k < 2; k++) {
      three_energy += acf->data[3 * gp + k];
      four_energy += acf->data[4 * gp + k];
    }
  } else {
    /*Expanded to be more accurate in time sig estimation */
    for (k = -2; k < 2; k++) {
      three_energy += acf->data[3 * gp + k] + acf->data[6 * gp + k];
      four_energy += acf->data[4 * gp + k] + acf->data[2 * gp + k];
    }
  }
  return (three_energy > four_energy) ? 3 : 4;
}

void
aubio_beattracking_checkstate (aubio_beattracking_t * bt)
{
  uint_t i, j, a, b;
  uint_t flagconst = 0;
  sint_t counter = bt->counter;
  uint_t flagstep = bt->flagstep;
  smpl_t gp = bt->gp;
  smpl_t bp = bt->bp;
  smpl_t rp = bt->rp;
  smpl_t rp1 = bt->rp1;
  smpl_t rp2 = bt->rp2;
  uint_t laglen = bt->rwv->length;
  uint_t acflen = bt->acf->length;
  uint_t step = bt->step;
  fvec_t *acf = bt->acf;
  fvec_t *acfout = bt->acfout;

  if (gp) {
    // compute shift invariant comb filterbank
    fvec_zeros (acfout);
    for (i = 1; i < laglen - 1; i++) {
      for (a = 1; a <= bt->timesig; a++) {
        for (b = 1; b < 2 * a; b++) {
          acfout->data[i] += acf->data[i * a + b - 1];
        }
      }
    }
    // since gp is set, gwv has been computed in previous checkstate
    fvec_weight (acfout, bt->gwv);
    gp = fvec_quadratic_peak_pos (acfout, fvec_max_elem (acfout));
  } else {
    //still only using general model
    gp = 0;
  }
  
  /* Update confidence after computing gp */
  aubio_beattracking_update_confidence(bt);

  //now look for step change - i.e. a difference between gp and rp that
  // is greater than 2*constthresh - always true in first case, since gp = 0
  if (counter == 0) {
    if (ABS (gp - rp) > 2. * bt->g_var) {
      flagstep = 1;             // have observed  step change.
      counter = 3;              // setup 3 frame counter
    } else {
      flagstep = 0;
    }
  }
  //i.e. 3rd frame after flagstep initially set
  if (counter == 1 && flagstep == 1) {
    //check for consistency between previous beatperiod values
    if (ABS (2 * rp - rp1 - rp2) < bt->g_var) {
      //if true, can activate context dependent model
      flagconst = 1;
      counter = 0;              // reset counter and flagstep
    } else {
      //if not consistent, then don't flag consistency!
      flagconst = 0;
      counter = 2;              // let it look next time
    }
  } else if (counter > 0) {
    //if counter doesn't = 1,
    counter = counter - 1;
  }

  rp2 = rp1;
  rp1 = rp;

  if (flagconst) {
    /* first run of new hypothesis */
    gp = rp;
    bt->timesig = fvec_gettimesig (acf, acflen, gp);
    for (j = 0; j < laglen; j++)
      bt->gwv->data[j] =
          EXP (-.5 * SQR ((smpl_t) (j + 1. - gp)) / SQR (bt->g_var));
    flagconst = 0;
    bp = gp;
    /* flat phase weighting */
    fvec_ones (bt->phwv);
  } else if (bt->timesig) {
    /* context dependant model */
    bp = gp;
    /* gaussian phase weighting */
    if (step > bt->lastbeat) {
      for (j = 0; j < 2 * laglen; j++) {
        bt->phwv->data[j] =
            EXP (-.5 * SQR ((smpl_t) (1. + j - step +
                    bt->lastbeat)) / (bp / 8.));
      }
    } else {
      //AUBIO_DBG("NOT using phase weighting as step is %d and lastbeat %d \n",
      //                step,bt->lastbeat);
      fvec_ones (bt->phwv);
    }
  } else {
    /* initial state */
    bp = rp;
    /* flat phase weighting */
    fvec_ones (bt->phwv);
  }

  /* do some further checks on the final bp value */

  /* if tempo is > 206 bpm, half it */
  while (0 < bp && bp < 25) {
#if AUBIO_BEAT_WARNINGS
    AUBIO_WRN ("doubling from %f (%f bpm) to %f (%f bpm)\n",
        bp, 60.*44100./512./bp, bp/2., 60.*44100./512./bp/2. );
    //AUBIO_DBG("warning, halving the tempo from %f\n", 60.*samplerate/hopsize/bp);
#endif /* AUBIO_BEAT_WARNINGS */
    bp = bp * 2;
  }

  //AUBIO_DBG("tempo:\t%3.5f bpm | ", 5168./bp);

  /* smoothing */
  //bp = (uint_t) (0.8 * (smpl_t)bp + 0.2 * (smpl_t)bp2);
  //AUBIO_DBG("tempo:\t%3.5f bpm smoothed | bp2 %d | bp %d | ", 5168./bp, bp2, bp);
  //bp2 = bp;
  //AUBIO_DBG("time signature: %d \n", bt->timesig);
  bt->counter = counter;
  bt->flagstep = flagstep;
  bt->gp = gp;
  bt->bp = bp;
  bt->rp1 = rp1;
  bt->rp2 = rp2;
  
  /* Update previous tempo for smoothing */
  if (bp != 0) {
    smpl_t current_bpm = 60. * bt->samplerate / (bt->hop_size * bp);
    
    /* Phase 3: Dynamic tempo tracking - store instantaneous estimate */
    bt->instantaneous_tempo = current_bpm;
    
    if (bt->enable_dynamic_tempo && bt->tempo_history) {
      /* Store in circular buffer */
      AUBIO_ASSERT_BOUNDS(bt->tempo_history_pos, bt->tempo_history->length);
      bt->tempo_history->data[bt->tempo_history_pos] = current_bpm;
      bt->tempo_history_pos = (bt->tempo_history_pos + 1) % bt->tempo_history->length;
    }
    
    bt->prev_tempo = current_bpm;
  }
}

smpl_t
aubio_beattracking_get_period (const aubio_beattracking_t * bt)
{
  return bt->hop_size * bt->bp;
}

smpl_t
aubio_beattracking_get_period_s (const aubio_beattracking_t * bt)
{
  return aubio_beattracking_get_period(bt) / (smpl_t) bt->samplerate;
}

smpl_t
aubio_beattracking_get_bpm (const aubio_beattracking_t * bt)
{
  smpl_t current_bpm;
  
  /* Phase 3: Use tempogram if enabled */
  if (bt->use_tempogram && bt->tempogram_obj && bt->tempogram_out) {
    /* Get tempo from tempogram analysis */
    current_bpm = aubio_tempogram_get_tempo(bt->tempogram_obj, bt->tempogram_out);
    
    /* Tempogram already handles multi-octave analysis internally,
     * but we still apply smoothing */
  } else if (bt->bp != 0) {
    /* Standard autocorrelation-based tempo detection */
    current_bpm = 60. / aubio_beattracking_get_period_s(bt);
    
    /* Phase 3: Multi-octave tempo detection
     * Check if detected tempo might be half or double the actual tempo
     * This helps with slow tempos (< 80 BPM) and fast tempos (> 200 BPM) */
    if (bt->enable_multi_octave) {
      smpl_t half_bpm = current_bpm / 2.0;
      smpl_t double_bpm = current_bpm * 2.0;
      
      /* If we have a tempo prior, use it to disambiguate */
      if (bt->tempo_prior_mean > 0.) {
        smpl_t distance_current = fabs(current_bpm - bt->tempo_prior_mean);
        smpl_t distance_half = fabs(half_bpm - bt->tempo_prior_mean);
        smpl_t distance_double = fabs(double_bpm - bt->tempo_prior_mean);
        
        /* Choose the tempo closest to the prior */
        if (distance_half < distance_current && distance_half < distance_double) {
          current_bpm = half_bpm;
        } else if (distance_double < distance_current && distance_double < distance_half) {
          current_bpm = double_bpm;
        }
      } else {
        /* No prior: Use heuristics
         * If current BPM is very slow (< 60) or very fast (> 240), 
         * likely to be octave error */
        if (current_bpm < 60.0 && double_bpm <= 200.0) {
          current_bpm = double_bpm;
        } else if (current_bpm > 240.0 && half_bpm >= 60.0) {
          current_bpm = half_bpm;
        }
      }
    }
  } else {
    return 0.;
  }
  
  /* Apply light smoothing based on confidence
   * Higher confidence = less smoothing (more responsive)
   * Lower confidence = more smoothing (more stable) */
  if (bt->prev_tempo > 0. && bt->tempo_confidence > 0.) {
    smpl_t alpha = 0.2 + 0.3 * bt->tempo_confidence;  /* 0.2 to 0.5 */
    current_bpm = alpha * current_bpm + (1.0 - alpha) * bt->prev_tempo;
  }
  
  return current_bpm;
}

smpl_t
aubio_beattracking_get_confidence (const aubio_beattracking_t * bt)
{
  return bt->tempo_confidence;
}

/* Update and get confidence with caching */
static void
aubio_beattracking_update_confidence (aubio_beattracking_t * bt)
{
  if (bt->gp) {
    smpl_t acf_sum = fvec_sum(bt->acfout);
    if (acf_sum != 0.) {
      bt->tempo_confidence = fvec_quadratic_peak_mag (bt->acfout, bt->gp) / acf_sum;
    } else {
      bt->tempo_confidence = 0.;
    }
  } else {
    bt->tempo_confidence = 0.;
  }
}

uint_t
aubio_beattracking_set_tempo_prior_mean(aubio_beattracking_t * bt, smpl_t tempo_mean)
{
  AUBIO_ASSERT_NOT_NULL(bt);
  
  /* Check for invalid input first, before assertions */
  if (tempo_mean <= 0. || tempo_mean > 300.) {
    AUBIO_ERR("beattracking: tempo prior mean must be in range (0, 300] BPM\n");
    return AUBIO_FAIL;
  }
  
  /* Now assert on the valid range in debug builds */
  AUBIO_ASSERT_RANGE(tempo_mean, 20.0, 300.0);
  
  bt->tempo_prior_mean = tempo_mean;
  /* Update Rayleigh parameter based on new prior mean */
  bt->rayparam = 60. * bt->samplerate / tempo_mean / bt->hop_size;
  
  /* Recompute Rayleigh weighting vector */
  uint_t laglen = bt->rwv->length;
  uint_t i;
  for (i = 0; i < laglen; i++) {
    AUBIO_ASSERT_BOUNDS(i, laglen);
    bt->rwv->data[i] = ((smpl_t) (i + 1.) / SQR ((smpl_t) bt->rayparam)) *
        EXP ((-SQR ((smpl_t) (i + 1.)) / (2. * SQR ((smpl_t) bt->rayparam))));
  }
  return AUBIO_OK;
}

uint_t
aubio_beattracking_set_tempo_prior_std(aubio_beattracking_t * bt, smpl_t tempo_std)
{
  AUBIO_ASSERT_NOT_NULL(bt);
  
  /* Check for invalid input first */
  if (tempo_std <= 0. || tempo_std > 10.) {
    AUBIO_ERR("beattracking: tempo prior std must be in range (0, 10] BPM\n");
    return AUBIO_FAIL;
  }
  
  /* Now assert on valid range in debug builds */
  AUBIO_ASSERT_RANGE(tempo_std, 0.1, 10.0);
  
  bt->tempo_prior_std = tempo_std;
  /* Adjust g_var based on prior std - wider prior means more variance allowed */
  bt->g_var = 3.901 * (tempo_std / 1.0);  /* Scale relative to default std of 1.0 */
  return AUBIO_OK;
}

uint_t
aubio_beattracking_set_adaptive_winlen(aubio_beattracking_t * bt, uint_t enabled)
{
  AUBIO_ASSERT_NOT_NULL(bt);
  /* enabled is uint_t, so bounds checking would be 0 or 1, but accept any non-zero as true */
  bt->adaptive_winlen = enabled ? 1 : 0;
  return AUBIO_OK;
}

uint_t
aubio_beattracking_set_multi_octave(aubio_beattracking_t * bt, uint_t enabled)
{
  AUBIO_ASSERT_NOT_NULL(bt);
  bt->enable_multi_octave = enabled ? 1 : 0;
  return AUBIO_OK;
}

uint_t
aubio_beattracking_set_dynamic_tempo(aubio_beattracking_t * bt, uint_t enabled)
{
  AUBIO_ASSERT_NOT_NULL(bt);
  bt->enable_dynamic_tempo = enabled ? 1 : 0;
  return AUBIO_OK;
}

smpl_t
aubio_beattracking_get_instantaneous_bpm(const aubio_beattracking_t * bt)
{
  AUBIO_ASSERT_NOT_NULL(bt);
  return bt->instantaneous_tempo;
}

smpl_t
aubio_beattracking_get_tempo_variance(const aubio_beattracking_t * bt)
{
  AUBIO_ASSERT_NOT_NULL(bt);
  
  if (!bt->tempo_history || !bt->enable_dynamic_tempo) {
    return 0.0;
  }
  
  /* Calculate variance of recent tempo estimates */
  return fvec_variance(bt->tempo_history);
}

uint_t
aubio_beattracking_set_fft_autocorr(aubio_beattracking_t * bt, uint_t enabled)
{
  AUBIO_ASSERT_NOT_NULL(bt);
  bt->use_fft_autocorr = enabled ? 1 : 0;
  return AUBIO_OK;
}

uint_t
aubio_beattracking_set_use_tempogram(aubio_beattracking_t * bt, uint_t enabled)
{
  AUBIO_ASSERT_NOT_NULL(bt);
  
  bt->use_tempogram = enabled ? 1 : 0;
  
  /* Lazy initialization of tempogram when first enabled */
  if (bt->use_tempogram && !bt->tempogram_obj) {
    /* Create tempogram with power-of-2 window size for FFT */
    uint_t tempogram_win = 512;  /* Must be power of 2 */
    bt->tempogram_obj = new_aubio_tempogram(tempogram_win, bt->hop_size, bt->samplerate);
    
    if (!bt->tempogram_obj) {
      AUBIO_ERR("beattracking: failed to create tempogram object\n");
      bt->use_tempogram = 0;
      return AUBIO_FAIL;
    }
    
    /* Create tempogram output matrix (rows = tempo bins, cols = 1 for single frame) */
    uint_t tempo_bins = tempogram_win / 2 + 1;
    bt->tempogram_out = new_fmat(tempo_bins, 1);
    
    if (!bt->tempogram_out) {
      AUBIO_ERR("beattracking: failed to create tempogram output matrix\n");
      del_aubio_tempogram(bt->tempogram_obj);
      bt->tempogram_obj = NULL;
      bt->use_tempogram = 0;
      return AUBIO_FAIL;
    }
  }
  
  return AUBIO_OK;
}

uint_t
aubio_beattracking_set_onset_enhancement(aubio_beattracking_t * bt, uint_t enabled)
{
  AUBIO_ASSERT_NOT_NULL(bt);
  
  bt->onset_enhancement = enabled ? 1 : 0;
  
  return AUBIO_OK;
}

void
aubio_beattracking_get_acf(const aubio_beattracking_t * bt, fvec_t * acf)
{
  AUBIO_ASSERT_NOT_NULL(bt);
  AUBIO_ASSERT_NOT_NULL(acf);
  
  if (!bt->acf) {
    fvec_zeros(acf);
    return;
  }
  
  /* Copy autocorrelation function to output */
  uint_t copy_len = MIN(bt->acf->length, acf->length);
  uint_t i;
  for (i = 0; i < copy_len; i++) {
    AUBIO_ASSERT_BOUNDS(i, bt->acf->length);
    AUBIO_ASSERT_BOUNDS(i, acf->length);
    acf->data[i] = bt->acf->data[i];
  }
  
  /* Zero remaining elements if output is larger */
  for (i = copy_len; i < acf->length; i++) {
    AUBIO_ASSERT_BOUNDS(i, acf->length);
    acf->data[i] = 0.0;
  }
}

/* Phase 3A: Onset Enhancement for Tempogram
 * Preprocess onset signal to improve beat periodicity detection
 * Uses median filtering to smooth noisy onset patterns from polyphonic music
 */
static smpl_t
aubio_beattracking_enhance_onset(aubio_beattracking_t * bt, smpl_t raw_onset)
{
  AUBIO_ASSERT_NOT_NULL(bt);
  AUBIO_ASSERT_NOT_NULL(bt->onset_history);
  
  /* Skip enhancement if disabled */
  if (!bt->onset_enhancement) {
    return raw_onset;
  }
  
  /* Add raw onset to circular history buffer */
  AUBIO_ASSERT_BOUNDS(bt->onset_history_pos, bt->onset_history->length);
  bt->onset_history->data[bt->onset_history_pos] = raw_onset;
  bt->onset_history_pos = (bt->onset_history_pos + 1) % bt->onset_history->length;
  
  /* Apply median filter to reduce noise from overlapping drum sounds
   * Median is robust to outliers while preserving sharp onset peaks
   * This helps FFT resolve clear periodicity in polyphonic music
   */
  smpl_t smoothed_onset = fvec_median(bt->onset_history);
  
  /* Adaptive thresholding: enhance peaks above local mean
   * This increases contrast between beats and background
   */
  smpl_t mean_onset = fvec_mean(bt->onset_history);
  smpl_t enhanced_onset = smoothed_onset;
  
  if (smoothed_onset > mean_onset) {
    /* Boost peaks: amplify onset values above mean by 1.5x (increased from 1.2x)
     * This makes periodic beats more prominent in FFT analysis
     */
    enhanced_onset = mean_onset + 1.5 * (smoothed_onset - mean_onset);
  } else {
    /* Suppress values below mean to increase contrast
     * This helps FFT focus on clear beat peaks
     */
    enhanced_onset = smoothed_onset * 0.7;
  }
  
  return enhanced_onset;
}

void
aubio_beattracking_feed_tempogram(aubio_beattracking_t * bt, smpl_t onset_value)
{
  AUBIO_ASSERT_NOT_NULL(bt);
  
  /* Only process if tempogram is enabled and initialized */
  if (!bt->use_tempogram || !bt->tempogram_obj || !bt->tempogram_out) {
    return;
  }
  
  /* Apply onset enhancement (Phase 3A) to improve detection on real audio
   * Median filtering and adaptive thresholding reduce polyphonic noise
   */
  smpl_t enhanced_onset = aubio_beattracking_enhance_onset(bt, onset_value);
  
  /* Create single-value onset vector for tempogram */
  fvec_t *onset_val = new_fvec(1);
  if (!onset_val) {
    return;
  }
  
  /* Feed enhanced onset value to tempogram
   * This should be called on every hop to build up the onset time series
   * that the tempogram FFT analyzes for periodic beat patterns
   */
  onset_val->data[0] = enhanced_onset;
  aubio_tempogram_do(bt->tempogram_obj, onset_val, bt->tempogram_out);
  
  del_fvec(onset_val);
}
