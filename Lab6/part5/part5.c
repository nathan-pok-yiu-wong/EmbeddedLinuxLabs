#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include "../include/defines.h"

#include <stdlib.h>
#include <time.h>
/**  your part 6 user code here  **/
volatile sig_atomic_t running = 1;

#define video_BYTES 8   // number of characters to read from /dev/video
#define NUM_BOXES 8
#define BOX_SIZE 6
#define MAX_BOXES 16
#define MIN_BOXES 2
#define MAX_SPEED 16
#define MIN_SPEED 1

#define BYTES 256
#define KEY_SIZE 3
#define SW_SIZE 5

#define LINE_COLOR 0x07E0

// Green = 0x07E0
// RED, YELLOW, CYAN, MAGENTA, WHITE, ORANGE, BLUE
int palette[] = { 0xF800, 0xFFE0, 0x07FF, 0xF81F, 0xFFFF, 0xFD20, 0x001F };
#define PALETTE_SIZE (int)(sizeof(palette) / sizeof(palette[0]))

int screen_x, screen_y;

typedef struct
{
    int x, y;
    int dx, dy; // next one
    int color;
} Box;

void flush_keys(int key_fd)
{
    char junk[BYTES];
    while (read(key_fd, junk, BYTES) != 0);
}

static void init_box(Box *b)
{
    b->x     = rand() % (screen_x - BOX_SIZE + 1);
    b->y     = rand() % (screen_y - BOX_SIZE + 1);
    b->dx    = (rand() % 2) ? 1 : -1;
    b->dy    = (rand() % 2) ? 1 : -1;
    b->color = palette[rand() % PALETTE_SIZE];
}

// Catch 'enter' to close program
struct pollfd p_fd = { .fd = STDIN_FILENO, .events = POLLIN };
void catchSIGINT(int signum){
    running = 0;
}

/* This code uses the following character device driver: /dev/video. It draws several lines
 * on the VGA display, and then exits. */
int main(int argc, char *argv[])
{
    int video_FD, KEY_FD, SW_FD;     // file descriptor
    char video_buffer[video_BYTES];  // buffer for video char data
    char command[64];
    Box boxes[MAX_BOXES];

    int KEY_read, SW_read, ret_val;
    char KEY_buff[KEY_SIZE], SW_buff[SW_SIZE];

    int speed = 4;
    int num_boxes = NUM_BOXES;

    int draw_lines = 1;

    signal(SIGINT, catchSIGINT);

    // Generate a random seed
    srand(time(NULL));

    // Open the character device driver
    if ((video_FD = open("/dev/video", O_RDWR)) == -1) {
        printf("Error opening /dev/video: %s\n", strerror(errno));
        return -1;
    }

    if ((KEY_FD = open("/dev/IntelFPGAUP/KEY", O_RDONLY)) == -1) {
        printf("Error opening /dev/IntelFPGAUP/KEY: %s\n", strerror(errno)); return -1;
    }
    if ((SW_FD = open("/dev/IntelFPGAUP/SW", O_RDONLY)) == -1) {
        printf("Error opening /dev/IntelFPGAUP/SW: %s\n", strerror(errno)); return -1;
    }

    while (read (video_FD, video_buffer, video_BYTES))
        ;    // read the video port until EOF
    sscanf (video_buffer, "%3d %3d", &screen_x, &screen_y);

    flush_keys(KEY_FD);

    // Generate a random starting position and direction
    int i;
    for (i = 0; i < num_boxes; i++)
    {
        init_box(&boxes[i]);
    }

    printf("Press 'enter' to stop");

    while (running)
    {
        if (poll(&p_fd, 1, 0) > 0)
        {
            running = 0;
            break;
        }

        KEY_read = 0;
        while ((ret_val = read (KEY_FD, KEY_buff, BYTES - 1)) != 0)
            KEY_read += ret_val;	// read the driver until EOF
        KEY_buff[KEY_read] = '\0';	// NULL terminate

        if (strtol(KEY_buff, NULL, 16) == 1)
            speed += speed < MAX_SPEED ? 1 : 0;    // Increase speed by 5 until max 60
        if (strtol(KEY_buff, NULL, 16) == 2)
            speed -= speed > MIN_SPEED ? 1 : 0;     // Decrease speed by 5 until min 5

        if (strtol(KEY_buff, NULL, 16) == 4)
            if (num_boxes < MAX_BOXES)
            {
                init_box(&boxes[num_boxes]);
                num_boxes++;
            }

        if (strtol(KEY_buff, NULL, 16) == 8)
        {
            num_boxes -= num_boxes > MIN_BOXES ? 1 : 0;
        }

        SW_read = 0;
        while ((ret_val = read (SW_FD, SW_buff, BYTES - 1)) != 0)
            SW_read += ret_val;	    // read the driver until EOF
        SW_buff[SW_read] = '\0';	// NULL terminate

        if (strtol(SW_buff, NULL, 16) == 0)
            draw_lines = 1;
        else
            draw_lines = 0;


        // Speed 1-12, we speed up fps
        int fps = (speed <= 12) ? 5 * speed : 60;

        // Speed 13-16, we make boxes move faster
        int step = (speed <= 12) ? 1 : speed - 11;
        struct timespec delay = { 0, 1000000000 / fps};

        // Moves boxes
        for (i = 0; i < num_boxes; i++)
        {
            // Step
            boxes[i].x += boxes[i].dx * step;
            boxes[i].y += boxes[i].dy * step;

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

        // Draw to back buffer
        sprintf(command, "clear");
        write(video_FD, command, strlen(command));

        // Send command to draw lines between boxes
        if (draw_lines)
        {
            for (i = 0; i < num_boxes - 1; i++)
            {
                sprintf(command, "line %d,%d %d,%d %X\n",
                    boxes[i].x + BOX_SIZE / 2,
                    boxes[i].y + BOX_SIZE / 2,
                    boxes[i+1].x + BOX_SIZE / 2, // From one box to another
                    boxes[i+1].y + BOX_SIZE / 2,
                    boxes[i].color);
                write (video_FD, command, strlen(command));
            }
        }

        // Send command to draw boxes
        for (i = 0; i < num_boxes; i++)
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
            boxes[i].x + BOX_SIZE / 2,
            boxes[i].y + BOX_SIZE / 2,
            boxes[num_boxes - 1].x + BOX_SIZE / 2, // From one box to another
            boxes[num_boxes - 1].y + BOX_SIZE / 2,
            boxes[num_boxes - 1].color);
        write (video_FD, command, strlen(command));

        // vsync
        sprintf(command, "sync");
        write(video_FD, command, strlen(command));
        nanosleep(&delay, NULL);
    }

    // clear
    sprintf(command, "clear");
    // Send sync command
    sprintf(command, "sync");
    write(video_FD, command, strlen(command));

    close (video_FD);
    close(KEY_FD);
    close(SW_FD);
    return 0;
}
