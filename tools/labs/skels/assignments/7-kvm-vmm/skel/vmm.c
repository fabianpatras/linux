#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <linux/kvm.h>
#include <stdlib.h>

#include "vm.h"
#include "vcpu.h"
#include "device.h"

/* The linker scripts sets this values to the guest code. This is
   the code that we will write on the VM. Basically the .o files.
   Look into payload.ld to see how these are created.
*/
extern const unsigned char guest16[], guest16_end[];

// used for 32-bit protected mode
extern const unsigned char guest32[], guest32_end[];
extern const unsigned char guest64[], guest64_end[];

const unsigned char hlt_code[] = { 0xf4 };

void print_code(const unsigned char *code, uint64_t len) {
	uint64_t i;
	for (i = 0; i < len; i++) {
		printf("0x%02x%s", code[i], i % 4 == 0 ? "\n" : " ");
	}
	printf("\n");
}

int handle_mmio(virtual_cpu *vcpu, virtual_machine *vm, device_t *simvirt_dev) {
	// printf("[handle_mmio]\n");
	uint64_t memval = 0;
	uint64_t mmio_mem_offset;

	// printf("\t> %s len [%d] addr [0x%lx]\n",
	// 	vcpu->kvm_run->mmio.is_write ? "writing" : "reading",
	// 	vcpu->kvm_run->mmio.len,
	// 	vcpu->kvm_run->mmio.phys_addr);

	if (vcpu->kvm_run->mmio.phys_addr < vm->mem_size) {
		printf("\t> Wrong address??\n");
		exit(0);
	}

	mmio_mem_offset = vcpu->kvm_run->mmio.phys_addr - VM_MMIO_START;

	if (mmio_mem_offset > vm->mmio_mem_size) {
		printf("\t> Outside of MMIO region somehow\n");
		exit(0);
	}

	memcpy(&memval, &vcpu->kvm_run->mmio.data, vcpu->kvm_run->mmio.len);
	// printf("offset [%d] char [%c] hex [0x%x]\n", mmio_mem_offset,
	// 	(char) memval, memval);

	if (simvirt_dev->driver_status != DRIVER_OK) {
		if ((char) memval == 'R') {
			printf("Am primit 'R'\n");
			simvirt_dev->device_status = DEVICE_RESET;
		}
	}

	printf("%c", (char) memval);

	return 0;
}

int handle_io(virtual_cpu *vcpu, virtual_machine *vm, device_t *simvirt_dev) {
	UNUSED_PARAMETER(vm);
	UNUSED_PARAMETER(simvirt_dev);
	// uint32_t memval = 0;
	uint8_t chr = 0;

	// printf("\t> writing [%c] [0x%x] (size [%d] bytes) on port [%x]\n",
	// 	// (uint8_t) is needed because pointer arithmetic would add sizeof (ptr type) multiples
	// 	((uint8_t *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset)[0],
	// 	((uint8_t *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset)[0],
	// 	vcpu->kvm_run->io.size,
	// 	vcpu->kvm_run->io.port);

	if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT) {
		// outb/outl

		if (vcpu->kvm_run->io.size != 4) {
			chr = ((uint8_t *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset)[0];
			putc(chr ,stdout);
			// printf("We need outl, not outb\n");
			fflush(stdout);
			return 0;
		}
	}

	return 0;
}

int main(int argc, char **argv) {
	UNUSED_PARAMETER(argc);
	UNUSED_PARAMETER(argv);
	struct vm vm;
	struct vcpu vcpu;
	struct kvm_regs regs;
	int api_ver;
	int rc;
	uint64_t code_gpa = 0x1000;
	uint64_t vm_exits = 0;
	device_t simvirt_dev = {
		.device_status = 0,
		.driver_status = 0,
		.magic = 0,
		.max_queue_len = 0
	};

	vm.sys_fd = open("/dev/kvm", O_RDWR);
	if (vm.sys_fd < 0) {
		perror("open /dev/kvm");
		exit(1);
	}

	api_ver = ioctl(vm.sys_fd, KVM_GET_API_VERSION, 0);
	if (api_ver != KVM_API_VERSION) {
		perror("KVM_GET_API_VERSION");
		exit(1);
	}

	/* TODO: Initialize the VM. We will use 0x100000 bytes for the memory */
	create_vm(&vm);

	/* TODO: Initialize the VCPU */
	create_vcpu(&vcpu, &vm);

	/* TODO: Setup real mode. We will use guest_16_bits to test this. */
	// setup_real_mode(&vcpu);
	setup_protected_mode(&vcpu);

	/* TODO: IF real mode works all right. We can try to set up long mode */
	setup_long_mode(&vcpu);
	// setup_2_lvl_paging(&vcpu, &vm);
	setup_4_lvl_paging(&vcpu, &vm);

	// memcpy(vm.mem, guest16, guest16_end - guest16);
	// copy_to_vm_pa(&vm, guest16, guest16_end - guest16, code_gpa);
	// copy_to_vm_pa(&vm, guest32, guest32_end - guest32, code_gpa);
	copy_to_vm_pa(&vm, guest64, guest64_end - guest64, code_gpa);
	// copy_to_vm_pa(&vm, hlt_code, 1, code_gpa);

	set_rip(&vcpu, code_gpa);

	for (;;) {
		
		/* TODO: Run the VCPU with KVM_RUN */
		rc = ioctl(vcpu.fd, KVM_RUN, 0);
		if (rc) {
			perror("KVM_RUN");
			exit(1);
		}
		vm_exits++;

		/* TODO: Handle VMEXITs */
		switch (vcpu.kvm_run->exit_reason) {
		case KVM_EXIT_HLT: {
			goto check; 
		}
		case KVM_EXIT_FAIL_ENTRY: { 
			printf("Exit qualification [0x%8llx]", vcpu.kvm_run->fail_entry.hardware_entry_failure_reason);
			break;
		}
		case KVM_EXIT_MMIO: {
			/* TODO: Handle MMIO read/write. Data is available in the shared memory at 
			vcpu->kvm_run */
			handle_mmio(&vcpu, &vm, &simvirt_dev);
			continue;
		}
		case KVM_EXIT_IO: {
			/* TODO: Handle IO ports write (e.g. outb). Data is available in the shared memory
			at vcpu->kvm_run. The data is at vcpu->kvm_run + vcpu->kvm_run->io.data_offset; */
			handle_io(&vcpu, &vm, &simvirt_dev);
			continue;
		}
		}
		fprintf(stderr,	"\nGot exit_reason %d,"
			" expected KVM_EXIT_HLT (%d)\n",
			vcpu.kvm_run->exit_reason, KVM_EXIT_HLT);
		exit(1);
	}

	/* We verify that the guest code ran accordingly */
	check:
	if (ioctl(vcpu.fd, KVM_GET_REGS, &regs) < 0) {
		perror("KVM_GET_REGS");
		exit(1);
	}

	/* Verify that the guest has written 42 to RAX |*/
	if (regs.rax != 42) {
		printf("Wrong result: {E,R,}AX is %lld\n", regs.rax);
		return 0;
	} else {
		printf("Ok result: {E,R,}AX is %lld\n", regs.rax);
	}

	if (probe_and_compare_guest_memory(&vm, 0x400, 4, 42))
		return 0;

	printf("There were a total of [%ld] vm exits\n", vm_exits);
	printf("%s\n", "Finished vmm");
	return 0;
}
