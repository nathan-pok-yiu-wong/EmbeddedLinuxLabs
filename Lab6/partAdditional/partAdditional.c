#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <termios.h>
#include <stdlib.h>
#include <time.h>
#include "../include/defines.h"

volatile sig_atomic_t running = 1;

#define video_BYTES 8
#define BYTES 256
#define KEY_SIZE 256          /* was 3: read() below can return up to BYTES-1 bytes */
#define SW_SIZE 256

#define TEXT_COLS 80
#define TEXT_ROWS 60

int screen_x, screen_y;

static struct termios orig_term;
struct pollfd p_fd = { .fd = STDIN_FILENO, .events = POLLIN };

static void restore_terminal(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
}

static void enable_raw_mode(void)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &orig_term);
    atexit(restore_terminal);
    raw = orig_term;
    raw.c_lflag &= ~(ICANON | ECHO);   /* ISIG stays on, so Ctrl-C still works */
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static void put_char(int fd, int x, int y, char c)
{
    char cmd[32];
    int len = snprintf(cmd, sizeof(cmd), "text %d,%d %c", x, y, c);
    write(fd, cmd, len);
}

static void send_cmd(int fd, const char *cmd)
{
    write(fd, cmd, strlen(cmd));
}

void flush_keys(int key_fd)
{
    char junk[BYTES];
    while (read(key_fd, junk, BYTES) != 0);
}

void catchSIGINT(int signum)
{
    running = 0;
}

int main(int argc, char *argv[])
{
    int video_FD, KEY_FD, SW_FD;
    char video_buffer[video_BYTES];
    int KEY_read, ret_val;
    char KEY_buff[KEY_SIZE];
    int cx = 0, cy = 0;

    signal(SIGINT, catchSIGINT);
    srand(time(NULL));

    if ((video_FD = open("/dev/video", O_RDWR)) == -1) {
        printf("Error opening /dev/video: %s\n", strerror(errno));
        return -1;
    }
    if ((KEY_FD = open("/dev/IntelFPGAUP/KEY", O_RDONLY)) == -1) {
        printf("Error opening /dev/IntelFPGAUP/KEY: %s\n", strerror(errno));
        return -1;
    }
    if ((SW_FD = open("/dev/IntelFPGAUP/SW", O_RDONLY)) == -1) {
        printf("Error opening /dev/IntelFPGAUP/SW: %s\n", strerror(errno));
        return -1;
    }

    while (read(video_FD, video_buffer, video_BYTES))
        ;
    sscanf(video_buffer, "%3d %3d", &screen_x, &screen_y);

    flush_keys(KEY_FD);

    enable_raw_mode();
    send_cmd(video_FD, "erase");
    printf("Type to draw on VGA. Press ESC to quit.\n");

    while (running)
    {
        /* wait up to 16 ms for a key press */
        if (poll(&p_fd, 1, 16) > 0 && (p_fd.revents & POLLIN))
        {
            char c;
            if (read(STDIN_FILENO, &c, 1) == 1)
            {
                if (c == 27) { // Use ESC to end program
                    running = 0;
                    break;
                }
                else if (c == '\n' || c == '\r') {
                    cx = 0;
                    if (++cy >= TEXT_ROWS) cy = 0;
                }
                else if (c == 127 || c == '\b') { // delete and back
                    if (cx > 0) cx--;
                    else if (cy > 0) { cy--; cx = TEXT_COLS - 1; }
                    put_char(video_FD, cx, cy, ' ');
                }
                else if (c >= 32 && c < 127) { // Accepted characters
                    put_char(video_FD, cx, cy, c);
                    if (++cx >= TEXT_COLS) {
                        cx = 0;
                        if (++cy >= TEXT_ROWS) cy = 0;
                    }
                }
            }
        }

        KEY_read = 0;

        while ((ret_val = read(KEY_FD, KEY_buff, BYTES - 1)) != 0)
            KEY_read += ret_val;
        KEY_buff[KEY_read] = '\0';

        send_cmd(video_FD, "sync");
    }

    send_cmd(video_FD, "erase");
    send_cmd(video_FD, "clear");
    send_cmd(video_FD, "sync");

    close(video_FD);
    close(KEY_FD);
    close(SW_FD);
    return 0;
}