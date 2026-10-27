#include <linux/fs.h>               // struct file, struct file_operations
#include <linux/init.h>             // for __init, see code
#include <linux/module.h>           // for module init and exit macros
#include <linux/miscdevice.h>       // for misc_device_register and struct miscdev
#include <linux/uaccess.h>          // for copy_to_user, see code
#include <asm/io.h>                 // for mmap
#include "../include/address_map_arm_vm.h"


/**  your part 3 kernel code here  **/

// START of declarations needed for the character device drivers
static int LEDR_device_open (struct inode *, struct file *);
static int LEDR_device_release (struct inode *, struct file *);
static ssize_t LEDR_device_write(struct file *, const char *, size_t, loff_t *);

static int HEX_device_open (struct inode *, struct file *);
static int HEX_device_release (struct inode *, struct file *);
static ssize_t HEX_device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define LEDR_DEV "LEDR"
#define HEX_DEV "HEX"
#define LEDR_SIZE 3        // used for writing -- (usage) command
#define HEX_SIZE 6        // used for writing -- (usage) command

static struct file_operations LEDR_fops = {
    .owner = THIS_MODULE,
    .write = LEDR_device_write,
    .open = LEDR_device_open,
    .release = LEDR_device_release
};

static struct file_operations HEX_fops = {
    .owner = THIS_MODULE,
    .write = HEX_device_write,
    .open = HEX_device_open,
    .release = HEX_device_release
};

static struct miscdevice LEDR = { 
    .minor = MISC_DYNAMIC_MINOR, 
    .name = LEDR_DEV,
    .fops = &LEDR_fops,
    .mode = 0666
};

static struct miscdevice HEX = { 
    .minor = MISC_DYNAMIC_MINOR, 
    .name = HEX_DEV,
    .fops = &HEX_fops,
    .mode = 0666
};

static int LEDR_registered = 0;
static int HEX_registered = 0;
// END of declarations needed for the character device drivers

// START of declarations for the KEY and SW ports
void *LW_virtual;        // used to map physical addresses for the light-weight bridge
volatile int * LEDR_ptr;  // virtual pointer to KEY port
volatile int * HEX5_HEX4_ptr;   // virtual pointer to SW port
volatile int * HEX3_HEX0_ptr;
// END of declarations for the ports

// 7 Seg Pattern 
char seg7_pattern[10] =	{0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
						 0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};

/* Code to initialize the drivers */
static int __init init_drivers(void)
{
    int err = 0;

    /* START of code to make LEDR port character device driver */
    err = misc_register (&LEDR);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", LEDR_DEV);
    }
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", LEDR_DEV);
        LEDR_registered = 1;
    }

    /* START of code to make HEX port character device driver */
    err = misc_register (&HEX);
    if (err < 0) {
        printk (KERN_ERR "/dev/%s: misc_register() failed\n", HEX_DEV);
    }
    else {
        printk (KERN_INFO "/dev/%s driver registered\n", HEX_DEV);
        HEX_registered = 1;
    }
    /* END of code to make a character device driver */

    /* START of code to set up virtual port addresses */
    // generate a virtual address for the FPGA lightweight bridge
    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);

    // Set up the LEDR and HEX ports
    LEDR_ptr = LW_virtual + LEDR_BASE; // LEDR port base address
    *LEDR_ptr = 0;

    HEX5_HEX4_ptr = LW_virtual + HEX5_HEX4_BASE;   // HEX5 to HEX4 port base address
    HEX3_HEX0_ptr = LW_virtual + HEX3_HEX0_BASE;   // HEX3 to HEX0 port base address
    *HEX5_HEX4_ptr = 0;
    *HEX3_HEX0_ptr = 0;

    return 0;
}

/* Remove the character device drivers */
static void __exit stop_drivers(void) {  
    *LEDR_ptr = 0;
    *HEX5_HEX4_ptr = 0;
    *HEX3_HEX0_ptr = 0;

    iounmap (LW_virtual);
    /* Remove the devices from the kernel */
    if (LEDR_registered) {
        misc_deregister (&LEDR);
        printk (KERN_INFO "/dev/%s driver de-registered\n", LEDR_DEV);
    }
    if (HEX_registered) {
        misc_deregister (&HEX);
        printk (KERN_INFO "/dev/%s driver de-registered\n", HEX_DEV);
    }
}

/* Called when a process opens the LEDR driver */
static int LEDR_device_open(struct inode *inode, struct file *file) {
    return SUCCESS;
}

/* Called when a process closes the LEDR driver */
static int LEDR_device_release(struct inode *inode, struct file *file) {
    return 0;
}

/* Called when a process opens the HEX driver */
static int HEX_device_open(struct inode *inode, struct file *file) {
    return SUCCESS;
}

/* Called when a process closes the HEX driver */
static int HEX_device_release(struct inode *inode, struct file *file) {
    return 0;
}

/* Called when a process writes to the LEDR driver. */
static ssize_t LEDR_device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset) {
    char LEDR_string[LEDR_SIZE + 1];    // extra byte is for a NULL terminator
    size_t bytes;
    int value;
    int ret;
    bytes = length;

    if (bytes > LEDR_SIZE)             // can copy all at once, or not?
        bytes = LEDR_SIZE;
    if (copy_from_user (LEDR_string, buffer, bytes) != 0)
        printk (KERN_ERR "/dev/%s: copy_from_user unsuccessful\n", LEDR_DEV);
    LEDR_string[bytes] = '\0';        // NULL terminate
    *offset += bytes;                // keep track of how much was copied

    ret = kstrtoint(LEDR_string, 16, &value);
    if (ret == -EINVAL) {
        pr_err("Parsing error: Invalid characters in string\n");
        return ret;
    } else if (ret == -ERANGE) {
        pr_err("Overflow error: Value too large for int\n");
        return ret;
    }

    *LEDR_ptr = value;

    return length;    
}

/* Called when a process writes to the LEDR driver. */
static ssize_t HEX_device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset) {
    char HEX_string[HEX_SIZE + 1];    // extra byte is for a NULL terminator
    size_t bytes;
    int value;
    int ret;
    bytes = length;

    if (bytes > HEX_SIZE)             // can copy all at once, or not?
        bytes = HEX_SIZE;
    if (copy_from_user (HEX_string, buffer, bytes) != 0)
        printk (KERN_ERR "/dev/%s: copy_from_user unsuccessful\n", HEX_DEV);
    HEX_string[bytes] = '\0';        // NULL terminate
    *offset += bytes;                // keep track of how much was copied

    ret = kstrtoint(HEX_string, 10, &value);
    if (ret == -EINVAL) {
        pr_err("Parsing error: Invalid characters in string\n");
        return ret;
    } else if (ret == -ERANGE) {
        pr_err("Overflow error: Value too large for int\n");
        return ret;
    }

    *HEX5_HEX4_ptr = seg7_pattern[(value / 100000)] << 8;
    *HEX5_HEX4_ptr |= seg7_pattern[(value % 100000) / 10000];

    *HEX3_HEX0_ptr = seg7_pattern[(value % 10000) / 1000] << 24;
    *HEX3_HEX0_ptr |= seg7_pattern[(value % 1000) / 100] << 16;
    *HEX3_HEX0_ptr |= seg7_pattern[(value % 100) / 10] << 8;
    *HEX3_HEX0_ptr |= seg7_pattern[(value % 10)];

    return length; 
}

MODULE_LICENSE("GPL");
module_init(init_drivers);
module_exit(stop_drivers);