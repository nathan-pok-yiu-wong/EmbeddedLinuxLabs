#include "audio.h"
#include <math.h>

extern unsigned int * audio_ptr;    // virtual address pointer to audio device

void Audio_Init() {
    // Clear the FIFOs, disable interrupts
    *(audio_ptr) = 0xC;
    *(audio_ptr) = 0;
}

// Play a tone with frequency freq. Assume sampling rate of 48 kHz. 
// Volume is a 31-bit value (0x0 - 0x7fffffff)
void Audio_Play_Sinusoid(float freq, unsigned int length_msecs, unsigned int volume) {
    unsigned int num_of_samples = length_msecs * SAMPLING_RATE / 1000;
    int i;
    double radians_per_sample;

    radians_per_sample = PI2 * freq / SAMPLING_RATE; 
    for (i = 0; i < num_of_samples; i++) {
        // Wait until there is space (at least 15 bytes) in outgoing fifo
        while ((*(audio_ptr + FIFOSPACE) & 0xff0000) < 0xf0000 ||
            (*(audio_ptr + FIFOSPACE) & 0xff000000) < 0xf000000)
            ;
        // radians/cycle * cycles/sec * sec/sample = radians/sample
        *(audio_ptr + RDATA) = (int)(volume * sin (i * radians_per_sample));
        *(audio_ptr + LDATA) = (int)(volume * sin (i * radians_per_sample));
    }
}
