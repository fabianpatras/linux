#include <stddef.h>
#include <stdint.h>
#include "queue.h"
#include "device.h"

// Phys addr memory layout:
// Code: [0x1000, sizeof(code)) // 16 x 4KB page long
// Heap: (Guest Memory): [0x01_0000, xxx) 				// |
// Stack: [xxx, 0x10_0000) - grows down from 0x10_0000  // | both combined to 240 x 4KB page long
// MMIO Devices: [0x10_0000, 0x10_1000) // 1 x 4KB page long

// Paging structures: [0x10_1000, 0x10_2000) // 1 x 4KB page long

// deleted everything else -> this is 32 bit code
// go to 64-bit code for the simvirt implementation

/* Helper functions */
static void outb(uint16_t port, uint8_t value) {
	asm("outb %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {

    /* Our VM doesn't have malloc or anything, we simply use the memory */
	char p[] = "Hello, world!\n";
	uint32_t len = sizeof(p) / sizeof(char);
	uint8_t i = 0;

	for (i = 0; i < len; i++) {
		outb(0xE9, p[i]);
	}

	*(long *) 0x400 = 42;

    /* Example of using the queue
	   p = 0x010000 + 400;
	   *p = 10;
	   circ_bbuf_push(&g2h_queue, *p);
	*/
	
	/* Note: at heap_phys_addr+0xa is stored the device_table_t. We will use the address dev_mmio_start for the ring buffer queue. */

    /* TODO: Using the paravirtualized driver we have written for SIMVIRTIO, send
     "Ana are mere!\n" */
	/* Note that you have to go through the device setup process: writing the magic number to 0xE9,
	   doing the DEVICE_RESET and DEVICE_CONFIG phases, sending the data to the device backend in the VMM
	   using circ_bbuf_push and finally calling hlt */

	for (;;)
		asm("hlt" : /* empty */ : "a" (42) : "memory");

}
