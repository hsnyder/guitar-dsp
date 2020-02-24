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
#include "tremolo.h"
#include <math.h>
#include <errno.h>



lfo_t lfo(unsigned int samplerate, float freq_hz)
{
	lfo_t self;
	self.omega = freq_hz * 2 * M_PI / samplerate;
	self.t = 0;
	return self;
}

float lfo_next(lfo_t * self)
{
	float val = sinf(self->omega * self->t);
	self->t++;
	if(self->t * self->omega >= 2 * M_PI) self->t = 0;
	return val;
}


float lfo_next_tri(lfo_t * self)
{
	float val = 2 / M_PI * asinf(sinf(self->omega * self->t));
	self->t++;
	if(self->t * self->omega >= 2 * M_PI) self->t = 0;
	return val;
}


int tremolo_construct(tremolo_t * self, unsigned int samplerate, float depth, lfo_t lfo)
{
	if(depth < 0 || depth > 1 )
	{
		errno = EINVAL;
		return -1;
	}
	
	self->lfo = lfo;
	self->depth = depth;
	
	return 0;
}

float tremolo_apply(tremolo_t * self, float sample)
{
	float c = 1.0 - 0.5*(self->depth * lfo_next(&self->lfo) + self->depth);
	return c * sample;
}
