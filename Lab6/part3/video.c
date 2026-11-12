#include <linux/fs.h> // struct file, struct file_operations
#include <linux/init.h> // for __init, see code
#include <linux/module.h> // for module init and exit macros
#include <linux/miscdevice.h> // for misc_device_register and struct miscdev
#include <linux/uaccess.h> // for copy_to_user, see code
#include <linux/string.h>
#include <asm/io.h> // for mmap
#include "../include/address_map_arm.h"

/**  your part 3 kernel code here  **/

/* This kernel module provides a character device driver that controls a video display.
 * The user-interface to the driver is through the file /dev/video. The following commands
 * can be written to the driver through this file: "clear", "pixel x,y color", and
 * "line x1,y1 x2,y2 color".
 * Reading from the /dev/video produces the string "screen_x screen_y", where these are
 * the number of columns and rows on the screen, respectively. */

/* Prototypes for functions used for video display */
void get_screen_specs (volatile int *);
void clear_screen (int*);
void draw_line(int, int, int, int, short int);
void plot_pixel(int, int, short int, int*);
void wait_for_vsync(volatile int *);

// START of declarations needed for a character device
static int video_open (struct inode *, struct file *);
static int video_release (struct inode *, struct file *);
static ssize_t video_read (struct file *, char *, size_t, loff_t *);
static ssize_t video_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = video_read,
    .write = video_write,
    .open = video_open,
    .release = video_release
};

#define ABS(x)  (((x) > 0) ? (x) : -(x))

#define SUCCESS 0
#define DEV_NAME "video"
#define VIDEO_BYTES 8   // number of bytes to return when read (includes a \n)
#define VIDEO_SIZE 25   // max number of bytes accepted for a write

static struct miscdevice video_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEV_NAME,
    .fops = &fops,
    .mode = 0666
};

static int video_registered = 0;
// END of declarations needed for a character device

// START of declarations for the video
void *LW_virtual;                 // used to map physical addresses for the light-weight bridge
volatile int * pixel_ctrl_ptr;    // virtual address of pixel buffer DMA controller
int onChip_buff;                  // current location of on chip buffer memory
int SDRAM_buff;                   // current location of SDRAM buffer memeory
int resolution_x, resolution_y;   // VGA screen size
int back_buff;                    // Current Location of Back Buffer.
// END of declarations

// Helper function to help debugging a stuck pixel
static unsigned short read_pixel(int *buffer, int x, int y)
{
    return *(volatile unsigned short *)((char *)buffer + (y << 10) + (x << 1));
}

/* Code to initialize the video driver */
static int __init start_video(void)
{
    int err = misc_register (&video_dev);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV_NAME);
    }
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", DEV_NAME);
        video_registered = 1;
    }
    // end of special code for char. dev. driver

    // generate a virtual address for the FPGA lightweight bridge
    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);
    if (LW_virtual == 0)
        printk (KERN_ERR "/dev/%s: ioremap_nocache returned NULL\n", DEV_NAME);

    // Create virtual memory access to the pixel buffer controller
    pixel_ctrl_ptr = (unsigned int *) (LW_virtual + PIXEL_BUF_CTRL_BASE);
    get_screen_specs (pixel_ctrl_ptr);                // determine X, Y screen size

    // Create virtual memory access to the on chip buffer
    onChip_buff = (int) ioremap_nocache (FPGA_ONCHIP_BASE, FPGA_ONCHIP_SPAN);
    if (onChip_buff == 0)
        printk (KERN_ERR "/dev/%s: ioremap_nocache returned NULL\n", DEV_NAME);

    // Create virtual memeory access to the SDRAM buffer
    SDRAM_buff = (int) ioremap_nocache (SDRAM_BASE, SDRAM_SPAN);
    if (SDRAM_buff == 0)
        printk(KERN_ERR "/dev/%s: ioremap_nocache returned NULL\n", DEV_NAME);

    /* Erase the pixel buffers */
    clear_screen(onChip_buff);
    clear_screen(SDRAM_buff);

    // Initialize Front and Back Buffers
    *(pixel_ctrl_ptr + 1) = FPGA_ONCHIP_BASE;  // make on-chip the front buffer
    wait_for_vsync(pixel_ctrl_ptr);
    *(pixel_ctrl_ptr + 1) = SDRAM_BASE;        // SDRAM becomes the back buffer
    back_buff = SDRAM_buff;                    // virtual address, not physical

    return 0;
}

