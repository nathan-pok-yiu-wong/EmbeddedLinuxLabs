#include "audio.h"
#include <math.h>

extern unsigned int * audio_ptr;    // virtual address pointer to audio device

void Audio_Init() {
    // Clear the FIFOs, disable interrupts
    *(audio_ptr) = 0xC;
    *(audio_ptr) = 0;
}

void Audio_Write_To_Buffer(int sample) {
    *(audio_ptr + RDATA) = sample;
    *(audio_ptr + LDATA) = sample;
}

void Audio_Wait_For_Buffer_Space() {
    while((*(audio_ptr + FIFOSPACE) & 0xff0000) < 0xf0000 ||
        (*(audio_ptr + FIFOSPACE) & 0xff000000) < 0xf000000 )
    ;
}
