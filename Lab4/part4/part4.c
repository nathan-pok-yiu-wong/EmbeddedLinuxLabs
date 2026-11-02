#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <poll.h> // Include checking logic to poll user input while triggering the end of game when the timer hits 0


/**  your part 4 user code here  **/
#define BYTES 256        // max number of characters to read from /dev/chardev
#define SUCCESS 0
#define LEDR_SIZE 5        // 3 for value, 1 for \n, 1 for null terminator
#define KEY_SIZE 3       // 1 for value, 1 for \n, 1 for null terminator
#define SW_SIZE 5        // 3 for value, 1 for \n, 1 for null terminator


// Some function prototypes
void get_time(int file_descriptor, char buf[], int *minutes, int *seconds, int *hundreths);
int get_equation(int score);
void flush_keys(int key_fd);
int convert_time_to_seconds(int time);

volatile sig_atomic_t stop;
void catchSIGINT(int signum){
    stop = 1;
}

// STDIN_FILENO tells it to poll standard input
// POLLIN tells it to inform us when there is data (aka when "enter" is pressed)
struct pollfd p_fd = { .fd = STDIN_FILENO, .events = POLLIN };

int main(int argc, char *argv[]){

	int KEY_FD, SW_FD, LEDR_FD, STOPWATCH_FD;	// file descriptor

    // buffers for different I/O data
  	char KEY_buffer[KEY_SIZE], SW_buffer[SW_SIZE], LEDR_buffer[LEDR_SIZE], STOPWATCH_buffer[BYTES], guess_buffer[BYTES];
	int ret_val, KEY_read, SW_read, STOPWATCH_read;
    int SW_value = 0;
    int run = 0;
    int time_limit = 0, time_limit_minutes = 0, time_limit_seconds = 0, time_limit_hundreths = 0;
    int total_time = 0;
    int score = 0; // Used to keep track of score and also increment difficulty of math problems
    int answer, guess;

    srand(time(NULL)); // This will generate a randomized seed for us to generate math problems

  	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
  	signal(SIGINT, catchSIGINT);
    
    // Open the KEY device driver for read
	if ((KEY_FD = open("/dev/KEY", O_RDONLY)) == -1)
	{   
		printf("Error opening /dev/KEY: %s\n", strerror(errno));
		return -1;
	}

    // Open the SW device driver for read
	if ((SW_FD = open("/dev/SW", O_RDONLY)) == -1)
	{   
		printf("Error opening /dev/SW: %s\n", strerror(errno));
		return -1;
	}

	// Open the LEDR device driver for write
	if ((LEDR_FD = open("/dev/LEDR", O_WRONLY)) == -1)
	{
		printf("Error opening /dev/LEDR: %s\n", strerror(errno));
		return -1;
    }

	// Open the stopwatch device driver for read/write
	if ((STOPWATCH_FD = open("/dev/stopwatch", O_RDWR)) == -1)
	{
		printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
		return -1;
    }

    // Clear any latched button presses from previous startup
    flush_keys(KEY_FD);

    // To instantly display the stopwatch on the HEX displays
    sprintf(STOPWATCH_buffer, "disp\n");
    write(STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));

    // Initialize stopwatch to 00:20:00
    sprintf (STOPWATCH_buffer, "%02d:%02d:%02d\n", 00, 20, 00);
    write (STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));

    // Set time limit and variables for MM:SS:DD
    time_limit = 2000;

    time_limit_minutes = time_limit / 6000;
    time_limit_hundreths = time_limit % 6000;
    time_limit_seconds = time_limit_hundreths / 100;
    time_limit_hundreths = time_limit_hundreths % 100;

    // Stopwatch is paused on startup
    sprintf(STOPWATCH_buffer, "stop\n");
    write(STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));
    

    printf("Set stopwatch if desired. Press KEY0 to start\n");

    while(!stop)
    {
        KEY_read = 0;
        SW_read = 0;
        SW_value = 0;
        int MM, SS, DD;

        if (!run)
        {
            while ((ret_val = read (KEY_FD, KEY_buffer, BYTES)) != 0)
                KEY_read += ret_val;	// read the driver until EOF
            KEY_buffer[KEY_read] = '\0';	// NULL terminate

            if(strtol(KEY_buffer, NULL, 16) == 1)
            {
                run = 1;
                sprintf(STOPWATCH_buffer, "run\n");
                write(STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));

                SW_value = 1023;
                sprintf (LEDR_buffer, "%03X\n", SW_value);
                write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));

                answer = get_equation(score);
            }

            else if(strtol(KEY_buffer, NULL, 16) == 2)
            {
                while ((ret_val = read (SW_FD, SW_buffer, BYTES)) != 0)
                    SW_read += ret_val;	// read the driver until EOF
                SW_buffer[SW_read] = '\0';	// NULL terminate

                SW_value = strtol(SW_buffer, NULL, 16);

                sprintf (LEDR_buffer, "%03X\n", SW_value);
                write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));

                // Extract the current time value and put it into local variables
                get_time(STOPWATCH_FD, STOPWATCH_buffer, &MM, &SS, &DD);

                // Replace DD
                if (SW_value < 100)
                    DD = SW_value;
                
                // Send back current time with DD altered
                sprintf (STOPWATCH_buffer, "%02d:%02d:%02d\n", MM, SS, DD);
                write (STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));

                time_limit = MM * 6000 + SS * 100 + DD;    // calculate time in 100th of a second

                time_limit_minutes = time_limit / 6000;
                time_limit_hundreths = time_limit % 6000;
                time_limit_seconds = time_limit_hundreths / 100;
                time_limit_hundreths = time_limit_hundreths % 100;
            }

            else if(strtol(KEY_buffer, NULL, 16) == 4)
            {
                while ((ret_val = read (SW_FD, SW_buffer, BYTES)) != 0)
                    SW_read += ret_val;	// read the driver until EOF
                SW_buffer[SW_read] = '\0';	// NULL terminate

                SW_value = strtol(SW_buffer, NULL, 16);

                sprintf (LEDR_buffer, "%03X\n", SW_value);
                write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));

                // Extract the current time value and put it into local variables
                get_time(STOPWATCH_FD, STOPWATCH_buffer, &MM, &SS, &DD);

                //Replace SS
                if (SW_value < 60)
                    SS = SW_value;
                
                // Send back current time with SS altered
                sprintf (STOPWATCH_buffer, "%02d:%02d:%02d\n", MM, SS, DD);
                write (STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));

                time_limit = MM * 6000 + SS * 100 + DD;    // calculate time in 100th of a second

                time_limit_minutes = time_limit / 6000;
                time_limit_hundreths = time_limit % 6000;
                time_limit_seconds = time_limit_hundreths / 100;
                time_limit_hundreths = time_limit_hundreths % 100;
            }

            else if(strtol(KEY_buffer, NULL, 16) == 8)
            {
                while ((ret_val = read (SW_FD, SW_buffer, BYTES)) != 0)
                    SW_read += ret_val;	// read the driver until EOF
                SW_buffer[SW_read] = '\0';	// NULL terminate

                SW_value = strtol(SW_buffer, NULL, 16);

                sprintf (LEDR_buffer, "%03X\n", SW_value);
                write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));

                // Extract the current time value and put it into local variables
                get_time(STOPWATCH_FD, STOPWATCH_buffer, &MM, &SS, &DD);

                //Replace MM
                if (SW_value < 60)
                MM = SW_value;
                
                // Send back current time with DD altered
                sprintf (STOPWATCH_buffer, "%02d:%02d:%02d\n", MM, SS, DD);
                write (STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));

                time_limit = MM * 6000 + SS * 100 + DD;    // calculate time in 100th of a second

                time_limit_minutes = time_limit / 6000;
                time_limit_hundreths = time_limit % 6000;
                time_limit_seconds = time_limit_hundreths / 100;
                time_limit_hundreths = time_limit_hundreths % 100;
            }

            usleep (5000); // Button Debounce
        }
        
        else
        {
            // Extract the current time value and put it into local variables
            get_time(STOPWATCH_FD, STOPWATCH_buffer, &MM, &SS, &DD);
            // time is up
            if (MM == 0 && SS == 0 && DD == 0)
            {
                total_time = convert_time_to_seconds(total_time);
                printf("\n");
                printf("Time expired! You answered %d questions, in an average of %.2f seconds!\n", score, (double) total_time / score);
                stop = 1;
            }

            // (pointer to file descriptor, # of entries, millisecond timeout)
            else if (poll(&p_fd, 1, 10) > 0)
            {
                int n = read(STDIN_FILENO, guess_buffer, BYTES);
                if (n > 0)
                {
                    int temp_MM, temp_SS, temp_DD;

                    guess_buffer[n] = '\0'; // Add null terminator

                    guess = strtol(guess_buffer, NULL, 10);

                    if (guess == answer)
                    {
                        // Increase score and setup next question
                        score++;
                        answer = get_equation(score);

                        // Extract the current time value and put it into local variables
                        get_time(STOPWATCH_FD, STOPWATCH_buffer, &temp_MM, &temp_SS, &temp_DD);

                        total_time += ((time_limit) - (temp_MM * 6000 + temp_SS * 100 + temp_DD));
                        
                        // Reset timer
                        sprintf (STOPWATCH_buffer, "%02d:%02d:%02d\n", time_limit_minutes, time_limit_seconds, time_limit_hundreths);
                        write (STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));
                    }
                    else
                    {
                        printf("Try again: ");
                        fflush(stdout);
                    }
                }
            }

        }
    }

    // Set LEDs off to 0 once exiting code
    sprintf (LEDR_buffer, "%03X\n", 0);
    write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));

    // To end the stopwatch on the HEX displays
    sprintf(STOPWATCH_buffer, "nodisp\n");
    write(STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));


    close(KEY_FD);
    close(SW_FD);
	close(LEDR_FD);

    return 0;
}

