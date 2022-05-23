#include <stdio.h>
#include "vm.h"

void create_vm(virtual_machine *vm) {
	int rc = 0;
	printf("[create_vm]\n");

	vm->fd = ioctl(vm->sys_fd, KVM_CREATE_VM, 0);
	if (vm->fd < 0) {
		perror("KVM_CREATE_VM");
		exit(1);
	}

	vm->mem = mmap(NULL,
		       VM_MEMORY_SIZE,
		       PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
		       -1,
		       0);
	
	if (vm->mem == MAP_FAILED) {
		perror("mmap VM mem");
		exit(1);
	}

	struct kvm_userspace_memory_region mem = {
		.slot = 0,
		.guest_phys_addr = 0,
		.memory_size = VM_MEMORY_SIZE,
		.userspace_addr = (unsigned long long) vm->mem,
	};

	rc = ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &mem);
	if (rc < 0) {
		perror("KVM_SET_USER_MEMORY_REGION");
		exit(1);
	}

	rc = ioctl(vm->fd, KVM_SET_TSS_ADDR, 0xffffd000);
	if (rc < 0) {
		perror("KVM_SET_TSS_ADDR");
		exit(1);
	}
}
