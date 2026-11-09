#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <limits.h>
#include <semaphore.h>
#include "defines.h"

/**  your part 5 user code here  **/

#define BYTES 256   // Max characters to read from /dev/chardev
#define SUCCESS 0
#define KEY_SIZE 3  // 1 for val, 1 for /n , 1 for null-term
#define SW_SIZE 5   // 3 for val, 1 for /n, 1 for null-term
#define MAX_VERT 16
#define MIN_VERT 2
#define MAX_FPS 60
#define MIN_FPS 5

void get_win_dims();
void wait_for_vsync();
void clear_win(void);
void plot_pix(int x, int y, int colour, const char *c);
void draw_line(int, int, int, int, char);
void* render_fn(void* arg);
void flush_keys(int key_fd);

sem_t mutex;

volatile sig_atomic_t running = 1;
volatile sig_atomic_t draw_lines = 1;
static volatile int fps = 15;
static volatile int num_vertices;

// Struct to hold position and direction of travel of a vertex.
struct vertex{
    int col;
    int row;
    int dir_v;
    int dir_h;
    int colour;
};
// Vertex initializer helper function
static void init_vertex(struct vertex *v, int top, int bottom, int left, int right)
{
    v->col    = left + rand() % (right - left + 1);
    v->row    = top  + rand() % (bottom - top + 1);
    v->dir_h  = (rand() % 3) - 1;
    v->dir_v  = (rand() % 3) - 1;
    v->colour = (rand() % 7) + 30;
}

// STDIN_FILENO tells it to poll standard input
// POLLIN tells it to inform us when there is data (aka when "enter" is pressed)
struct pollfd p_fd = { .fd = STDIN_FILENO, .events = POLLIN };

void catchSIGINT(int signum){
    running = 0;
}

int main(void)
{
    sem_init(&mutex, 0, 1);

    srand(time(NULL));                              // Random seed
    num_vertices = (rand() % 4) + 4;                // Randomize number of vertices

    int KEY_FD, SW_FD; // File descriptors for chardevs

    char KEY_buff[KEY_SIZE], SW_buff[SW_SIZE];
    int ret_val, KEY_read, SW_read;

    // Open device drivers
    if ((KEY_FD = open("/dev/IntelFPGAUP/KEY", O_RDONLY)) == -1) {printf("Error opening /dev/IntelFPGAUP/KEY: %s\n", strerror(errno)); return -1;}
    if ((SW_FD = open("/dev/IntelFPGAUP/SW", O_RDONLY)) == -1 ) {printf("Error opening /dev/IntelFPGAUP/SW: %s\n", strerror(errno)); return -1;}

    flush_keys(KEY_FD);

    // Get window dimentions
    struct winsize win;
    ioctl(0, TIOCGWINSZ, &win);

    clear_win( );
    printf ("\e[?25l");                             // hides the cursor

    fflush (stdout);

    pthread_t render_thread;                        // Create seperate thread for handling rendering
    pthread_create(&render_thread, NULL, render_fn, &win);

    while (running)                                 // wait for user to press return
    {

        if (poll(&p_fd, 1, 0) > 0) running = 0;

        KEY_read = 0;
        while ((ret_val = read (KEY_FD, KEY_buff, BYTES - 1)) != 0)
            KEY_read += ret_val;	// read the driver until EOF
        KEY_buff[KEY_read] = '\0';	// NULL terminate

        if (strtol(KEY_buff, NULL, 16) == 1)
            fps += fps < MAX_FPS ? 5 : 0;    // Increase fps by 5 until max 60
        if (strtol(KEY_buff, NULL, 16) == 2)
            fps -= fps > MIN_FPS ? 5 : 0;     // Decrease fps by 5 until min 5

        if (strtol(KEY_buff, NULL, 16) == 4)
        {
            sem_wait(&mutex);
            num_vertices += num_vertices < MAX_VERT ? 1 : 0;    // Increase num_vert until max
            sem_post(&mutex);
        }
        if (strtol(KEY_buff, NULL, 16) == 8)
        {
            sem_wait(&mutex);
            num_vertices -= num_vertices > MIN_VERT ? 1 : 0;     // Decrease num_vert until min
            sem_post(&mutex);
        }

        SW_read = 0;
        while ((ret_val = read (SW_FD, SW_buff, BYTES - 1)) != 0)
            SW_read += ret_val;	    // read the driver until EOF
        SW_buff[SW_read] = '\0';	// NULL terminate

        if (strtol(SW_buff, NULL, 16) == 0)
            draw_lines = 1;
        else
            draw_lines = 0;

        usleep (5000); // Button Debounce
    }

    pthread_join(render_thread, NULL);
    sem_destroy(&mutex);

    printf("\e[0m\e[%d;1H\e[?25h", win.ws_row);     // reset colour, cursor to bottom, show cursor
    fflush(stdout);
    return 0;
}

