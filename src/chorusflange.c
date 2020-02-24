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
#include "chorusflange.h"
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>

int timeMod_construct(timeMod_t * self, unsigned int samplerate, 
					float depth,
					float excursion, 
					float feedback,
					float delayTimeMS,
					lfo_t lfo)
{
	if(depth < 0 || depth > 1 || delayTimeMS < 0 || delayTimeMS > 40
		|| excursion < 0 || excursion > 1)
	{
		errno = EINVAL;
		return -1;
	}
	
	self->bq = NULL; // optional biquad filter (applied to the delayed signal only).
	
	self->excursion = excursion; // amplitude of the pitch oscillation of the modulated signal
	self->depth = depth; // perceived strength of the effect
	self->lfo = lfo;
	self->feedback = feedback;
	
	// delay time
	self->D = (delayTimeMS/1000.0) * samplerate;
	
	// delay buffer
	self->w = calloc(self->D+1, sizeof(float));
	// circular pointer into the allocated buffer
	self->p = self->w;
	
	if(self->w)
		return 0;
	return -1;
}


void timeMod_destruct(timeMod_t * self)
{
	free(self->w);
}



// This implementation is based on examples from: Orfanidis, Introduction to Signal Processing (2010).


float timeMod_apply(timeMod_t * self, float sample)
{
	float mix = 0.5 * self->depth;

	// To implement modulation, we tap the delay line in a varying location.
	// The varying amount of delay is what causes the pitch shifting that is characteristic of these effects.
	// Hence the use of the 'tapi' (interpolated delay line tap) functions from delay.h/delay.c
	
	//float d = self->D - self->D * 0.5*self->excursion * (lfo_next(&self->lfo) + 1);
	float d = 0.5 * self->D * (1 - self->excursion * lfo_next_tri(&self->lfo));
	float s = tapi(self->D, self->w, self->p, d);
	float y = (1.0-mix) * sample + mix * s;
	//float y = 0.5 * (sample + s);
	
	float x = sample + s * self->feedback ;
	
	if(self->bq) *self->p = apply_biquad(self->bq,x); 
	else *self->p = x;
	
	cdelay(self->D, self->w, &self->p);
	return y;
}
