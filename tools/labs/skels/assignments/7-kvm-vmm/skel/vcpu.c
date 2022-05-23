#include <stdio.h>
#include "vcpu.h"

void create_vcpu(virtual_cpu *vcpu, virtual_machine *vm) {
	uint64_t memval = 0;

	printf("[create_vcpu]\n");
	vcpu->fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
	if (vcpu->fd < 0) {
		perror("KVM_CREATE_VCPU");
		exit(1);
	}

	memval = ioctl(vm->sys_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
	if (memval == 0) {
		perror("KVM_GET_VCPU_MMAP_SIZE");
		exit(1);
	}

	vcpu->kvm_run = mmap(NULL,
			     memval,
			     PROT_READ | PROT_WRITE,
			     MAP_SHARED,
			     vcpu->fd,
			     0);

	if (vcpu->kvm_run == MAP_FAILED) {
		perror("mmap vCPU mem");
		exit(1);
	}
}

void setup_real_mode(virtual_cpu *vcpu) {
	struct kvm_sregs sregs;
	struct kvm_regs regs;
	int rc = 0;

	printf("[setup_real_mode]\n");
	
	rc = ioctl(vcpu->fd, KVM_GET_SREGS, &sregs);
	if (rc < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	rc = ioctl(vcpu->fd, KVM_GET_REGS, &regs);
	if (rc < 0) {
		perror("KVM_GET_REGS");
		exit(1);
	}

	sregs.cr0 &= ~CR0_PE;
	// printf("[cr0][0x%16llx]\n", sregs.cr0);
	// printf("[cr4][0x%16llx]\n", sregs.cr4);

	sregs.cr0 &= ~(EFER_LME | EFER_LMA);
	// printf("[efer][0x%16llx]\n", sregs.efer);

	sregs.cr3 = 0;

	sregs.cs.selector = 0;
	sregs.cs.base = 0;
	sregs.cs.limit = 0xffff;

	sregs.gdt.base = 0;
	sregs.gdt.limit = 0xffff;

	regs.rflags = 0x2;
	regs.rip = 0x0;

	rc = ioctl(vcpu->fd, KVM_SET_SREGS, &sregs);
	if (rc < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}

	rc = ioctl(vcpu->fd, KVM_SET_REGS, &regs);
	if (rc < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}
}
