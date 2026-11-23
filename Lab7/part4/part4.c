#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include "../include/accel_wrappers.h"

#define TAP_HOLD_LOOPS 100          // 100 * 5 ms = 0.5 s
#define COLS 80             // ASCII terminal size
#define ROWS 24
#define FULL_SCALE 1000.0   // mg of tilt that moves the bubble to the edge (1 g)
#define X_SIGN  1           // flip these if the bubble moves the wrong way
#define Y_SIGN -1

/* Keys while running:  c calibrate   f toggle full resolution   g cycle range (2/4/8/16 g)
 *                      r cycle data rate   +/- change smoothing alpha   i re-init   d print device ID
 * Command-line (optional): ./part3 [full range [rate]]   e.g.  ./part3 1 2 25 */

volatile sig_atomic_t stop;
void catchSIGINT(int signum) { stop = 1; }

static const int   ranges[] = {2, 4, 8, 16};
static const float rate_hz[] = {100, 50, 25, 12.5f, 6.25f};
#define NR (int)(sizeof(rate_hz) / sizeof(rate_hz[0]))

static int full = 0, range_i = 3, rate_i = 3;   // ADXL345_Init() default: 16 g, 10-bit, 12.5 Hz
static double alpha = 0.85;
static struct termios old_term;

static void put(int row, int col, char c) { printf("\033[%d;%dH%c", row, col, c); }

static void status(void) {
    printf("\033[%d;1H\033[K %s, +/-%d g, %.2f Hz, alpha = %.2f | c f g r + - i d",
        ROWS, full ? "full-res" : "10-bit", ranges[range_i], rate_hz[rate_i], alpha);
}

static void restore_term(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    printf("\033[%d;1H\033[?25h\n", ROWS);
}

int main(int argc, char *argv[]) {
    int ready, tap_detect, x, y, z, mg, i;
    double av_x = 0, av_y = 0;
    int col = COLS / 2, row = ROWS / 2, old_col = col, old_row = row;
    struct termios raw;
    char ch;
    int tap_hold = 0, last_tap = 0;

    signal(SIGINT, catchSIGINT);
    if (!accel_open()) return -1;

    accel_init();                                   // known starting state
    if (argc >= 3) {                                // optional: format and rate
        full = atoi(argv[1]);
        for (i = 0; i < 4; i++) if (ranges[i] == atoi(argv[2])) range_i = i;
        accel_format(full, ranges[range_i]);
    }
    if (argc >= 4) { accel_rate(atof(argv[3])); }

    // non-blocking, unbuffered keyboard (Ctrl+C still works)
    tcgetattr(STDIN_FILENO, &old_term);
    raw = old_term;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    printf("\033[2J\033[?25l");
    status();

    while (!stop) {   // RHH
        //printf("\ntest\n");
        if (!accel_read(&ready, &x, &y, &z, &mg)) break;
        //printf("%d\n", ready);
        tap_detect = ready & 0x3; // Select HH -> RHH & 0b011
        ready = ready >> 2;       // Select R -> RHH >> 2
        //printf("\033[1;1H\033[K R%1d HH = %2d", ready, tap_detect);
        if (tap_detect) { last_tap = tap_detect; tap_hold = TAP_HOLD_LOOPS; }
        else if (tap_hold > 0) tap_hold--;

        if (ready) {
            int ax = x * mg, ay = y * mg, az = z * mg;      // mg

            // running average: av = av * alpha + new * (1 - alpha)
            av_x = av_x * alpha + ax * (1.0 - alpha);
            av_y = av_y * alpha + ay * (1.0 - alpha);

            col = COLS / 2 + (int)(X_SIGN * av_x / FULL_SCALE * (COLS / 2 - 2));
            row = ROWS / 2 + (int)(Y_SIGN * av_y / FULL_SCALE * (ROWS / 2 - 3));
            if (col < 1) col = 1;
            if (col > COLS) col = COLS;
            if (row < 2) row = 2;
            if (row > ROWS - 1) row = ROWS - 1;

            printf("\033[1;1H\033[K  %-11s | X = %5d  Y = %5d  Z = %5d  (mg)",
                    tap_hold ? ((last_tap & 2) ? "DOUBLE TAP" : "SINGLE TAP") : "",
                    ax, ay, az);

            if (col != old_col || row != old_row) {
                put(old_row, old_col, ' ');
                old_col = col; old_row = row;
            }
            put(row, col, 'O');
            fflush(stdout);
        }

        // keyboard commands -> wrappers -> write() to /dev/accel
        /*
        c - Zero/Tare the accelorometer
        f - Change from 10-bit resolution to variable resolution
        g - Select through the resolution range
        r - Select through output data rate
        i - Re-Initialize accelerometer to default
        d - Should print 0xE5 (ADXL345 ID), if not i2c is broken
        + - Increase alpha, makes a steadier but laggy bubble
        - - Decrease alpha, bubble is responsive but jumpy
        */

        while (read(STDIN_FILENO, &ch, 1) == 1) {
            switch (ch) {
                case 'c': accel_calibrate(); av_x = av_y = 0; break;
                case 'f': full = !full; accel_format(full, ranges[range_i]); break;
                case 'g': range_i = (range_i + 1) % 4; accel_format(full, ranges[range_i]); break;
                case 'r': rate_i = (rate_i + 1) % NR; accel_rate(rate_hz[rate_i]); break;
                case '+': if (alpha < 1) alpha += 0.05; break;
                case '-': if (alpha > 0.05) alpha -= 0.05; break;
                case 'i': accel_init(); full = 0; range_i = 3; rate_i = 3; break;
                case 'd': accel_device(); break;
            }
            status();
            fflush(stdout);
        }
        usleep(5000);
    }

    restore_term();
    accel_close();
    return 0;
}