void* render_fn(void* arg)
{
    struct winsize *win = arg;
    int top = 1;
    int bottom = win->ws_row - 2;                       // keep the last row for the message
    int left = 1;
    int right = win->ws_col;
    int active_num_vertices;

    sem_wait(&mutex);
    active_num_vertices = num_vertices;
    sem_post(&mutex);

    struct vertex vertices[MAX_VERT];

    int iter;
    for (iter = 0; iter < active_num_vertices; iter++)         // Populate with random vertex coordinates & directions
    {
        init_vertex(&vertices[iter], top, bottom, left, right);
    }

    while (running)
    {
        struct timespec delay = { 0, 1000/fps * 1000 * 1000 };  // Update at set FPS

        sem_wait(&mutex);
        if (active_num_vertices < num_vertices)
        {
            int diff = num_vertices - active_num_vertices;
            for (iter = 0; iter < diff; iter++) // Update Array
            {
                init_vertex(&vertices[num_vertices + iter], top, bottom, left, right);
            }
        }

        active_num_vertices = num_vertices; // Update local value
        sem_post(&mutex);

        for (iter = 0; iter < active_num_vertices; iter++)                                     // Update vertices
        {
            struct vertex *v = &vertices[iter];
            v->col += v->dir_h;
            v->row += v->dir_v;

            if      (v->row >= bottom) { v->row = bottom; v->dir_v = -ABS(v->dir_v); }  // Check bounds
            else if (v->row <= top)    { v->row = top;    v->dir_v =  ABS(v->dir_v); }
            if      (v->col >= right)  { v->col = right;  v->dir_h = -ABS(v->dir_h); }
            else if (v->col <= left)   { v->col = left;   v->dir_h =  ABS(v->dir_h); }
        }

        printf("\e[2J");
        printf("\e[0m\e[%d;1HPress Enter to Exit.", win->ws_row);                   // message on the bottom row

        for (iter = 0; iter < active_num_vertices; iter++)                                 // Draw new Lines
        {
            int next = (iter + 1) % active_num_vertices;
            if (draw_lines)
                draw_line(vertices[iter].col, vertices[iter].row, vertices[next].col, vertices[next].row, vertices[iter].colour);
            else
                plot_pix(vertices[iter].col, vertices[iter].row, vertices[iter].colour, "\u2588");
        }

        fflush(stdout);

        nanosleep(&delay, NULL);
    }
    return NULL;
}

void clear_win()
{
    printf("\e[2J");
    fflush(stdout);
}

void draw_line(int x0, int y0, int x1, int y1, char colour)
{
    // Vertical line: x1 == x0
    if (x1 == x0)
    {
        int y_;
        int y_start = y1 > y0 ? y0 : y1;
        int y_end   = y1 > y0 ? y1 : y0;
        for (y_ = y_start; y_ <= y_end; y_++)
            plot_pix(x0, y_, colour, "\u2588");
        return;
    }

    float slope  = (float)(y1 - y0) / (x1 - x0);

    float step = ABS(slope) > 1 ? 1.0f / ABS(slope) : 1.0f;    // always positive
    float dir   = x1 > x0 ? 1.0f : -1.0f;                      // direction of travel along x
    float range = x1 > x0 ? x1 - x0 : x0 - x1;
    int steps   = (int)(range / step + 0.5f);                  // number of samples after the first

    int i;
    for (i = 0; i <= steps; i++)
    {
        float sample_pt = dir * i * step;                      // offset from x0, no accumulated drift
        int x_pt = (int)(x0 + sample_pt + 0.5f);               // round to the nearest cell
        int y_pt = (int)(y0 + slope * sample_pt + 0.5f);
        plot_pix(x_pt, y_pt, colour, "\u2588");
    }
}

void plot_pix(int x, int y, int colour, const char *c)
{
    printf("\x1b[%dm\x1b[%d;%dH%s", colour, y, x, c);
}

void flush_keys(int key_fd)
{
    char junk[BYTES];
    while (read(key_fd, junk, BYTES) != 0)
        ;
}
