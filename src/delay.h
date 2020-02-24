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
#ifndef DELAY_H
#define DELAY_H

// this file and the associated .c contain an implementation of a simple delay (echo) effect.

typedef float (*fdbk_fn_t) (void*,float);

typedef
struct simple_delay
{
	fdbk_fn_t feedback_fn; // callback to be called whenever the delay line feeds back (i.e. whenever the output of the delay line re-enters the delay line).
	// used to insert low pass filters to simpluate an "analog" delay sound.
	void * feedback_fn_arg; // argument to callback
	float feedback_gain; // gain each time the delay line feeds back. 0 = no feedback, 1 = signal never decays.
	
	float mix; // mix percentage betwen effected (delayed) signal and dry signal.
	
	float * dLine; // delay line buffer
	unsigned int dLine_n; // delay line size
	int pos; // current position in delay line
}simple_delay_t;

float simple_delay_apply(struct simple_delay * delay, float sample);

// feedback and mix should be 0 to 1
// returns 0 if ok, -1 and sets errno on an error.
int simple_delay_construct(struct simple_delay * output, 
						    float feedback, 
						    float mix, 
						    unsigned int sampRate, 
						    float delayTimeMS,
						    fdbk_fn_t feedback_fn,
						    void* feedback_fn_arg);
						    
void simple_delay_destruct(struct simple_delay * delay);


// -- some other delay related functions that just operate on raw buffers with no struct encapsulation. reused in chorusflange.c --
// These functions are based on examples in: Orfanidis, Introduction to Signal Processing (2010).

// circular wrap of pointer p, relative to array w
void wrap(int M, float *w, float **p);

// circular buffer implementation of D-fold delay
void cdelay(int D, float *w, float **p);

// i-th tap of a circular delay line buffer (w) of length D, where p is a circular pointer indicating the start of the buffer. 
float tap(int D, float *w, float *p, int i);

// i-th tap of a circular delay line buffer (by index, q, instead of circular pointer)
float tap2(int D, float *w, int q, int i);

// interpolated tap output of delay line 
// usage:   d = desired non-integer delay
//          q = circular buffer pointer
float tapi(int D, float *w, float *p, float d);

// interpolated tap output of dela line (by index)
// usage:   d = desired non-integer delay
//          q = circular buffer offset
float tapi2(int D, float *w, int q, float d);

#endif
