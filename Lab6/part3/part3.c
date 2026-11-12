#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include "../include/defines.h"

/**  your part 3 user code here  **/
volatile sig_atomic_t running = 1;

#define video_BYTES 8   // number of characters to read from /dev/video

int screen_x, screen_y;

// Catch 'enter' to close program
struct pollfd p_fd = { .fd = STDIN_FILENO, .events = POLLIN };
void catchSIGINT(int signum){
    running = 0;
}

/* This code uses the following character device driver: /dev/video. It draws several lines
 * on the VGA display, and then exits. */
int main(int argc, char *argv[])
{
    int video_FD;                       // file descriptor
    char video_buffer[video_BYTES];  // buffer for video char data
    char command[64];

    // Open the character device driver
    if ((video_FD = open("/dev/video", O_RDWR)) == -1) {
        printf("Error opening /dev/video: %s\n", strerror(errno));
        return -1;
    }

    while (read (video_FD, video_buffer, video_BYTES))
        ;    // read the video port until EOF
    sscanf (video_buffer, "%3d %3d", &screen_x, &screen_y);

    printf("Press 'enter' to stop");

    int x1, x2, y1, y2;
    int top = 20;
    int bottom = screen_y - 20;
    int dir = -1;

    x1 = 20;
    x2 = screen_x - 20;
    y1 = y2 = top;

    while (running)
    {
        // Send command to write line
        sprintf(command, "line %d,%d %d,%d %X\n", x1, y1, x2, y2, GREEN);
        write (video_FD, command, strlen(command));

        //Update y1, y2, and check bounds
        y1 += dir;
        y2 += dir;

        if (y1 == bottom || y1 == top)
            dir = -dir;

        // Send sync command
        sprintf(command, "sync");
        write(video_FD, command, strlen(command));
    }

    printf ("\nExiting sample solution program\n");

    close (video_FD);
    return 0;
}
