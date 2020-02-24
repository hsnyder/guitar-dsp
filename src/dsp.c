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
#define STRING2(x) #x
#define STRING(x) STRING2(x)

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <alsa/asoundlib.h>

// Linux/POSIX
#include <unistd.h> 
#include <sys/resource.h>
#include <sched.h>

#include "biquad_filt.h"
#include "delay.h"
#include "convolution.h"
#include "tremolo.h"
#include "chorusflange.h"


#define PERIODSZ 64 // Number of samples to fetch/write at a time from the audio device, i.e. wakeup interval
#define NPERIODS 2 // Number of periods that ALSA buffers at a time. Total latency is period size * number of periods buffered (each direction).
#define N 1440 // Impulse response length. Longer impulse responses are truncated. Strongly affects whether or not this program will be able to hit it's audio IO deadlines.

_Static_assert(N % 4 == 0, "N must be divisible by 4");
_Static_assert(PERIODSZ % 4 == 0, "PERIODSZ must be divisible by 4");

snd_pcm_t *playback_handle;
snd_pcm_t *capture_handle;

unsigned int rate = 44100;


// Function to deal with all the ALSA boilerplate code required to set up a device. 
// We need to call this twice (to set up the input and output devices respectively).
//
// The first and last arguments are input, the others are outputs.
void setup_dev(const char * dev, snd_pcm_t ** handle, snd_pcm_hw_params_t ** hwparams, snd_pcm_sw_params_t ** swparams, snd_pcm_stream_t stream)
{
	int err;

	if ((err = snd_pcm_open (handle, dev, stream, 0)) < 0) {
		fprintf (stderr, "cannot open audio device %s (%s)\n", 
			 dev,
			 snd_strerror (err));
		exit (1);
	}
	   
	if ((err = snd_pcm_hw_params_malloc (hwparams)) < 0) {
		fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
			 
	if ((err = snd_pcm_hw_params_any (*handle, *hwparams)) < 0) {
		fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_access (*handle, *hwparams, SND_PCM_ACCESS_RW_NONINTERLEAVED)) < 0) {
		fprintf (stderr, "cannot set access type (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_format (*handle, *hwparams, SND_PCM_FORMAT_FLOAT_LE)) < 0) {
		fprintf (stderr, "cannot set sample format (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	int dir = 0;
	if ((err = snd_pcm_hw_params_set_rate_near (*handle, *hwparams, &rate, &dir)) < 0) {
		fprintf (stderr, "cannot set sample rate (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_channels (*handle, *hwparams, 2)) < 0) {
		fprintf (stderr, "cannot set channel count (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	
	
	if ((err = snd_pcm_hw_params_set_periods(*handle, *hwparams, NPERIODS, 0)) < 0) {
	  fprintf(stderr, "Error setting periods (%s) \n", snd_strerror(err));
	  exit(1);
	}
  
	if ((snd_pcm_hw_params_set_period_size(*handle, *hwparams, PERIODSZ, 0)) < 0) {
	  fprintf(stderr, "Error setting buffersize (%s) \n", snd_strerror(err));
	  exit(1);
	}
	
	
	
	

	if ((err = snd_pcm_hw_params (*handle, *hwparams)) < 0) {
		fprintf (stderr, "cannot set parameters (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	
	snd_pcm_uframes_t persz = 0;
	snd_pcm_uframes_t buffersize = 0;
	unsigned int periods  = 0;
	
	err = snd_pcm_hw_params_get_period_size(*hwparams, &persz, &dir);
	if(err) printf("Err 1: %s\n", snd_strerror(err));
	err = snd_pcm_hw_params_get_periods(*hwparams, &periods, &dir);
	if(err) printf("Err 2: %s\n", snd_strerror(err));
	err = snd_pcm_hw_params_get_buffer_size(*hwparams, &buffersize);
	if(err) printf("Err 3: %s\n", snd_strerror(err));
	


	/* tell ALSA to wake us up whenever PERIODSZ or more frames
	   of playback data can be delivered. Also, tell
	   ALSA that we'll start the device ourselves.
	*/

	if ((err = snd_pcm_sw_params_malloc (swparams)) < 0) {
		fprintf (stderr, "cannot allocate software parameters structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	if ((err = snd_pcm_sw_params_current (*handle, *swparams)) < 0) {
		fprintf (stderr, "cannot initialize software parameters structure (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	if ((err = snd_pcm_sw_params_set_avail_min (*handle, *swparams, PERIODSZ)) < 0) {
		fprintf (stderr, "cannot set minimum available count (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	if (stream == SND_PCM_STREAM_PLAYBACK && (err = snd_pcm_sw_params_set_start_threshold (*handle, *swparams, 2*PERIODSZ)) < 0) {
		fprintf (stderr, "cannot set start mode (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	if (stream == SND_PCM_STREAM_CAPTURE && (err = snd_pcm_sw_params_set_start_threshold (*handle, *swparams, 0U)) < 0) {
		fprintf (stderr, "cannot set start mode (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
	if ((err = snd_pcm_sw_params (*handle, *swparams)) < 0) {
		fprintf (stderr, "cannot set software parameters (%s)\n",
			 snd_strerror (err));
		exit (1);
	}

	/* the interface will interrupt the kernel every PERIODSZ frames, and ALSA
	   will wake up this program very soon after that.
	*/

	if ((err = snd_pcm_prepare (*handle)) < 0) {
		fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
			 snd_strerror (err));
		exit (1);
	}
}


#define ODEVICE "default"
#define IDEVICE "default"

// decibels to amplitude
inline float db_to_amp(float db)
{
	return pow(10.0, db / 20.0);
}

// amplitude to decibels
inline float amp_to_db(float amp)
{
	return 20.0 * log10f( amp );
}


int  
main (int argc, char *argv[])
{

	// These variables enable and disable the various effects that have been implemented.
	// At the moment they cannot currently be configured while the program is running. 
	bool efx_conv = true; // convolution
	bool efx_lowcut = false; // highpass filter
	bool efx_tremolo = false;
	bool efx_choflange = false;
	bool efx_delay = false;
	
	// We're expecting to get the impulse response filename (wav) as a command line argument.
	// We also can accept a second command line argument that indicates the gain in DB (prefixed by + or -).
	int av_ir_idx = 0;
	float gain = 1.0;
	// Deal with command line args. 
	for(int i = 1; i < argc; i++)
	{
		if(strlen(argv[i]) < 2)
		{
			printf("One or more arguments were too short.\n");
			exit(1);
		}
		
		if(argv[i][0] == '+' || argv[i][0] == '-')
		{
			gain = strtof(argv[i],NULL);
			printf("Gain: %f dB\n", gain);
			if(gain != 0.0) gain = db_to_amp(gain);
		}
		else
		{
			// the "other" argument we assume is the path to the impulse response
			av_ir_idx = i;
		}
	}
	
	
   

	
	// --- Hardware setup --------------------------------
	snd_pcm_hw_params_t *o_hw_params;
	snd_pcm_sw_params_t *o_sw_params;
	snd_pcm_hw_params_t *i_hw_params;
	snd_pcm_sw_params_t *i_sw_params;
	snd_pcm_sframes_t frames_to_deliver;

	setup_dev(ODEVICE, &playback_handle, &o_hw_params, &o_sw_params ,SND_PCM_STREAM_PLAYBACK);
	snd_pcm_hw_params_free (o_hw_params);
	setup_dev(IDEVICE, &capture_handle, &i_hw_params, &i_sw_params ,SND_PCM_STREAM_CAPTURE);
	snd_pcm_hw_params_free (i_hw_params);
	
	// --- Set this process to high priority --------------------------------

	// This seems to slightly reduce missed audio IO deadlines on the Raspberry Pi 3B.
	if(-1 == setpriority(PRIO_PROCESS, getpid(), SCHED_FIFO))
	{
		printf("Failed to set priority: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	
	// --- Effects setup --------------------------------

	// In this section we instantiate all of the effects. 
	// At the moment, the effects parameters are hard coded and cannot be changed while the software is running.
	// See the relevant "_construct" functions for descriptions of what those parameters are.


	// MUST setup convolution first because the IR can change the sample rate.
	
	// Convolution
	convolution_t conv;
	if(av_ir_idx == 0) 
	{
		printf("No IR.\n");
		efx_conv = false;
	}
	else // we are using an IR
	{
		// LOAD IR
		char * msg = NULL;
		int err = convolution_construct(&conv, &msg, &rate, PERIODSZ, argv[av_ir_idx], N);
		if(msg != NULL) printf("%s\n",msg);
		if(err == -1) exit(1);
		printf("IR '%s' Loaded.\n", argv[av_ir_idx]);
	}
	
	// Low frequency cut
	struct bq_filter LowCutFilt;
	if(-1 == make_biquad(&LowCutFilt, BQ_HIGHPASS, 150, rate, 0.707))
	{
		printf("Failed to create high pass filter: %s\n", strerror(errno));
		exit(1);
	}
	
	
	// Tremolo
	tremolo_t trem;
	if(-1 == tremolo_construct(&trem, rate, 0.4, lfo(rate, 3.5)))
	{
		printf("Failed to create tremolo: %s\n", strerror(errno));
		exit(1);
	}
	
	// Chorus/flange
	timeMod_t mod;
	if(-1 == timeMod_construct(&mod, rate, 1.0, 0.2, 0.0, 5.0, lfo(rate, 1.0)))
	{
		printf("Failed to construct chorus/flange effect: %s\n", strerror(errno));
		exit(1);
	}
	struct bq_filter cfLPF;
	if(-1 == make_biquad(&cfLPF, BQ_LOWPASS, 2000, rate, 0.707))
	{
		printf("Failed to create low pass filter: %s\n", strerror(errno));
		exit(1);
	}
	mod.bq = &cfLPF; // we assign a low pass filter to our modulation effect to amek it sound warmer.
	
	// Delay (with lowpass in the feedback loop to make it sound a bit warmer / more analog)
	// ... lowpass
	struct bq_filter dlyLPF;
	if(-1 == make_biquad(&dlyLPF, BQ_LOWPASS, 2000, rate, 0.707))
	{
		printf("Failed to create low pass filter: %s\n", strerror(errno));
		exit(1);
	}
	// ... actual delay
	struct simple_delay dly;
	if(-1 == simple_delay_construct(&dly, db_to_amp(-10), 0.3, rate, 250.0, &apply_biquad_generic, &dlyLPF ))
	{
		printf("Failed to construct delay: %s\n", strerror(errno));
		exit(1);
	}
   
   
	// --- Routing ----------------------------
	
	// effects order is:
	// convolution -> gain -> tremolo -> chorus/flange -> delay 
	
	// define our buffers
	float sampsOutL[PERIODSZ];
	float sampsOutR[PERIODSZ];
	float garbage[PERIODSZ]; // unused right input channel (guitar plugged into left).
	float intermediate1[PERIODSZ]; // intermediate buffer used between effects.
	
	for(int i = 0; i < PERIODSZ; i++)
	{
		sampsOutL[i] = 0.0;
		sampsOutR[i] = 0.0;
		intermediate1[i] = 0.0;
		garbage[i] = 0.0;
	}
		
	//  card i/o

	// these pointers actually do the routing. 
	
	// card_ibufs gives pointers to where the L and R input channel data should be copied.
	void*  card_ibufs[2] = {intermediate1,garbage};
	
	// if we're convolving, we need to send the input data to the convolution buffer instead of intermediate1.
	if(efx_conv) card_ibufs[0] = convolution_getInputPtr(&conv);

	// these pointers tell the program where to get the L and R output samples.
	void*  card_obufs[2] = {intermediate1,intermediate1}; 
	// right now we're using the intermediate buffer as our output buffer.
	// I used to have a (non-open-source) reverb implemented in here as well, which was a stereo effect. 
	// The stereo nature of that effect justified the existence of the sampsOutL and sampsOutR buffers
	// i.e. the input to the reverb was intermediate1 and the output to the card was the aforementioned.
	
  
	// --- Main loop ----------------------------
	
	int err;


	while (1) {
		
		// --- Input ----------------------------
		err = snd_pcm_readn (capture_handle, card_ibufs, PERIODSZ);
		
		if(err < 0) 
		{ 
			err = snd_pcm_recover(capture_handle, err, 0);
		}
		if(err < 0)
		{
			printf ("Read failed (%s)\n", snd_strerror (err));
			break;
		}
		else if (err != PERIODSZ)
		{
			printf("Couldn't read what I wanted.\n");
			break;
		}
	   
		// --- Convolution ----------------------------
		if(efx_conv) convolution_apply(&conv, intermediate1);
		
		// --- Gain, Tremolo, Chorus/Flange, Delay ----------------------------

		// while convolution needs to operate on a chunk of data at a time, the following effects
		// can operate one sample at a time. So apply them, sample by sample.
		for(int i = 0; i < PERIODSZ; i++)
		{
			intermediate1[i] *= gain;
			if(efx_lowcut) intermediate1[i] = apply_biquad(&LowCutFilt, intermediate1[i]);
			if(efx_tremolo) intermediate1[i] = tremolo_apply(&trem, intermediate1[i]);
			if(efx_choflange) intermediate1[i] = timeMod_apply(&mod, intermediate1[i]);
			if(efx_delay) intermediate1[i] = simple_delay_apply(&dly, intermediate1[i]);
		   
		}
		
 
		// --- Output ----------------------------
		err = snd_pcm_writen (playback_handle, card_obufs, PERIODSZ);
		
		if(err < 0) 
		{ 
			err = snd_pcm_recover(playback_handle, err, 0);
		}
		if(err < 0) 
		{
			printf ("Write failed (%s)\n", snd_strerror (err));
			break;
		}
		else if (err != PERIODSZ)
		{
			printf("Couldn't write what I wanted.\n");
			break;
		}
	
	} 

	
	snd_pcm_close (playback_handle);
	snd_pcm_close (capture_handle);

	
	simple_delay_destruct(&dly);
	convolution_destruct(&conv);
	timeMod_destruct(&mod);
	
	exit (0);
} 