void get_time(int file_descriptor, char buf[], int *minutes, int *seconds, int *hundreths)
{
    int FD_read = 0;
    int ret_val;

    while ((ret_val = read (file_descriptor, buf, BYTES)) != 0)
        FD_read += ret_val;	// read the driver until EOF
    buf[FD_read] = '\0';	// NULL terminate

    // Extract the current time value and put it into local variables
    sscanf(buf, "%d:%d:%d", minutes, seconds, hundreths);
}

int get_equation(score)
{   
    int answer;

    // Every 4 correct answers increase the difficulty
    int difficulty = 1;
    for (int d = 0; d < (score / 4); d++)
        difficulty = difficulty * 10;

    int i = rand() % (10 * difficulty);
    int k = rand() % (10 * difficulty);

    printf("%d + %d = ", i, k);
    fflush(stdout);

    answer = i + k;
    return answer;
}

void flush_keys(int key_fd)
{
    char junk[BYTES];
    while (read(key_fd, junk, BYTES) != 0)
        ;
}

int convert_time_to_seconds(int time)
{
    int minutes, seconds, hundreths;

    minutes = time / 6000;
    
    hundreths = time % 6000;
    seconds = hundreths / 100;

    hundreths = hundreths % 100;

    return (minutes * 60 + seconds + hundreths * 0.01);
}