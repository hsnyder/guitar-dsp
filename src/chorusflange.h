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
#ifndef CHORUS_FLANGE_H
#define CHORUS_FLANGE_H

// This file and the associated .c contain a generic implementation of a modulation effect (i.e. "chorus" or "flanger").

#include "delay.h"
#include "tremolo.h"
#include "biquad_filt.h"


// the struct contains the sate of the effect, and needn't be touched manually (with the exception of bq_filter, if the user wishes to insert a biquad filter into the delay line).
// this structure is set up by the timeMod_construct function.
typedef
struct timeMod
{
	float mix;
	float depth;
	float excursion;
	float feedback;
	lfo_t lfo;
	float * w;
	float * p;
	int D;
	struct bq_filter * bq; // manually set this (after calling _construct) to put a filter on the delay line.
} timeMod_t;

// For chorus: delay time is 5-30 ms
// For flange: delay time is 1-5 ms
// Flanger also typically uses nonzero feedback.

// returns -1 and sets errno on error.
// returns 0 on success
// float params are 0 to 1.
//	depth - the strength of the modulation effect (the amount of time-delayed signal mixed in) (0 to 1)
// 	excursion - the amplitude of the pitch oscillations of the modulated signal (0 to 1)
// 	feedback - the amplitude of the delay line output that's fed back into the delay line (0 to 1). Makes the sound "flangier"
// 	delayTimeMS - delay time in milliseconds
//      lfo - the low freqency oscillator responsible for the varying delay of the modulated signal.
int timeMod_construct(timeMod_t * self, unsigned int samplerate,  
					float depth,
					float excursion,
					float feedback,
					float delayTimeMS,
					lfo_t lfo);


void timeMod_destruct(timeMod_t * self);
			
// act on the next sample		
float timeMod_apply(timeMod_t * self, float sample);

#endif
