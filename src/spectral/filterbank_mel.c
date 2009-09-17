/*
  Copyright (C) 2007-2009 Paul Brossier <piem@aubio.org>
                      and Amaury Hazan <ahazan@iua.upf.edu>

  This file is part of Aubio.

  Aubio is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Aubio is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Aubio.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "aubio_priv.h"
#include "fvec.h"
#include "cvec.h"
#include "spectral/filterbank.h"
#include "mathutils.h"

void
aubio_filterbank_set_mel_coeffs (aubio_filterbank_t * fb, smpl_t samplerate,
    smpl_t freq_min, smpl_t freq_max)
{

  fvec_t *filters = aubio_filterbank_get_coeffs (fb);
  uint_t n_filters = filters->channels, win_s = filters->length;

  /* Malcolm Slaney parameters */
  smpl_t lowestFrequency = 133.3333;
  smpl_t linearSpacing = 66.66666666;
  smpl_t logSpacing = 1.0711703;

  uint_t linearFilters = 13;
  uint_t logFilters = 27;
  uint_t allFilters = linearFilters + logFilters;

  /* throw a warning if filterbank object fb is too short */
  if (allFilters > n_filters) {
    AUBIO_WRN ("not enough Mel filters, got %d but %d needed\n",
        n_filters, allFilters);
  }

  /* buffers for computing filter frequencies */
  fvec_t *freqs = new_fvec (allFilters + 2, 1);

  /* convenience reference to lower/center/upper frequency for each triangle */
  fvec_t *lower_freqs = new_fvec (allFilters, 1);
  fvec_t *upper_freqs = new_fvec (allFilters, 1);
  fvec_t *center_freqs = new_fvec (allFilters, 1);

  /* height of each triangle */
  fvec_t *triangle_heights = new_fvec (allFilters, 1);

  /* lookup table of each bin frequency in hz */
  fvec_t *fft_freqs = new_fvec (win_s, 1);

  uint_t fn;                    /* filter counter */
  uint_t bin;                   /* bin counter */

  /* first step: filling all the linear filter frequencies */
  for (fn = 0; fn < linearFilters; fn++) {
    freqs->data[0][fn] = lowestFrequency + fn * linearSpacing;
  }
  smpl_t lastlinearCF = freqs->data[0][fn - 1];

  /* second step: filling all the log filter frequencies */
  for (fn = 0; fn < logFilters + 2; fn++) {
    freqs->data[0][fn + linearFilters] =
        lastlinearCF * (POW (logSpacing, fn + 1));
  }

  /* fill up the lower/center/upper */
  for (fn = 0; fn < allFilters; fn++) {
    lower_freqs->data[0][fn] = freqs->data[0][fn];
    center_freqs->data[0][fn] = freqs->data[0][fn + 1];
    upper_freqs->data[0][fn] = freqs->data[0][fn + 2];
  }

  /* compute triangle heights so that each triangle has unit area */
  for (fn = 0; fn < allFilters; fn++) {
    triangle_heights->data[0][fn] =
        2. / (upper_freqs->data[0][fn] - lower_freqs->data[0][fn]);
  }

  /* fill fft_freqs lookup table, which assigns the frequency in hz to each bin */
  for (bin = 0; bin < win_s; bin++) {
    fft_freqs->data[0][bin] = aubio_bintofreq (bin, samplerate, win_s);
  }

  /* zeroing of all filters */
  fvec_zeros (filters);

  /* building each filter table */
  for (fn = 0; fn < n_filters; fn++) {

    /* skip first elements */
    for (bin = 0; bin < win_s - 1; bin++) {
      if (fft_freqs->data[0][bin] <= lower_freqs->data[0][fn] &&
          fft_freqs->data[0][bin + 1] > lower_freqs->data[0][fn]) {
        break;
      }
    }
    bin++;

    /* compute positive slope step size */
    smpl_t riseInc =
        triangle_heights->data[0][fn] /
        (center_freqs->data[0][fn] - lower_freqs->data[0][fn]);

    /* compute coefficients in positive slope */
    for (; bin < win_s - 1; bin++) {
      filters->data[fn][bin] =
          (fft_freqs->data[0][bin] - lower_freqs->data[0][fn]) * riseInc;

      if (fft_freqs->data[0][bin + 1] > center_freqs->data[0][fn])
        break;
    }
    bin++;

    /* compute negative slope step size */
    smpl_t downInc =
        triangle_heights->data[0][fn] /
        (upper_freqs->data[0][fn] - center_freqs->data[0][fn]);

    /* compute coefficents in negative slope */
    for (; bin < win_s - 1; bin++) {
      filters->data[fn][bin] +=
          (upper_freqs->data[0][fn] - fft_freqs->data[0][bin]) * downInc;

      if (fft_freqs->data[0][bin + 1] > upper_freqs->data[0][fn])
        break;
    }
    /* nothing else to do */

  }

  /* destroy temporarly allocated vectors */
  del_fvec (freqs);
  del_fvec (lower_freqs);
  del_fvec (upper_freqs);
  del_fvec (center_freqs);

  del_fvec (triangle_heights);
  del_fvec (fft_freqs);

}
