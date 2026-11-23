#include <linux/kernel.h>
#include <linux/fs.h>               // struct file, struct file_operations
#include <linux/init.h>             // for __init, see code
#include <linux/module.h>           // for module init and exit macros
#include <linux/miscdevice.h>       // for misc_device_register and struct miscdev
#include <linux/uaccess.h>          // for copy_to_user / copy_from_user
#include <linux/string.h>           // strcmp, strncmp
#include <linux/errno.h>
#include <asm/io.h>                 // for mmap

#include "../include/address_map_arm.h"
#include "../include/ADXL345.h"

/* /dev/accel
 * READ : "R XXXX YYYY ZZZZ SS\n"
 * WRITE: text commands (sent by accel_wrappers.c or by echo)
 *      device          print the ADXL345 device ID with printk
 *      init            re-initialize the ADXL345            -> ADXL345_Init()
 *      calibrate       run the calibration routine          -> ADXL345_Calibrate()
 *      format F G      F = 0: fixed 10-bit, F = 1: full resolution; G = 2, 4, 8, 16 (+/- g)
 *      rate R          output data rate in Hz (25, 12.5, 6.25, 1.56, 0.78, ...)
 * The ADXL345 functions are in ../ADXL345.c (build it with -DKERNEL). */

static int device_open (struct inode *, struct file *);
static int device_release (struct inode *, struct file *);
static ssize_t device_read (struct file *, char *, size_t, loff_t *);
static ssize_t device_write (struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define DEV_NAME "accel"
#define ACCEL_BYTES 20      // number of bytes to return when read (includes a \n)
#define CMD_LEN 64          // max length of a command written to the driver
#define FMT_RANGE_MASK 0x03 // range bits (D1:D0) of the DATA_FORMAT register

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static struct miscdevice accel_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEV_NAME,
    .fops = &fops,
    .mode = 0666
};

static int accel_registered = 0;

volatile unsigned int * I2C0_ptr;
volatile unsigned int * SYSMGR_ptr;
static int16_t XYZ[3];

/* Supported output data rates, in hundredths of Hz (no floating point in the kernel),
 * and the matching BW_RATE register codes. */
// Divide value by 100 to get Hz value
struct rate_entry { int centi_hz; unsigned char code; };
static const struct rate_entry rates[] = {
    {10000,  0x0A}, {5000,  0x09}, {2500,  0x08},
    {1250,   0x07}, {625,   0x06}, {313,   0x05}, {156,   0x04},
    {78,     0x03}, {39,    0x02}, {20,    0x01}, {10,    0x00}
};
#define NUM_RATES (sizeof(rates) / sizeof(rates[0]))

/* Parse "12", "12.5", "6.25" into hundredths of Hz. Returns 0 on success. */
static int parse_centi_hz (const char *s, int *out) {
    int whole = 0, frac = 0, frac_digits = 0;
    if (*s < '0' || *s > '9') return -1;
    while (*s >= '0' && *s <= '9') {
        whole = whole * 10 + (*s - '0');
        if (whole > 100000) return -1;
        s++;
    }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9' && frac_digits < 2) {
            frac = frac * 10 + (*s - '0');
            frac_digits++;
            s++;
        }
        if (frac_digits == 1) frac *= 10;
    }
    if (*s != '\0') return -1;
    *out = whole * 100 + frac;
    return 0;
}

static void do_rate (const char *arg) {
    int centi, i;
    if (parse_centi_hz (arg, &centi) != 0) {
        printk (KERN_ERR "/dev/%s: bad rate \"%s\"\n", DEV_NAME, arg);
        return;
    }
    for (i = 0; i < NUM_RATES; i++) {
        if (centi == rates[i].centi_hz) {
            ADXL345_REG_WRITE (ADXL345_REG_BW_RATE, rates[i].code);
            printk (KERN_INFO "/dev/%s: output data rate set to %d.%02d Hz\n", DEV_NAME,
                centi / 100, centi % 100);
            return;
        }
    }
    printk (KERN_ERR "/dev/%s: unsupported rate. Use 100, 50, 25, 12.5, 6.25, 3.13, "
        "1.56, 0.78, 0.39, 0.20 or 0.10 Hz\n", DEV_NAME);
}

static void do_format (int F, int G) {
    uint8_t val, range, resolution;
    switch (G) {
        case 2:  range = XL345_RANGE_2G;  break;
        case 4:  range = XL345_RANGE_4G;  break;
        case 8:  range = XL345_RANGE_8G;  break;
        case 16: range = XL345_RANGE_16G; break;
        default:
            printk (KERN_ERR "/dev/%s: format range G must be 2, 4, 8 or 16\n", DEV_NAME);
            return;
    }
    if (F != 0 && F != 1) {
        printk (KERN_ERR "/dev/%s: format F must be 0 or 1\n", DEV_NAME);
        return;
    }
    resolution = F ? XL345_FULL_RESOLUTION : 0;

    ADXL345_REG_READ (ADXL345_REG_DATA_FORMAT, &val);   // keep the other bits as they are
    val &= ~(FMT_RANGE_MASK | XL345_FULL_RESOLUTION);
    val |= range | resolution;
    ADXL345_REG_WRITE (ADXL345_REG_DATA_FORMAT, val);

    mg_per_lsb = calc_mg_per_lsb (resolution, range);   // from ADXL345.c
    printk (KERN_INFO "/dev/%s: format %s, +/-%d g, %d mg/LSB\n", DEV_NAME,
        F ? "full resolution" : "10-bit", G, mg_per_lsb);
}

