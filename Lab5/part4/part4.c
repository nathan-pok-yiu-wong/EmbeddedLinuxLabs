#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "defines.h"

/**  your part 4 user code here  **/
// int min_x, min_y, max_x, max_y;

void get_win_dims();
void wait_for_vsync();
void clear_win(void);
void plot_pix(int x, int y, int colour, const char *c);
void draw_line(int, int, int, int, char);
void* render_fn(void* arg);

static volatile int running = 1;

// Struct to hold position and direction of travel of a vertex.
struct vertex{
    int col;
    int row;
    int dir_v;
    int dir_h;
    int colour;
};

int main(void)
{
    // Get window dimentions
    struct winsize win;
    ioctl(0, TIOCGWINSZ, &win);

    clear_win( );
    printf ("\e[?25l");                             // hides the cursor

    fflush (stdout);

    pthread_t render_thread;                        // Create seperate thread for handling rendering
    pthread_create(&render_thread, NULL, render_fn, &win);

    (void) getchar ();                              // wait for user to press return

    running = 0;                                    // End thread
    pthread_join(render_thread, NULL);

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

    srand(time(NULL));                                  // Random seed
    int num_vertices = (rand() % 4) + 4;                // Randomize number of vertices
    struct vertex vertices[num_vertices];

    int iter;
    for (iter = 0; iter < num_vertices; iter++)         // Populate with random vertex coordinates & directions
    {
        vertices[iter].col = left + rand() % (right - left + 1);
        vertices[iter].row = top  + rand() % (bottom - top + 1);
        vertices[iter].dir_h = (rand() % 3) - 1;        // Can start moving in either direction or not at all
        vertices[iter].dir_v = (rand() % 3) - 1;
        vertices[iter].colour = (rand() % 7) + 30;
    }

    struct timespec delay = { 0, 500 * 1000 * 1000 };    // 500 ms per frame

    while (running)
    {
        for (iter = 0; iter < num_vertices; iter++)                                     // Update vertices
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

        for (iter = 0; iter < num_vertices; iter++)                                 // Draw new Lines
        {
            int next = (iter + 1) % num_vertices;
            draw_line(vertices[iter].col, vertices[iter].row, vertices[next].col, vertices[next].row, vertices[iter].colour);
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
