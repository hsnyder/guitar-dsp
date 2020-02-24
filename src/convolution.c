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
#include "convolution.h"

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include <wav.h> 

#define CONV_ERRMSG_BUFSZ 1024

char errmsg[CONV_ERRMSG_BUFSZ];

// read the impulse response supplied, and output the sample rate detected.
// IR_max_size_truncate is the maximum size of the impulse response (in samples) before it gets truncated.
// DrWav is used for wav file handling

int convolution_construct(convolution_t * self, char ** errMsg, unsigned int * rate, unsigned int period_sz, const char * IR_filename, unsigned int IR_max_size_truncate)
{  
	if(IR_max_size_truncate % 4 != 0)
	{
		snprintf(errmsg, CONV_ERRMSG_BUFSZ-1, "IR_max_size_truncate must be divisible by 4");
		*errMsg = errmsg;
		return -1;
	}
	
	if(period_sz % 4 != 0)
	{
		snprintf(errmsg, CONV_ERRMSG_BUFSZ-1, "period_sz must be divisible by 4");
		*errMsg = errmsg;
		return -1;
	}
	
	memset(self, 0 , sizeof(convolution_t));
	memset(errmsg,0, 1024);
	
	// read the wav file. 
	drwav wav;
	if (!drwav_init_file(&wav, IR_filename))
	{
		snprintf(errmsg, CONV_ERRMSG_BUFSZ-1, "Couldn't open specified IR '%s'",IR_filename);
		*errMsg = errmsg;
		return -1;
	}
	
	if(wav.sampleRate != 44100 && wav.sampleRate != 48000)
	{
		snprintf(errmsg, CONV_ERRMSG_BUFSZ-1, "Unsupported sample rate %u Hz", wav.sampleRate);
		*errMsg = errmsg;
		drwav_uninit(&wav);
		return -1;
	}
	*rate = wav.sampleRate;
	
	if(wav.channels != 1)
	{
		snprintf(errmsg, CONV_ERRMSG_BUFSZ-1, "Only mono files are supported as impulse responses");
		*errMsg = errmsg;
		drwav_uninit(&wav);
		return -1;
	}
	
	if(wav.totalSampleCount > IR_max_size_truncate)
	{
		snprintf(errmsg, CONV_ERRMSG_BUFSZ-1, "WARNING: Program only supports IRs of up to %d samples. Supplied file contained %u, so truncation will occurr", IR_max_size_truncate, wav.totalSampleCount);
		*errMsg = errmsg;
	}
	int n_to_read = wav.totalSampleCount > IR_max_size_truncate ? IR_max_size_truncate : wav.totalSampleCount;
	
	self->periodsz = period_sz;
	self->n = n_to_read;
	self->data = malloc(sizeof(float32x4_t) * self->n + sizeof(float) * (self->n + self->periodsz - 1));
	if(!self->data)
	{
		snprintf(errmsg, CONV_ERRMSG_BUFSZ-1, "%s", strerror(errno));
		*errMsg = errmsg;
		drwav_uninit(&wav);
		return -1;
	}
	memset(self->data, 0, sizeof(float32x4_t) * self->n + sizeof(float) * (self->n + self->periodsz - 1));
	float IR[n_to_read];
	size_t sampsread = drwav_read_f32(&wav, n_to_read, IR);
	
	if(sampsread != n_to_read)
	{
		snprintf(errmsg, CONV_ERRMSG_BUFSZ-1, "Expected %u samples, but got %zu", n_to_read, sampsread);
		*errMsg = errmsg;
		drwav_uninit(&wav);
		return -1;
	}
	
	float32x4_t* kernel_reverse = (float32x4_t*)self->data;
 
	// Reverse the kernel and repeat each value across a 4-vector
	for(int i=0; i < self->n; i++){
		float kernel_block[4] ;
		kernel_block[0] = IR[self->n - i - 1];
		kernel_block[1] = IR[self->n - i - 1];
		kernel_block[2] = IR[self->n - i - 1];
		kernel_block[3] = IR[self->n - i - 1];
 
		kernel_reverse[i] = vld1q_f32(kernel_block);
	}
	
	drwav_uninit(&wav);
	
	return 0;
}


void convolution_destruct(convolution_t * self)
{
	free(self->data);
}


// return pointer to the data buffer.
float * convolution_getInputPtr(convolution_t * self)
{
	float * buffer = (float*) (self->data + sizeof(float32x4_t) * self->n);
	return buffer + self->n - 1;
}


