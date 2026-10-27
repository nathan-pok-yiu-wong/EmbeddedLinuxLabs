#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h> // Included to use strtol

/**  your part 4 user code here  **/
#define BYTES 256        // max number of characters to read from /dev/chardev
#define SUCCESS 0
#define LEDR_DEV "LEDR"
#define HEX_DEV "HEX"
#define LEDR_SIZE 5        // 3 for value, 1 for \n, 1 for null terminator
#define HEX_SIZE 9        // 6 for value, 1 for \n, 1 for null terminator
#define KEY_SIZE 3       // 1 for value, 1 for \n, 1 for null terminator
#define SW_SIZE 5        // 3 for value, 1 for \n, 1 for null terminator

volatile sig_atomic_t stop;
void catchSIGINT(int signum){
    stop = 1;
}

/* This code uses the character device driver /dev/chardev. The code reads the default
 * message from the driver and then prints it. After this the code changes the message in
 * a loop by writing to the driver, and prints each new message. The program exits if it
 * receives a kill signal (for example, ^C typed on stdin). */
int main(int argc, char *argv[]){

	int KEY_FD, SW_FD, LEDR_FD, HEX_FD;		// file descriptor

    // buffers for different I/O data
  	char KEY_buffer[KEY_SIZE], SW_buffer[SW_SIZE], LEDR_buffer[LEDR_SIZE], HEX_buffer[HEX_SIZE];
	int ret_val, KEY_read, SW_read; 
    int SW_value = 0;
    int total = 0;

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

    // Open the HEX device driver for write
	if ((HEX_FD = open("/dev/HEX", O_WRONLY)) == -1)
	{
		printf("Error opening /dev/HEX: %s\n", strerror(errno));
		return -1;
	}


    while(!stop)
    {
        KEY_read = 0;
        SW_read = 0;
        SW_value = 0;
		while ((ret_val = read (KEY_FD, KEY_buffer, BYTES)) != 0)
			KEY_read += ret_val;	// read the driver until EOF
        KEY_buffer[KEY_read] = '\0';	// NULL terminate

        if(strtol(KEY_buffer, NULL, 16))
        {
            while ((ret_val = read (SW_FD, SW_buffer, BYTES)) != 0)
			    SW_read += ret_val;	// read the driver until EOF
            SW_buffer[SW_read] = '\0';	// NULL terminate

            SW_value = strtol(SW_buffer, NULL, 16);
            
            total += SW_value;

            sprintf (LEDR_buffer, "%03X\n", SW_value);
            write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));

            sprintf(HEX_buffer, "%d\n", total);
            write (HEX_FD, HEX_buffer, strlen(HEX_buffer));

        }

        usleep (1000);
    }

    // Set LEDs off to 0 once exiting code
    sprintf (LEDR_buffer, "%03X\n", 0);
    write (LEDR_FD, LEDR_buffer, strlen(LEDR_buffer));

    // Set HEX display to 0 once exiting code
    sprintf(HEX_buffer, "%d\n", 0);
    write (HEX_FD, HEX_buffer, strlen(HEX_buffer));

    close(KEY_FD);
    close(SW_FD);
	close(LEDR_FD);
    close(HEX_FD);

   return 0;
}