#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include "../include/address_map_arm.h"
#include "../include/interrupt_ID.h"


void * LW_virtual; // Lightweight bridge base address
volatile int *timer0_ptr, *KEY_ptr, *HEX3_HEX0_ptr, *HEX5_HEX4_ptr, *SW_ptr; // virtual addresses

char seg7_pattern[10] =	{0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110, 
						 0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};

int time = 360000;
int min = 60;
int sec = 0;
int hundreths = 0;

irq_handler_t timer0_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{

	// Countdown behaviour
	*(timer0_ptr) = 0; // Clear timer interrupt
	if (time == 0)
		time = 360000;

	time -= 1;

	min = time / 6000;
	*HEX5_HEX4_ptr = seg7_pattern[min / 10] << 8;				// most significant minute digit
	*HEX5_HEX4_ptr |= seg7_pattern[min % 10];					// least significant minute digit

	sec = (time % 6000) / 100;
	*HEX3_HEX0_ptr = seg7_pattern[sec / 10] << 24;
	*HEX3_HEX0_ptr |= seg7_pattern[sec % 10] << 16;

	hundreths = (time % 100);
	*HEX3_HEX0_ptr |= seg7_pattern[hundreths / 10] << 8;
	*HEX3_HEX0_ptr |= seg7_pattern[hundreths % 10];

	return (irq_handler_t) IRQ_HANDLED;
} 

irq_handler_t key_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	if (*(KEY_ptr + 3) & 0b0001)
	{
		if (*(timer0_ptr) & 0x0002) // is timer running?
			*(timer0_ptr + 1) = 0x0008; // timer disable
		else
			*(timer0_ptr + 1) = 0x0007; // timer enable
	}

	if (*(KEY_ptr + 3) & 0b1000) // Key 3 is pressed
	{
		if (*SW_ptr < 60)
		{
			min = *SW_ptr;
			*HEX5_HEX4_ptr = seg7_pattern[min / 10] << 8;
			*HEX5_HEX4_ptr |= seg7_pattern[min % 10];
			time = min * 6000 + sec * 100 + hundreths;
		}
	}
	else if (*(KEY_ptr + 3) & 0b0100) // Key 2 is pressed
	{
		if (*SW_ptr < 60)
		{
			sec = *SW_ptr;
			*HEX3_HEX0_ptr &= 0x0000FFFF; // Conserve bottom 16 bits
			*HEX3_HEX0_ptr |= seg7_pattern[sec / 10] << 24;
			*HEX3_HEX0_ptr |= seg7_pattern[sec % 10] << 16;
			time = min * 6000 + sec * 100 + hundreths;
		}
	}
	else if (*(KEY_ptr + 3) & 0b0010) // Key 1 is pressed
	{	
		if (*SW_ptr < 100)
		{
			hundreths = *SW_ptr;
			*HEX3_HEX0_ptr &= 0xFFFF0000; // Conserve top 16 bits
			*HEX3_HEX0_ptr |= seg7_pattern[hundreths / 10] << 8;
			*HEX3_HEX0_ptr |= seg7_pattern[hundreths% 10];
			time = min * 6000 + sec * 100 + hundreths;
		}
	}

	*(KEY_ptr + 3) = 0xF; 	// Clear the Edgecapture register (clears current key interrupt)
	return (irq_handler_t) IRQ_HANDLED;
}

static int __init initialize_stopwatch_handler(void)
{
	int counter;
	int ret_timer;
	int ret_key;
	// generate a virtual address for the FPGA lightweight bridge
	LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);

	timer0_ptr = LW_virtual + TIMER0_BASE;

	// Set starting time
	counter = 1000000; // period = 1/(100 MHz) * (1 x 10^6) = 0.01 sec
	// 1000000 = 0b11110100001001000000 which requires 20 bits
	// Store 16 in lower register, and 4 in the higher register
	*(timer0_ptr + 2) = (counter & 0xFFFF);
	*(timer0_ptr + 3) = (counter >> 16) & 0xFFFF;

	// Initialize timer and enable timer interrupt
	*(timer0_ptr + 1) = 0x0007;
	HEX3_HEX0_ptr = LW_virtual + HEX3_HEX0_BASE; // virtual address for HEX3_HEX0 port
	HEX5_HEX4_ptr = LW_virtual + HEX5_HEX4_BASE;

	// Initialize pushbuttons
	KEY_ptr = LW_virtual + KEY_BASE; // virtual address for KEY port
	*(KEY_ptr + 3) = 0xF; // Clear the Edgecapture register
	*(KEY_ptr + 2) = 0xF; // Enable IRQ generation for the 4 buttons

	// Initialize switches
	SW_ptr = LW_virtual + SW_BASE;

	// Register the interrupt handler.
	ret_timer = request_irq (TIMER0_IRQ, (irq_handler_t) timer0_irq_handler, IRQF_SHARED,
	"timer0_irq_handler", (void *) (timer0_irq_handler));
	if (ret_timer)
		return ret_timer; // Check for error

	ret_key = request_irq (KEY_IRQ, (irq_handler_t) key_irq_handler, IRQF_SHARED,
	"key_irq_handler", (void *) (key_irq_handler));
	if (ret_key)
		return ret_key; // Check for error

	return 0; // Return 0 is no request_irq failed
}

static void __exit cleanup_stopwatch_handler(void)
{
	*(timer0_ptr + 1) = 0x8; // [3:0] -> STOP, START, CONT, ITO
	*HEX3_HEX0_ptr = 0;  // Clear seven-seg display
	*HEX5_HEX4_ptr = 0;
	iounmap (LW_virtual);
	free_irq (TIMER0_IRQ, (void*) timer0_irq_handler);
	free_irq (KEY_IRQ, (void*) key_irq_handler);
}

module_init(initialize_stopwatch_handler);
module_exit(cleanup_stopwatch_handler);