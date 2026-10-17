#include "physical.h"
#include "../include/address_map_arm.h"
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>


volatile sig_atomic_t stop; // sig_atomic ensures stop doesn't getting corrupted during an interrupt
void catchSIGINT(int); // SIGNIT = ^C, used to close program
unsigned char message[] = "intEL SoC FPGA     ";

#define LEN ((int) sizeof message - 1)
#define DISPLAY 6

// 128 is the size of ASCII characters (0-127)
unsigned char seg_table[128] = {
    ['i'] = 0b00000100,
    ['n'] = 0b01010100,
    ['t'] = 0b01111000,
    ['E'] = 0b01111001,
    ['L'] = 0b00111000,
    ['S'] = 0b01101101,
    ['o'] = 0b01011100,
    ['C'] = 0b00111001,
    ['F'] = 0b01110001,
    ['P'] = 0b01110011,
    ['G'] = 0b01111101,
    ['A'] = 0b01110111,
    [' '] = 0b00000000,
};

unsigned char code[LEN];



int main(void)
{
    signal(SIGINT, catchSIGINT); // Signal is an interrupt handler, catching SIGINT, then handling it with the catchSIGINT function

    struct timespec ts;
    time_t start_time;
    start_time = time(NULL); // returns current time
    int pos = 0;
    int scroll = 1;
    unsigned int w[DISPLAY];

    volatile int *HEX3_HEX0_ptr, *HEX5_HEX4_ptr, *KEY_ptr;
    int fd = -1;
    void *LW_virtual; //0xFF200000

    if ((fd = open_physical(fd)) == -1)
        return (-1);
        
    if  (!(LW_virtual = map_physical(fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)))
        return (-1);

    HEX3_HEX0_ptr = (int *) (LW_virtual + HEX3_HEX0_BASE);
    HEX5_HEX4_ptr = (int *) (LW_virtual + HEX5_HEX4_BASE);
    KEY_ptr = (int *) (LW_virtual + KEY_BASE);

    ts.tv_sec = 0;
    ts.tv_nsec = 250000000; // 0.25 ms

    // Encode an entry to each character's HEX display
    for (int i = 0; i < LEN; i++)
    {
        code[i] = seg_table[message[i] & 0x7F];
    }   

    while(!stop)
    {
        // Scrolling Behaviour
        for (int i = 0; i < DISPLAY; i++)
        {
            w[i] = code[(pos + i) % LEN];
        }

        *HEX5_HEX4_ptr = (w[0] << 8)  | 
                         (w[1]);

        *HEX3_HEX0_ptr = (w[2] << 24) |
                         (w[3] << 16) |
                         (w[4] << 8)  |
                         (w[5]);
        
        if (scroll)
        {
            pos = (pos + 1) % LEN;
        }

        if (*(KEY_ptr + 3))
        {
            scroll ^= 1;
            *(KEY_ptr) = 0xF; // Clearing thhe KEY flag
        }

        nanosleep(&ts, NULL);
        if (time (NULL) - start_time > 12) stop = 1;

    }

    *HEX3_HEX0_ptr = 0;			// turn off HEX display
    *HEX5_HEX4_ptr = 0;
	unmap_physical (LW_virtual, LW_BRIDGE_SPAN);	// release the physical-memory mapping
	close_physical (fd);	// close /dev/mem

    return 0;
}

void catchSIGINT(int stop_flag)
{
    stop = 1;
}