/* Function to find the VGA screen specifications */
void get_screen_specs(volatile int * pixel_ctrl_ptr)
{
    int resolution_reg;

    resolution_reg = *(pixel_ctrl_ptr + 2);
    resolution_x = resolution_reg & 0xFFFF;
    resolution_y = resolution_reg >> 16;
}

/* Function to clear the pixel buffer */
void clear_screen(int* buffer)
{
    int y, x;

    for (x = 0; x < resolution_x; x++)
        for (y = 0; y < resolution_y; y++)
            plot_pixel (x, y, 0, buffer);
}

void wait_for_vsync(volatile int * pixel_ctrl_ptr)
{
    volatile int status;

    *pixel_ctrl_ptr = 1; // Write to DMA Controller to Swap

    status = *(pixel_ctrl_ptr + 3);
    while ((status & 0x01) != 0)        // TODO: potential improvement for PartAdditional: Look into removing constant polling in kernel code -> better performance/behaviour
    {
        status = *(pixel_ctrl_ptr + 3); // Poll status
    }

    // Update which is currently the back buffer
    if (*(pixel_ctrl_ptr + 1) == SDRAM_BASE)
        back_buff = SDRAM_buff;
    else
        back_buff = onChip_buff;
}

void plot_pixel(int x, int y, short int line_color, int* buffer)
{
    *(short int *)((char*)buffer + (y << 10) + (x << 1)) = line_color;
}

/* Bresenham's line drawing algorithm. */
void draw_line(int x0, int y0, int x1, int y1, short int color)
{
/* This function draws a line between points (x0, y0) and (x1, y1). The function assumes
 * that the coordinates are valid */
    register int x_0 = x0;
    register int y_0 = y0;
    register int x_1 = x1;
    register int y_1 = y1;
    register char steep = (ABS(y_1 - y_0) > ABS(x_1 - x_0)) ? 1 : 0;
    register int deltax, deltay, tmp, error, ystep, x, y;

    /* Preprocessing inputs */
    if (steep > 0) {
        // Swap x_0 and y_0
        tmp = x_0;
        x_0 = y_0;
        y_0 = tmp;
        // Swap x_1 and y_1
        tmp = x_1;
        x_1 = y_1;
        y_1 = tmp;
    }
    if (x_0 > x_1) {
        // Swap x_0 and x_1
        tmp = x_0;
        x_0 = x_1;
        x_1 = tmp;
        // Swap y_0 and y_1
        tmp = y_0;
        y_0 = y_1;
        y_1 = tmp;
    }

    /* Setup local variables */
    deltax = x_1 - x_0;
    deltay = ABS(y_1 - y_0);
    error = -(deltax / 2);
    y = y_0;
    if (y_0 < y_1)
        ystep = 1;
    else
        ystep = -1;

    /* Draw a line - either go along the x-axis (steep = 0) or along the y-axis (steep = 1) */
    for (x = x_0; x <= x_1; x++) {
        if (steep == 1)
            plot_pixel (y, x, color, (int*) back_buff);
        else
            plot_pixel (x, y, color, (int*) back_buff);
        error = error + deltay;
        if (error > 0) {
            y = y + ystep;
            error = error - deltax;
        }
    }
}

/* Remove the character device driver */
static void __exit stop_video(void)
{
    /* unmap the physical-to-virtual mappings */
    iounmap (LW_virtual);
    iounmap ((void *) onChip_buff);
    iounmap ((void *) SDRAM_buff);

    /* Remove the device from the kernel */
    if (video_registered) {
        misc_deregister (&video_dev);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME);
    }
}

/* Called when a process opens the video */
static int video_open(struct inode *inode, struct file *file)
{
    return SUCCESS;
}

/* Called when a process closes the video */
static int video_release(struct inode *inode, struct file *file)
{
    return 0;
}

/* Called when a process reads from the video. If *offset = 0, the current value of
 * the video is copied back to the user, and VIDEO_BYTES is returned. But if
 * *offset = VIDEO_BYTES, nothing is copied back to the user and 0 (EOF) is returned. */
