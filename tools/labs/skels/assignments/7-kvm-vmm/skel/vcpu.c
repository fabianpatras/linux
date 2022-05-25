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

// you need to call `setup_protected_mode` beforehand
void setup_long_mode(virtual_cpu *vcpu) {
	struct kvm_sregs sregs;
	int rc = 0;

	struct kvm_segment seg = {
		.base = 0,
		.limit = 0xffffffff,
		.selector = 1 << 3,
		.present = 1,
		.type = 11, /* Code: execute, read, accessed */
		.dpl = 0,
		.db = 0,
		.s = 1, /* Code/data */
		.l = 1,
		.g = 1, /* 4KB granularity */
	};

	printf("[setup_long_mode]\n");
	
	rc = ioctl(vcpu->fd, KVM_GET_SREGS, &sregs);
	if (rc < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	sregs.cs = seg;

	seg.type = 3;
	seg.selector = 2 << 3;
	sregs.ds = sregs.es = sregs.fs = sregs.gs = sregs.ss = seg;

	sregs.efer = EFER_LME | EFER_LMA;
	sregs.cr4 = CR4_PAE;

	rc = ioctl(vcpu->fd, KVM_SET_SREGS, &sregs);
	if (rc < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}
}

void setup_2_lvl_paging(virtual_cpu *vcpu, virtual_machine *vm) {
	struct kvm_sregs sregs;
	int rc = 0;

	// 257-th physical page (0x101 * 0x1000)
	uint64_t page_directory_pa = 0x101000;

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

	// can't set this here because on the `KVM_SET_SREGS` ioctl in `long setup`
	// the kvm verifies if the sregs are valid an because we set ther LME and LMA
	// in efer we also need to have CR4 set properly.
	// sregs.cr4 |= CR4_PSE;

	rc = ioctl(vcpu->fd, KVM_SET_SREGS, &sregs);
	if (rc < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}
}

void setup_4_lvl_paging(virtual_cpu *vcpu, virtual_machine *vm) {
	struct kvm_sregs sregs;
	int rc = 0;

	// 257-th physical page (0x101 * 0x1000)
	uint64_t pml4_pa = 0x101000;
	// 258-th physical page (0x102 * 0x1000)
	uint64_t pdpt_pa = 0x102000;
	// 259-th physical page (0x103 * 0x1000)
	uint64_t page_directory_pa = 0x103000;

	// points to GPA `pdpt_pa`
	uint64_t pml4e = pdpt_pa
			| PDE64_PRESENT
			| PDE64_RW
			| PDE64_USER;

	// points to GPA `page_directory_pa`
	uint64_t pdpte = page_directory_pa
			| PDE64_PRESENT
			| PDE64_RW
			| PDE64_USER;

	// points to GPA 0x0
	uint64_t pde_2mb = PDE64_PRESENT
			| PDE64_RW
			| PDE64_USER
			| PDE64_PS;

	printf("[setup_4_lvl_paging]\n");

	rc = ioctl(vcpu->fd, KVM_GET_SREGS, &sregs);
	if (rc < 0) {
		perror("KVM_GET_SREGS");
		exit(1);
	}

	// writing the PDE that points to a 4MB page
	((uint64_t *)(vm->mem + pml4_pa))[0] = pml4e;
	((uint64_t *)(vm->mem + pdpt_pa))[0] = pdpte;
	((uint64_t *)(vm->mem + page_directory_pa))[0] = pde_2mb;

	// setting CR3 to GPA of the PML4
	sregs.cr3 = pml4_pa;

	sregs.cr4 = CR4_PAE;

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
