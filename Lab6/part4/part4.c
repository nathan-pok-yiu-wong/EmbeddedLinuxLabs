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
#define NUM_BOXES 8
#define BOX_SIZE 6


int screen_x, screen_y;

typedef struct
{
    int x, y;
    int dx, dy; // next one
    int color;
} Box;

int palette[] = { 0xF800, 0xFFE0, 0x07FF, 0xF81F, 0xFFFF, 0xFD20, 0x001F };
#define PALETTE_SIZE (int)(sizeof(palette) / sizeof(palette[0]))

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
    Box boxes[NUM_BOXES];

    // Generate a random seed
    srand(time(NULL));

    // Open the character device driver
    if ((video_FD = open("/dev/video", O_RDWR)) == -1) {
        printf("Error opening /dev/video: %s\n", strerror(errno));
        return -1;
    }

    while (read (video_FD, video_buffer, video_BYTES))
        ;    // read the video port until EOF
    sscanf (video_buffer, "%3d %3d", &screen_x, &screen_y);

    // Generate a random starting position and direction

    int i;
    for (i = 0; i < NUM_BOXES; i++)
    {
        boxes[i].x = rand() % (screen_x - BOX_SIZE);
        boxes[i].y = rand() % (screen_y - BOX_SIZE);
        boxes[i].dx = rand() % 2 ? 1 : -1;
        boxes[i].dy = rand() % 2 ? 1 : -1;
        boxes[i].color = palette[rand() % PALETTE_SIZE];
    }

    printf("Press 'enter' to stop");

    while (running)
    {
        if (poll(&p_fd, 1, 0) > 0)
        {
            running = 0;
            break;
        }

        // Clear pixel buffer
        sprintf(command, "clear");
        write(video_FD, command, strlen(command));

        // Send command to draw lines between boxes
        for (i = 0; i < NUM_BOXES - 1; i++)
        {
            sprintf(command, "line %d,%d %d,%d %X\n",
                boxes[i].x + BOX_SIZE / 2,
                boxes[i].y + BOX_SIZE / 2,
                boxes[i+1].x + BOX_SIZE / 2, // From one box to another
                boxes[i+1].y + BOX_SIZE / 2,
                boxes[i].color);
            write (video_FD, command, strlen(command));
        }

        // Send command to draw boxes
        for (i = 0; i < NUM_BOXES; i++)
        {
            sprintf(command, "box %d,%d %d,%d %X\n",
                boxes[i].x,
                boxes[i].y,
                boxes[i].x + BOX_SIZE - 1, // Draw from one edge to other edge of box
                boxes[i].y + BOX_SIZE - 1,
                boxes[i].color);
            write (video_FD, command, strlen(command));
        }

        // CLOSE THE BOX
        sprintf(command, "line %d,%d %d,%d %X\n",
            boxes[0].x + BOX_SIZE / 2,
            boxes[0].y + BOX_SIZE / 2,
            boxes[NUM_BOXES - 1].x + BOX_SIZE / 2, // From one box to another
            boxes[NUM_BOXES - 1].y + BOX_SIZE / 2,
            boxes[NUM_BOXES - 1].color);
        write (video_FD, command, strlen(command));

        // Calculate next frame direction
        for (i = 0; i < NUM_BOXES; i++)
        {
            boxes[i].x += boxes[i].dx;
            boxes[i].y += boxes[i].dy;

            // Edge detection
            if (boxes[i].x <= 0)
            {
                boxes[i].x = 0;
                boxes[i].dx = 1;
            }

            if (boxes[i].y <= 0)
            {
                boxes[i].y = 0;
                boxes[i].dy = 1;
            }

            if (boxes[i].x >= (screen_x - BOX_SIZE))
            {
                boxes[i].x = (screen_x - BOX_SIZE);
                boxes[i].dx = -1;
            }

            if (boxes[i].y >= (screen_y - BOX_SIZE))
            {
                boxes[i].y = (screen_y - BOX_SIZE);
                boxes[i].dy = -1;
            }
        }

        // Send sync command
        sprintf(command, "sync");
        write(video_FD, command, strlen(command));
    }

    printf ("\nExiting sample solution program\n");

    close (video_FD);
    return 0;
}