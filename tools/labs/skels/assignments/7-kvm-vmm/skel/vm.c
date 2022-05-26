#include <stdio.h>
#include "vm.h"

void create_vm(virtual_machine *vm) {
	int rc = 0;
	uint64_t map_addr = 0xffffc000;
	printf("[create_vm]\n");

	vm->fd = ioctl(vm->sys_fd, KVM_CREATE_VM, 0);
	if (vm->fd < 0) {
		perror("KVM_CREATE_VM");
		exit(1);
	}

	rc = ioctl(vm->sys_fd, KVM_CHECK_EXTENSION, KVM_CAP_NR_MEMSLOTS);
	if (rc <= 0) {
		perror("KVM_CHECK_EXTENSION");
		exit(1);
	}

	// guest RW physical address space
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

	// guest MMIO physical address space
	vm->mmio_mem = mmap(NULL,
		       VM_MMIO_SIZE,
		       PROT_READ,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
		       -1,
		       0);
	
	if (vm->mmio_mem == MAP_FAILED) {
		perror("mmap VM mmio_mem");
		exit(1);
	}

	rc = madvise(vm->mem, VM_MEMORY_SIZE, MADV_MERGEABLE);
	if (rc) {
		perror("madvise VM mem");
		exit(1);
	}

	rc = madvise(vm->mmio_mem, VM_MMIO_SIZE, MADV_MERGEABLE);
	if (rc) {
		perror("madvise VM mmio_mem");
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

	mem.slot = 1;
	// VM will see MMIO directly after RW mem
	mem.guest_phys_addr = VM_MEMORY_SIZE;
	mem.flags = KVM_MEM_READONLY;
	mem.memory_size = VM_MMIO_SIZE;
	mem.userspace_addr = (unsigned long long) vm->mmio_mem,

	rc = ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &mem);
	if (rc < 0) {
		perror("KVM_SET_USER_MEMORY_REGION");
		exit(1);
	}

	vm->mem_size = VM_MEMORY_SIZE;
	vm->mmio_mem_size = VM_MMIO_SIZE;

	rc = ioctl(vm->fd, KVM_SET_TSS_ADDR, 0xffffd000);
	if (rc < 0) {
		perror("KVM_SET_TSS_ADDR");
		exit(1);
	}

	rc = ioctl(vm->fd, KVM_SET_IDENTITY_MAP_ADDR, &map_addr);
	if (rc < 0) {
		perror("KVM_SET_IDENTITY_MAP_ADDR");
		exit(1);
	}
}

void copy_to_vm_pa(virtual_machine *vm, const unsigned char *from,
		   size_t len, size_t offset) {
	printf("[copy_to_vm_pa]\n");
	if (offset + len > vm->mem_size) {
		perror("copy_to_vm_pa not enough space");
		exit(1);
	}
	memcpy(vm->mem + offset, from, len);
}

int probe_and_compare_guest_memory(virtual_machine *vm, uint64_t gpa,
				   size_t data_size, uint64_t what) {
	uint64_t memval = 0;

	// printf("[probe_and_compare_guest_memory]\n");

	if (gpa < vm->mem_size) {
		// printf("\t > Reading from RW memory reagion\n");
		memcpy(&memval, &vm->mem[gpa], data_size);
	} else if (gpa - vm->mem_size < vm->mmio_mem_size) {
		// printf("\t > Reading from MMIO memory reagion\n");
		memcpy(&memval, &vm->mmio_mem[gpa - vm->mem_size], data_size);
	} else {
		printf("\t > Trying to read from outside of VM memory space\n");
		printf("\t > Eiting...\n");
		exit(0);
	}

	if (memval != what) {
		printf("Wrong result: memory at [0x%lx] is 0x%llx\n",
			gpa,
		       (unsigned long long)memval);
		return 1;
	}

	return 0;
}
