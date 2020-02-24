   
dsp: src/dsp.c 
	gcc src/*.c -Idrwav -O3 -mfpu=neon-vfpv4 -mcpu=cortex-a7 -lasound -lm -lrt -o bin/dsp

