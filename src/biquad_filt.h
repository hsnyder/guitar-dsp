/*	Copyright (C) 2018, 2020 Harris M. Snyder

	This file is part of guitardsp.

	guitardsp is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	guitardsp is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with guitardsp.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef BIQUAD_FILT_H
#define BIQUAD_FILT_H

// This file and the associated .c contain a simple biquad filter implementation.
// The filter contains it's state in the following struct, and operates on one sample at a time.

struct bq_filter
{
	float cx0;
	float cx1;
	float cx2;
	float cy1;
	float cy2;
	float dly1;
	float dly2;
};

// "mode" constants for the filter
#define BQ_LOWPASS 1
#define BQ_HIGHPASS 2
#define BQ_BANDPASS 3
#define BQ_NOTCH 4

// returns 0 on OK, or -1 on error (and sets errno - most likely EINVAL if type is wrong).
// type should be one of the above defines
int make_biquad(struct bq_filter * output, int type, float freq_cutoff, float sample_rate, float Q);

// apply the filter to the next sample.
float apply_biquad(struct bq_filter * filt, float sample);

// a version of the above that is genericized
// used as a callback in, for example, the delay effect.
float apply_biquad_generic(void * filt, float sample);

#endif
