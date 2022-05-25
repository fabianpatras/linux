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

	sregs.efer &= ~(EFER_LME | EFER_LMA);
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

void setup_protected_mode(virtual_cpu *vcpu) {
	struct kvm_sregs sregs;
	struct kvm_regs regs;
	int rc = 0;
	int flags = 0;

	struct kvm_segment seg = {
		.base = 0,
		.limit = 0xffffffff,
		.selector = 1 << 3,
		.present = 1,
		.type = 11, /* Code: execute, read, accessed */
		.dpl = 0,
		.db = 1,
		.s = 1, /* Code/data */
		.l = 0,
		.g = 1, /* 4KB granularity */
	};

	printf("[setup_protected_mode]\n");
	
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

	sregs.cr0 |= CR0_PE
		| CR0_MP
		| CR0_ET
		| CR0_NE
		| CR0_AM
		| CR0_PG;

	// sregs.efer = 0;
	sregs.cr3 = 0;

	// enables 4MB pages -> no need for PAE
	sregs.cr4 = CR4_PSE;

	sregs.cs = seg;

	seg.type = 3;
	seg.selector = 2 << 3;
	sregs.ds = sregs.es = sregs.fs = sregs.gs = sregs.ss = seg;

	memset(&regs, 0, sizeof(struct kvm_regs));
	regs.rflags = 0x2;
	regs.rip = 0x0;
	regs.rsp = 0x100000;

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

void msr_stuff(virtual_cpu *vcpu, virtual_machine *vm) {
	int rc, i, j;
	struct kvm_msr_list msr_list;
	struct kvm_sregs sregs;
	uint32_t indices[100];
	uint32_t crX_fixedY[4] = { 0x486, 0x487, 0x488, 0x489};

#define MSR_CR0_FIXED0 0x486
#define MSR_CR0_FIXED1 0x487
#define MSR_CR1_FIXED0 0x488
#define MSR_CR1_FIXED1 0x489

	msr_list.nmsrs = 50;

	// this struct won't be used outside the lifetime of this function
	msr_list.nmsrs = indices;

	printf("[msr_stuff]\n");

	rc = ioctl(vcpu->fd, KVM_GET_SREGS, &sregs);
	if (rc < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	rc = ioctl(vm->sys_fd, KVM_GET_MSR_FEATURE_INDEX_LIST, &msr_list);
	if (rc) {
		perror("KVM_GET_MSR_FEATURE_INDEX_LIST");
		exit(1);
	}

	for (i = 0; i < msr_list.nmsrs; i++) {
		printf("[0x%x]%s", msr_list.indices[i], i % 8 == 0 ? "\n" : " ");
	}
	printf("\n");

	for (i = 0; i < 4; i++) {
		for (j = 0; j < msr_list.nmsrs; j++) {
			if (msr_list.indices[j] == crX_fixedY[i]) {
				break;
			}
		}

		if (j == msr_list.nmsrs) {
			printf("MSR [0x%x] not found\n", crX_fixedY[i]);
			// not found
			continue;
		}

		switch (crX_fixedY[i])
		{
		case MSR_CR0_FIXED0:
			printf("[MSR_CR0_FIXED0]");
			break;
		
		default:
			break;
		}
	}
}

void setup_2_lvl_paging(virtual_cpu *vcpu, virtual_machine *vm) {
	struct kvm_sregs sregs;
	int rc = 0;

	// 257-th physical page (0x101 * 0x1000)
	// literally the first page
	uint64_t page_directory_pa = 0x0;

	// points to GPA 0x0
	uint64_t pde_4mb = PDE32_PRESENT
			| PDE32_RW
			| PDE32_USER
			| PDE32_PS;

	printf("[setup_2_lvl_paging]\n");

	rc = ioctl(vcpu->fd, KVM_GET_SREGS, &sregs);
	if (rc < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	// writing the PDE that points to a 4MB page
	((uint64_t *)(vm->mem + page_directory_pa))[0] = pde_4mb;

	// setting CR3 to GPA of the Page Directory
	sregs.cr3 = page_directory_pa;

	sregs.cr4 |= CR4_PSE;

	rc = ioctl(vcpu->fd, KVM_SET_SREGS, &sregs);
	if (rc < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}
}

void set_rip(virtual_cpu *vcpu, uint64_t rip) {
	struct kvm_regs regs;
	int rc = 0;
	
	rc = ioctl(vcpu->fd, KVM_GET_REGS, &regs);
	if (rc < 0) {
		perror("KVM_GET_REGS");
		exit(1);
	}
	
	regs.rip = rip;

	rc = ioctl(vcpu->fd, KVM_SET_REGS, &regs);
	if (rc < 0) {
		perror("KVM_SET_REGS");
		exit(1);
	}
}