static int __init start_accel(void) {
    int err = misc_register (&accel_dev);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", DEV_NAME);
    }
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", DEV_NAME);
        accel_registered = 1;
    }

    I2C0_ptr = ioremap_nocache (I2C0_BASE, I2C0_SPAN);
    SYSMGR_ptr = ioremap_nocache (SYSMGR_BASE, SYSMGR_SPAN);

    if ((I2C0_ptr == NULL) || (SYSMGR_ptr == NULL))
        printk (KERN_ERR "Error: ioremap_nocache returned NULL!\n");

    pass_addrs((unsigned int*) SYSMGR_ptr, (unsigned int*) I2C0_ptr);

    Pinmux_Config ();
    I2C0_Init ();
    ADXL345_Init ();
    return 0;
}

static void __exit stop_accel(void) {
    iounmap (I2C0_ptr);
    iounmap (SYSMGR_ptr);
    if (accel_registered) {
        misc_deregister (&accel_dev);
        printk (KERN_INFO "/dev/%s driver de-registered\n", DEV_NAME);
    }
}

static int device_open(struct inode *inode, struct file *file) {
    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset) {
    char themessage[ACCEL_BYTES + 1];
    size_t bytes;
    int NewData, i;
    int v[3];

    bytes = ACCEL_BYTES - (*offset);
    if (bytes) {
        NewData = ADXL345_IsDataReady ();
        if (NewData)
            ADXL345_XYZ_Read (XYZ);

        // clamp so each field stays 4 characters wide: the message must be exactly
        // ACCEL_BYTES long (full-resolution data can exceed 4 digits if the board is shaken)
        for (i = 0; i < 3; i++) {
            v[i] = XYZ[i];
            if (v[i] > 999) v[i] = 999;
            if (v[i] < -999) v[i] = -999;
        }
        snprintf(themessage, sizeof(themessage), "%1d %04d %04d %04d %02d\n",
            NewData, v[0], v[1], v[2], mg_per_lsb);

        if (copy_to_user (buffer, themessage, bytes) != 0)
            printk (KERN_ERR "/dev/%s: copy_to_user unsuccessful\n", DEV_NAME);
    }
    (*offset) = bytes;
    return bytes;
}

/* Called when a process writes a command to /dev/accel */
static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset) {
    char cmd[CMD_LEN];
    size_t n = (length < CMD_LEN - 1) ? length : CMD_LEN - 1;
    int F, G;
    uint8_t id;

    if (copy_from_user (cmd, buffer, n) != 0) {
        printk (KERN_ERR "/dev/%s: copy_from_user unsuccessful\n", DEV_NAME);
        return -EFAULT;
    }
    cmd[n] = '\0';
    while (n > 0 && (cmd[n-1] == '\n' || cmd[n-1] == '\r' || cmd[n-1] == ' '))
        cmd[--n] = '\0';

    // "d" command
    if (strcmp (cmd, "device") == 0) {
        ADXL345_IdRead (&id);
        printk (KERN_INFO "/dev/%s: ADXL345 device ID = 0x%02X\n", DEV_NAME, id);
    }
    // "i" command
    else if (strcmp (cmd, "init") == 0) {
        ADXL345_Init ();
        printk (KERN_INFO "/dev/%s: ADXL345 re-initialized\n", DEV_NAME);
    }
    // "c" commands
    else if (strcmp (cmd, "calibrate") == 0) {
        printk (KERN_INFO "/dev/%s: calibrating, keep the board flat and still\n", DEV_NAME);
        ADXL345_Calibrate ();
        printk (KERN_INFO "/dev/%s: calibration done\n", DEV_NAME);
    }
    // "f" and "g" command
    else if (strncmp (cmd, "format ", 7) == 0) {
        if (sscanf (cmd + 7, "%d %d", &F, &G) == 2)
            do_format (F, G);
        else
            printk (KERN_ERR "/dev/%s: usage: format F G\n", DEV_NAME);
    }
    // "r" command
    else if (strncmp (cmd, "rate ", 5) == 0) {
        do_rate (cmd + 5);
    }
    else {
        printk (KERN_ERR "/dev/%s: unknown command \"%s\"\n", DEV_NAME, cmd);
    }
    return length;      // consume the whole write so user programs don't retry
}

MODULE_LICENSE("GPL");
module_init (start_accel);
module_exit (stop_accel);