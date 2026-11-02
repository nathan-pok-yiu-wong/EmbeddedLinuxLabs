#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h> // Included to use strtol

/**  your part 3 user code here  **/
#define BYTES 256        // max number of characters to read from /dev/chardev
#define SUCCESS 0
#define LEDR_SIZE 5        // 3 for value, 1 for \n, 1 for null terminator
#define KEY_SIZE 3       // 1 for value, 1 for \n, 1 for null terminator
#define SW_SIZE 5        // 3 for value, 1 for \n, 1 for null terminator

volatile sig_atomic_t stop;
void catchSIGINT(int signum){
    stop = 1;
}

int main(int argc, char *argv[]){

	int KEY_FD, SW_FD, LEDR_FD, STOPWATCH_FD;	// file descriptor

    // buffers for different I/O data
  	char KEY_buffer[KEY_SIZE], SW_buffer[SW_SIZE], LEDR_buffer[LEDR_SIZE], STOPWATCH_buffer[BYTES];
	int ret_val, KEY_read, SW_read, STOPWATCH_read;
    int SW_value = 0;
    int run = 1;

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

    // To instantly display the stopwatch on the HEX displays
    sprintf(STOPWATCH_buffer, "disp\n");
    write(STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));

    // LED is fully lit when running and fully off when not running
    SW_value = 1023;
    sprintf (LEDR_buffer, "%03X\n", SW_value);
    write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));

    // Initialize stopwatch to 59:59:99
    sprintf (STOPWATCH_buffer, "%02d:%02d:%02d\n", 59, 59, 99);
    write (STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));

    // Stopwatch starts running on startup
    sprintf(STOPWATCH_buffer, "run\n");
    write(STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));

    while(!stop)
    {
        KEY_read = 0;
        SW_read = 0;
        SW_value = 0;
        STOPWATCH_read = 0;

		while ((ret_val = read (KEY_FD, KEY_buffer, BYTES)) != 0)
			KEY_read += ret_val;	// read the driver until EOF
        KEY_buffer[KEY_read] = '\0';	// NULL terminate

        if(strtol(KEY_buffer, NULL, 16) == 1)
        {
            // Set LEDRs to 0 for running and pausing
            SW_value = 0;
            sprintf (LEDR_buffer, "%03X\n", SW_value);
            write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));

            if (run)
            {
                run = 0;
                sprintf(STOPWATCH_buffer, "stop\n");
                write(STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));
                SW_value = 0;
                sprintf (LEDR_buffer, "%03X\n", SW_value);
                write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));
            }
            else
            {
                run = 1;
                sprintf(STOPWATCH_buffer, "run\n");
                write(STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));
                SW_value = 1023;
                sprintf (LEDR_buffer, "%03X\n", SW_value);
                write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));
            }
        }

        else if(strtol(KEY_buffer, NULL, 16) == 2)
        {
            unsigned MM, SS, DD;

            while ((ret_val = read (SW_FD, SW_buffer, BYTES)) != 0)
			    SW_read += ret_val;	// read the driver until EOF
            SW_buffer[SW_read] = '\0';	// NULL terminate

            SW_value = strtol(SW_buffer, NULL, 16);

            sprintf (LEDR_buffer, "%03X\n", SW_value);
            write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));

            while ((ret_val = read (STOPWATCH_FD, STOPWATCH_buffer, BYTES)) != 0)
			    STOPWATCH_read += ret_val;	// read the driver until EOF
            STOPWATCH_buffer[STOPWATCH_read] = '\0';	// NULL terminate

            // Extract the current time value and put it into local variables
            sscanf(STOPWATCH_buffer, "%d:%d:%d", &MM, &SS, &DD);

            // Replace DD
            // Required for DD due to how original stopwatch.c module is set up
            // They have it to only accept 8 bytes which would parse a DD of 128 to 12.
            if (SW_value < 100)
                DD = SW_value;
            
            // Send back current time with DD altered
            sprintf (STOPWATCH_buffer, "%02d:%02d:%02d\n", MM, SS, DD);
            write (STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));
        }

        else if(strtol(KEY_buffer, NULL, 16) == 4)
        {
            unsigned MM, SS, DD;

            while ((ret_val = read (SW_FD, SW_buffer, BYTES)) != 0)
			    SW_read += ret_val;	// read the driver until EOF
            SW_buffer[SW_read] = '\0';	// NULL terminate

            SW_value = strtol(SW_buffer, NULL, 16);

            sprintf (LEDR_buffer, "%03X\n", SW_value);
            write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));

            while ((ret_val = read (STOPWATCH_FD, STOPWATCH_buffer, BYTES)) != 0)
			    STOPWATCH_read += ret_val;	// read the driver until EOF
            STOPWATCH_buffer[STOPWATCH_read] = '\0';	// NULL terminate

            // Extract the current time value and put it into local variables
            sscanf(STOPWATCH_buffer, "%d:%d:%d", &MM, &SS, &DD);

            //Replace SS
            SS = SW_value;
            
            // Send back current time with SS altered
            sprintf (STOPWATCH_buffer, "%02d:%02d:%02d\n", MM, SS, DD);
            write (STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));
        }

        else if(strtol(KEY_buffer, NULL, 16) == 8)
        {
            unsigned MM, SS, DD;

            while ((ret_val = read (SW_FD, SW_buffer, BYTES)) != 0)
			    SW_read += ret_val;	// read the driver until EOF
            SW_buffer[SW_read] = '\0';	// NULL terminate

            SW_value = strtol(SW_buffer, NULL, 16);

            sprintf (LEDR_buffer, "%03X\n", SW_value);
            write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));

            while ((ret_val = read (STOPWATCH_FD, STOPWATCH_buffer, BYTES)) != 0)
			    STOPWATCH_read += ret_val;	// read the driver until EOF
            STOPWATCH_buffer[STOPWATCH_read] = '\0';	// NULL terminate

            // Extract the current time value and put it into local variables
            sscanf(STOPWATCH_buffer, "%d:%d:%d", &MM, &SS, &DD);

            //Replace MM
            MM = SW_value;
            
            // Send back current time with DD altered
            sprintf (STOPWATCH_buffer, "%02d:%02d:%02d\n", MM, SS, DD);
            write (STOPWATCH_FD, STOPWATCH_buffer, strlen(STOPWATCH_buffer));
        }

        usleep (5000);
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