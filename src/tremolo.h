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

#ifndef TREMOLO_H
#define TREMOLO_H

// This file and the associated .c contain a simple implemenation of a tremolo effect, and a low-frequency oscillator (LFO) implementation.

typedef struct lfo
{
	float omega; // angular frequency divided by sample rate
	unsigned int t; // current sample number
} lfo_t;

lfo_t lfo(unsigned int samplerate, float freq_hz); // lfo constructor
float lfo_next(lfo_t * self); // get next value from the LFO (called every interval of the sample rate). sinusoidal.
float lfo_next_tri(lfo_t * self); // same as above but triangle wave.





typedef struct tremolo
{
	lfo_t lfo; // tremolo oscillator
	float depth; // depth of amplitude oscillation.
} tremolo_t;


// returns -1 on failure, 0 on success. Sets errno on failure. 
// rate: how frequently it oscillates. 0 to 1
// depth: how deeply it oscillates. 0 to 1
int tremolo_construct(tremolo_t * self, unsigned int samplerate, float depth, lfo_t lfo);

float tremolo_apply(tremolo_t * self, float sample);

#endif
