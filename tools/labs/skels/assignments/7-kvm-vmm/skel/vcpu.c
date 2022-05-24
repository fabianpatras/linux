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

	sregs.cr0 = 0ll;
	flags = CR0_PE
		| CR0_MP
		// | CR0_EM
		// | CR0_TS
		| CR0_ET
		| CR0_NE
		// | CR0_WP
		| CR0_AM;
		// | CR0_NW
		// | CR0_CD
		// | CR0_PG;
	sregs.cr0 = flags;
	// printf("[cr0][0x%16llx]\n", sregs.cr0);
	// printf("[cr4][0x%16llx]\n", sregs.cr4);

	// sregs.efer &= ~(EFER_LME | EFER_LMA);
	sregs.efer = 0;

	sregs.cr3 = 0;

	// enables 4MB pages -> no need for PAE
	sregs.cr4 = CR4_VMXE | CR4_PSE;

	sregs.cs.selector = 0x8;
	sregs.cs.base = 0;
	sregs.cs.limit = 0xffff;
	sregs.cs.present = 1;
	sregs.cs.dpl = 0;
	sregs.cs.s = 1; // code or data
	// sregs.cs.db = 1; // 32-bit segment
	sregs.cs.type = 8 + 2 + 1; // code + read + accessed

	sregs.ds.selector = 0x10;
	sregs.ds.base = 0;
	sregs.ds.limit = 0xffff;
	sregs.ds.present = 1;
	sregs.ds.dpl = 0;
	sregs.ds.s = 1; // code or data
	// sregs.ds.db = 1; // 32-bit segment
	sregs.ds.type = 2 + 1; // write + accessed

	sregs.es.selector = 0x10;
	sregs.es.base = 0;
	sregs.es.limit = 0xffff;
	sregs.es.present = 1;
	sregs.es.dpl = 0;
	sregs.es.s = 1; // code or data
	// sregs.es.db = 1; // 32-bit segment
	sregs.es.type = 2 + 1; // write + accessed

	sregs.fs.selector = 0x10;
	sregs.fs.base = 0;
	sregs.fs.limit = 0xffff;
	sregs.fs.present = 1;
	sregs.fs.dpl = 0;
	sregs.fs.s = 1; // code or data
	// sregs.fs.db = 1; // 32-bit segment
	sregs.fs.type = 2 + 1; // write + accessed

	sregs.gs.selector = 0x10;
	sregs.gs.base = 0;
	sregs.gs.limit = 0xffff;
	sregs.gs.present = 1;
	sregs.gs.dpl = 0;
	sregs.gs.s = 1; // code or data
	// sregs.gs.db = 1; // 32-bit segment
	sregs.gs.type = 2 + 1; // write + accessed

	sregs.ss.selector = 0x10;
	sregs.ss.base = 0;
	sregs.ss.limit = 0xffff;
	sregs.ss.present = 1;
	sregs.ss.dpl = 0;
	sregs.ss.s = 1; // code or data
	// sregs.ss.db = 1; // 32-bit segment
	sregs.ss.type = 2 + 1; // write + accessed

	sregs.tr.selector = 0x18;
	sregs.tr.base = 0;
	sregs.tr.limit = 0xffff;
	sregs.tr.present = 1;
	sregs.tr.dpl = 0;
	sregs.tr.s = 0; // system segment
	// sregs.tr.db = 1; // 32-bit segment
	sregs.tr.type = 0x3; // 16-bit TSS (Busy)

	sregs.gdt.base = 0;
	sregs.gdt.limit = 0x20;

	sregs.idt.base = 0;
	sregs.idt.limit = 0;

	// ar = 0x8082
	sregs.ldt.base = 0;
	sregs.ldt.limit = 0x0;
	// sregs.ldt.present = 1;
	// sregs.ldt.type = 2; // LDT
	sregs.ldt.unusable = 1;

	regs.rflags = 0x2;
	regs.rip = 0x0;
	regs.rsp = 0x0;

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

void setup_2_lvl_paging(virtual_cpu *vcpu, virtual_machine *vm) {
	struct kvm_sregs sregs;
	struct kvm_regs regs;
	int rc = 0;
	uint64_t page_directory_pa = 0x10 * 0x1000; // 16-th physical page
	
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

	rc = ioctl(vcpu->fd, KVM_GET_REGS, &regs);
	if (rc < 0) {
		perror("KVM_GET_REGS");
		exit(1);
	}

	// writing the PDE that points to a 4MB page
	*(uint64_t *)(vm->mem + page_directory_pa) = pde_4mb;

	// setting CR3 to GPA of the Page Directory
	sregs.cr3 = page_directory_pa;

	rc = ioctl(vcpu->fd, KVM_SET_SREGS, &sregs);
	if (rc < 0) {
		perror("KVM_SET_SREGS");
		exit(1);
	}
}
