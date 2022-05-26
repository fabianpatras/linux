#ifndef _VM_H_
#define _VM_H_

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <linux/kvm.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED_PARAMETER(P)     ((void)(P))

// memory layout
// [0; VM_MEMORY_SIZE) -- read write guest physical space
//                   [VM_MEMORY_SIZE; VM_MEMORY_SIZE + VM_MMIO_SIZE) -- READ only MMIO

#define VM_MEMORY_SIZE 0x180000 // 1,5MB -> first 3/4 of 1 x 2MB page (with PAE)
#define VM_MMIO_SIZE   0x080000 // 0,5MB -> last 1/4 of 1 x 2MB page (with PAE)

typedef struct vm {
	/* fd for interacting with the kvm itself e.g. for creating a VM */
	int sys_fd;
	/* fd of the actual VM */
	int fd;
	/* Memory of the VM */
	char *mem;
	/* Memory size */
	uint64_t mem_size;

	/* MMIO region of the VM */
	char *mmio_mem;
	/* Memory size */
	uint64_t mmio_mem_size;
} virtual_machine;

void create_vm(virtual_machine *vm);
void copy_to_vm_pa(virtual_machine *vm, const unsigned char *from,
		   size_t len, size_t offset);

#endif