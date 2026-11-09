#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "defines.h"

/**  your part 3 user code here  **/
// int min_x, min_y, max_x, max_y;

void get_win_dims();
void wait_for_vsync();
void clear_win(void);
void plot_pix(int x, int y, int colour, const char *c);
void draw_line(int, int, int, int, char);
void* render_fn(void* arg);

static volatile int running = 1;

int main(void)
{
    // Get window dimentions
    struct winsize win;
    ioctl(0, TIOCGWINSZ, &win);

    clear_win( );
    printf ("\e[?25l");                                         // hides the cursor
        printf("\e[%d;1HPress Enter to Exit.", win.ws_row);     // message on the bottom row
    fflush (stdout);

    pthread_t render_thread;
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
    int bottom = win->ws_row - 2;                     // keep the last row for the message
    int row = top;
    int dir = 1;

    struct timespec delay = { 0, 60 * 1000 * 1000 };   // 60 ms per frame

    while (running)
    {
        printf("\e[%d;1H\e[2K", row - dir);            // erase the previous frame's row
        draw_line(4, row, win->ws_col - 4, row, GREEN);

        fflush(stdout);

        nanosleep(&delay, NULL);

        row += dir;
        if (row >= bottom || row <= top)
            dir = -dir;
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
    // Check for edge case where x1==x0, ie straight line up
    if (x1 == x0)
    {
        int y_;
        for (y_ = (y1 > y0 ? y0 : y1); y_ < (y1 > y0 ? y1 : y0); y_++) // loop through all points bottom to top
        {
            plot_pix(x0, y_, GREEN, "\u2588");
        }
        return;
    }

    float slope = (float)(y1-y0)/(x1-x0);

    float sample_pt;
    float calc_res = slope > 1 ? (1.0/slope) : 1.0; // how often do we need to sample the function with given slope to not skip a pixel

    int y_pt, x_pt;
    float range = ABS(x1-x0);

    for (sample_pt = 0; sample_pt < range; sample_pt += calc_res)
    {
        // Line follows Y=MX+B is y_pt=slope*x_pt+y0
        x_pt = (int)sample_pt + x0; // Floor sample_pt to int via cast, current x coordinate
        y_pt = (int)(slope*sample_pt + y0); // Calc y coordiate and floor to int via cast
        plot_pix(x_pt, y_pt, GREEN, "\u2588");
    }
}

void plot_pix(int x, int y, int colour, const char *c)
{
    printf("\x1b[%dm\x1b[%d;%dH%s", colour, y, x, c);
}
