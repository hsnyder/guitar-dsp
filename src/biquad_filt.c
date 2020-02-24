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
#include "biquad_filt.h"
#include <errno.h>
#include <math.h>


float apply_biquad(struct bq_filter * filt, float sample)
{
	float w,y;
	w =  -filt->dly1 * filt->cy1;
	w -=  filt->dly2 * filt->cy2;
	w +=  sample;
	y =   filt->dly1 * filt->cx1;
	y +=  filt->dly2 * filt->cx2;
	y +=  w * filt->cx0;
	filt->dly2 = filt->dly1;
	filt->dly1 = w;
   
	return y;
}

float apply_biquad_generic(void * filt, float sample)
{
	return apply_biquad((struct bq_filter *) filt, sample);
}


int make_biquad(struct bq_filter * output, int type, float freq_cutoff, float sample_rate, float Q)
{
	float k = tanf(M_PI * freq_cutoff / sample_rate);
	output->dly1 = 0;
	output->dly2 = 0;
	float norm;
	
	switch(type)
	{
		case BQ_LOWPASS:
			norm = 1.0 / (1.0 + k/Q + k*k);
			output->cx0 = k * k * norm;
			output->cx1 = 2 * output->cx0;
			output->cx2 = output->cx0;
			output->cy1 = 2 * (k * k - 1) * norm;
			output->cy2 = (1 - k/Q + k*k) * norm;
			break;
		case BQ_HIGHPASS:
			norm = 1.0 / (1.0 + k/Q + k*k);
			output->cx0 = 1 * norm;
			output->cx1 = -2 * output->cx0;
			output->cx2 = output->cx0;
			output->cy1 = 2 * (k * k -1) * norm;
			output->cy2 = (1 - k/Q + k*k) * norm;
			break;
		case BQ_BANDPASS:
			norm = 1.0 / (1.0 + k/Q + k*k);
			output->cx0 = k/Q * norm;
			output->cx1 = 0;
			output->cx2 = - output->cx0;
			output->cy1 = 2 * (k * k -1) * norm;
			output->cy2 = (1 - k/Q + k*k) * norm;
			break;
		case BQ_NOTCH:
			norm = 1.0 / (1.0 + k/Q + k*k);
			output->cx0 = (1 + k*k) * norm;
			output->cx1 = 2 * (k * k - 1) * norm;
			output->cx2 = output->cx0;
			output->cy1 = output->cx1;
			output->cy2 = (1 - k/Q + k*k) * norm;
			break;
		default:
			errno = EINVAL;
			return -1;
	}
	
	return 0;
}
