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
#define VM_MEMORY_SIZE 0x200000 // 2MB -> exactly 1 x 2MB page (with PAE)

typedef struct vm {
	/* fd for interacting with the kvm itself e.g. for creating a VM */
	int sys_fd;
	/* fd of the actual VM */
	int fd;
	/* Memory of the VM */
	char *mem;
	/* Memory size */
	uint64_t mem_size;
} virtual_machine;

void create_vm(virtual_machine *vm);
void copy_to_vm_pa(virtual_machine *vm, const unsigned char *from,
		   size_t len, size_t offset);

#endif