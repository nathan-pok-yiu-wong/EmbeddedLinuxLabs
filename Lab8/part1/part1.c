#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include "address_map_arm.h"
#include "audio.h"
#include "physical.h"

#define NUM_NOTES 13

// The frequencies of the notes (C, C#/Db, D, D#/Eb, E, F, F#/Gb, G, G#/Ab, A, a#/Bb, B, C)
static float note_freq[NUM_NOTES] = {261.626, 277.183, 293.665, 311.127, 329.628, 349.228, 
    369.994, 391.995, 415.305, 440.000, 466.164, 493.883, 523.251};

volatile unsigned int * audio_ptr;    // virtual address pointer to audio device

int main (int argc, char *argv[])
{
    int note_index;
    int fd = -1;        // used to open /dev/mem for access to physical addresses
    void *LW_virtual;   // used to map physical addresses for the light-weight bridge
    
    // Create access to the FPGA light-weight bridge
    if ((fd = open_physical (fd)) == -1)
        return (-1);
    else if ((LW_virtual = map_physical (fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == NULL)
        return (-1);
    audio_ptr = (unsigned int *) (LW_virtual + AUDIO_BASE);
    
    // Initialize Audio core
    Audio_Init();
    
    for (note_index = 0; note_index < NUM_NOTES; note_index++)
        Audio_Play_Sinusoid (note_freq[note_index], 300, MAX_VOLUME >> 3);
    
    unmap_physical (LW_virtual, LW_BRIDGE_SPAN);    // release the physical-memory mapping
    close_physical (fd);                            // close /dev/mem
    printf ("Exiting sample program solution\n");
    return 0;
} 

