#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "address_map_arm.h"
#include "audio.h"
#include "physical.h"

#define NUM_NOTES 13

// The frequencies of the notes (C, C#/Db, D, D#/Eb, E, F, F#/Gb, G, G#/Ab, A, a#/Bb, B, C)
static double note_freq[NUM_NOTES] = {261.626, 277.183, 293.665, 311.127, 329.628, 349.228,
    369.994, 391.995, 415.305, 440.000, 466.164, 493.883, 523.251};
static int note_volume[NUM_NOTES] = {0};

volatile unsigned int * audio_ptr;    // virtual address pointer to audio device

int main (int argc, char *argv[])
{
    int note_index, i, note, cur_sample;
    char *c;
    double radians_per_sample;
    int fd = -1;        // used to open /dev/mem for access to physical addresses
    void *LW_virtual;   // used to map physical addresses for the light-weight bridge
    
    if (argc < 2 || strlen (argv[1]) < NUM_NOTES) {
        printf("Usage: ./part2 <string 13 1s and 0s, no spaces>\n");
        return 0;
    }
    
    // Create access to the FPGA light-weight bridge
    if ((fd = open_physical (fd)) == -1)
        return (-1);
    else if ((LW_virtual = map_physical (fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == NULL)
        return (-1);
    audio_ptr = (unsigned int *) (LW_virtual + AUDIO_BASE);
    
    // Initialize Audio core
    Audio_Init();
    
    // Determine the volume of each tone
    for (note = 0; note < NUM_NOTES; note++) {
        // If note is '1', then volume is max volume / 16 (this ensures sum of all 13 will not
        // exceed max)
        if (argv[1][note] == '1')
            note_volume[note] = MAX_VOLUME >> 4;
    }
    for (i = 0; i < SAMPLING_RATE; i++) {
        // Wait until there is space in outgoing fifo
        Audio_Wait_For_Buffer_Space();
                
        // Sum up the multiple sinusoids
        cur_sample = 0;
        for (note = 0; note < NUM_NOTES; note++) {
            if (note_volume[note] > 0) {
                // radians/cycle * cycles/sec * sec/sample = radians/sample
                radians_per_sample = PI2 * note_freq[note] / SAMPLING_RATE; 
                cur_sample += (int)(note_volume[note] * sin(i * radians_per_sample));
            }
        }
        Audio_Write_To_Buffer (cur_sample);
    }
    unmap_physical (LW_virtual, LW_BRIDGE_SPAN);    // release the physical-memory mapping
    close_physical (fd);    // close /dev/mem
    printf ("Exiting sample program solution\n");
    return 0;
} 
