#include <stddef.h>
#include <stdint.h>
#include "queue.h"
#include "device.h"

// Phys addr memory layout:
// Code: [0x1000, sizeof(code))
// Heap: (Guest Memory): [0x010_000, xxx)
// Stack: [xxx, 0x100_000) - grows down from 0x100_000
// MMIO Devices: [0x100_000, 0x101_000)
static const uint64_t heap_phys_addr = 0x010000;
static const uint64_t dev_mmio_start = 0x100000;
simqueue_t g2h_queue;

/* Initializes a queue */
void create_q(uint64_t data_offset, int size, queue_control_t *qc)
{
    g2h_queue.maxlen = size;
    g2h_queue.q_ctrl = qc;
    g2h_queue.buffer = (void*)data_offset;
}

/* Helper functions */
static inline uint32_t inl(uint16_t port)
{
	uint32_t v;
	asm volatile("inl %1,%0" : "=a" (v) : "dN" (port));
	return v;
}

static inline void outl(uint16_t port, uint32_t v)
{
	asm volatile("outl %0,%1" : : "a" (v), "dN" (port));
}

static void outb(uint16_t port, uint8_t value) {
	asm("outb %0,%1" : /* empty */ : "a" (value), "Nd" (port) : "memory");
}

static inline uint8_t inb(uint16_t port) {
	uint8_t ret;

	asm volatile("inb %1, %0": "=a"(ret): "Nd"(port) );
	return ret;
}

void
__attribute__((noreturn))
__attribute__((section(".start")))
_start(void) {

    /* Our VM doesn't have malloc or anything, we simply use the memory */
	char *p;

	for (p = "Hello, world!\n"; *p; ++p)
		outb(0xE9, *p);

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