static ssize_t video_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
    char themessage[VIDEO_BYTES + 1];   // extra byte is for a NULL terminator
    size_t bytes;

    bytes = VIDEO_BYTES - (*offset);    // has the video data been sent yet?
    if (bytes) {
        sprintf(themessage, "%03d %03d\n", resolution_x, resolution_y);
        if (copy_to_user (buffer, themessage, bytes) != 0)
            printk (KERN_ERR "/dev/%s: copy_to_user unsuccessful\n", DEV_NAME);
    }
    (*offset) = bytes;                  // keep track of number of bytes sent to the user
    return bytes;                       // returns VIDEO_BYTES, or EOF
}

/* Called when a process writes to the video */
static ssize_t video_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
    char video_string[VIDEO_SIZE + 1];  // extra byte is for a NULL terminator
    int bytes;
    char command[8];
    int x1, y1, x2, y2, color;

    bytes = length;
    if (bytes > VIDEO_SIZE)             // can copy all at once, or not?
        bytes = VIDEO_SIZE;
    if (copy_from_user (video_string, buffer, bytes) != 0)
        printk (KERN_ERR "/dev/%s: copy_from_user in /dev/video unsuccessful\n", DEV_NAME);

    video_string[bytes] = '\0';
    if (video_string[0] == '-' && video_string[1] == '-')
    {
        printk (KERN_ERR "       clear\n");
        printk (KERN_ERR "       pixel X,Y color\n");
        printk (KERN_ERR "       line X1,Y1 X2,Y2 color\n");
        printk (KERN_ERR "       sync\n");
        printk (KERN_ERR "Notes: X,Y are integers, color is 16-bit hex\n");
    }
    else if (strncmp(video_string, "clear", 5) == 0)
    {
        clear_screen ((int*) back_buff);
    }
    else if (strncmp(video_string, "pixel", 5) == 0)
    {
        if (sscanf (video_string, "%7s %d,%d %X", command, &x1, &y1, &color) != 4)
        {
            printk (KERN_ERR "/dev/%s: can't parse user input: %s\n", DEV_NAME, video_string);
        }
        else if ((x1 < 0) || (x1 >= resolution_x) || (y1 < 0) || (y1 >= resolution_y)) // Bounds check missing in original
            printk(KERN_ERR "/dev/%s: bad coordinates: %d,%d\n", DEV_NAME, x1, y1);
        else
            plot_pixel(x1, y1, color, (int *) back_buff);
    }
    else if (strncmp(video_string, "line", 4) == 0)
    {
        if (sscanf (video_string, "%7s %d,%d %d,%d %X", command, &x1, &y1, &x2, &y2, &color) != 6)
        {
            printk (KERN_ERR "/dev/%s: can't parse user input: %s\n", DEV_NAME, video_string);
        }
        else if (strcmp (command, "line") == 0)
        {
            if ((x1 < 0) || (x1 > resolution_x - 1) || (x2 < 0) || (x2 > resolution_x - 1) ||
                (y1 < 0) || (y1 > resolution_y - 1) || (y2 < 0) || (y2 > resolution_y - 1))
                printk (KERN_ERR "/dev/%s: bad coordinates: %d,%d %d,%d\n", DEV_NAME, x1, y1, x2, y2);
            else
                draw_line (x1, y1, x2, y2, color);
        }
        else
            printk (KERN_ERR "/dev/%s: bad argument: %s\n", DEV_NAME, command);
    }
    else if (strncmp(video_string, "sync", 4) == 0)
    {
        wait_for_vsync(pixel_ctrl_ptr);
    }
    else if (strncmp(video_string, "dbg", 3) == 0)
    {
            printk(KERN_INFO "front=%08X back=%08X status=%08X\n",
                   *pixel_ctrl_ptr, *(pixel_ctrl_ptr + 1), *(pixel_ctrl_ptr + 3));
            printk(KERN_INFO "back_buff=%08X onchip=%08X sdram=%08X\n",
                   back_buff, onChip_buff, SDRAM_buff);
            printk(KERN_INFO "pixel(229,15): onchip=0x%04X sdram=0x%04X\n",
                   read_pixel((int *) onChip_buff, 229, 15),
                   read_pixel((int *) SDRAM_buff, 229, 15));
    }
    else if (strncmp(video_string, "poke", 4) == 0)
    {
            plot_pixel(229, 15, 0xF800, (int *) onChip_buff);
            plot_pixel(229, 15, 0xF800, (int *) SDRAM_buff);
    }
    else
    {
        printk(KERN_ERR "Invalid command, use '--' for help");
    }
    return length;
}

MODULE_LICENSE("GPL");
module_init (start_video);
module_exit (stop_video);
