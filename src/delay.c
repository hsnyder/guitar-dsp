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
#include "delay.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>

float simple_delay_apply(struct simple_delay * delay, float sample)
{
	float output = sample + delay->mix * delay->dLine[delay->pos];
	delay->dLine[delay->pos] = sample + delay->feedback_gain * delay->dLine[delay->pos];
	if(delay->feedback_fn) 
		delay->dLine[delay->pos] = delay->feedback_fn(delay->feedback_fn_arg, delay->dLine[delay->pos]);
	delay->pos = (delay->pos + 1) % delay->dLine_n;
	return output;
}

int simple_delay_construct(struct simple_delay * output, 
						    float feedback, 
						    float mix, 
						    unsigned int sampRate, 
						    float delayTimeMS,
						    fdbk_fn_t feedback_fn,
						    void* feedback_fn_arg)
{
	if( feedback < 0         || feedback > 1       ||
		mix < 0              || mix > 1            ||
		delayTimeMS > 1000.0 || delayTimeMS < 0.0 ||
		sampRate < 44100     || sampRate > 48000   )
		{
			errno = EINVAL;
			return -1;
		}
	
	output->dLine_n = (size_t)(delayTimeMS * sampRate / 1000);
	output->dLine = malloc(sizeof(float) * output->dLine_n);
	if(!output->dLine) return -1;
	memset(output->dLine, 0, sizeof(float) * output->dLine_n);
	
	output->pos = 0;
	output->mix = mix;
	output->feedback_gain = feedback;
	
	output->feedback_fn = feedback_fn;
	output->feedback_fn_arg = feedback_fn_arg;
	
	return 0;
}

void simple_delay_destruct(struct simple_delay * delay)
{
	free(delay->dLine);
}



void wrap(int M, float *w, float **p)
{
	if(*p > w + M)
		*p -= M+1;
	if(*p < w)
		*p += M+1;
}


void cdelay(int D, float *w, float **p)
{
	(*p)--;
	wrap(D,w,p);
}


float tap(int D, float *w, float *p, int i)
{
	return w[(p - w + i) % (D + 1)];
}

float tap2(int D, float *w, int q, int i)
{
	return w[(q+i) % (D+1)];
}


float tapi(int D, float *w, float *p, float d)
{
	int i, j;
	float si, sj;
	
	i = (int) d;
	j = (i+1) % (D+1);
	
	si = tap(D, w, p, i);
	sj = tap(D, w, p, j);
	
	return si + (d-i) * (sj-si);
}


float tapi2(int D, float *w, int q, float d)
{
	int i,j;
	float si,sj;
	
	i = (int) d;
	j = (i+1) % D+1;
	
	si = tap2(D, w, q, i);
	sj = tap2(D, w, q, j);
	
	return si + (d-i) * (sj-si);
}
