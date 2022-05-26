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

int handle_mmio(virtual_cpu *vcpu, virtual_machine *vm) {
	printf("[handle_mmio]\n");
	UNUSED_PARAMETER(vcpu);
	UNUSED_PARAMETER(vm);
	return 0;
}

int handle_io(virtual_cpu *vcpu, virtual_machine *vm) {
	UNUSED_PARAMETER(vm);
	// printf("[handle_io]\n");
	
	uint8_t chr = ((uint8_t *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset)[0];
	if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT) {
		// printf("writing [%c] (size [%d] bytes) on port [%x]\n",
		// 	// (uint8_t) is needed because pointer arithmetic would add sizeof (ptr type) multiples
		// 	((uint8_t *)vcpu.kvm_run + vcpu.kvm_run->io.data_offset)[0],
		// 	vcpu.kvm_run->io.size,
		// 	vcpu.kvm_run->io.port);
		putc(chr ,stdout);
	}
	fflush(stdout);
	return 0;
}

int probe_and_compare_guest_memory(virtual_machine *vm, uint64_t gpa,
				   size_t data_size, uint64_t what) {
	uint64_t memval = 0;

	printf("[probe_and_compare_guest_memory]\n");

	if (gpa < vm->mem_size) {
		printf("\t > Reading from RW memory reagion\n");
		memcpy(&memval, &vm->mem[gpa], data_size);
	} else if (gpa - vm->mem_size < vm->mmio_mem_size) {
		printf("\t > Reading from MMIO memory reagion\n");
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

int main(int argc, char **argv) {
	UNUSED_PARAMETER(argc);
	UNUSED_PARAMETER(argv);
	struct vm vm;
	struct vcpu vcpu;
	struct kvm_regs regs;
	int api_ver;
	int rc;
	uint64_t code_gpa = 0x1000;

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

	// print_code(guest32, guest32_end - guest32);

	for (;;) {
		
		/* TODO: Run the VCPU with KVM_RUN */
		rc = ioctl(vcpu.fd, KVM_RUN, 0);
		if (rc) {
			perror("KVM_RUN");
			exit(1);
		}

		// printf("exit reason [%d]\n", vcpu.kvm_run->exit_reason);

		/* TODO: Handle VMEXITs */
		switch (vcpu.kvm_run->exit_reason) {
		case KVM_EXIT_HLT: { goto check; }
		case KVM_EXIT_FAIL_ENTRY: { 
			printf("Exit qualification [0x%8llx]", vcpu.kvm_run->fail_entry.hardware_entry_failure_reason);
			break;
		}
		case KVM_EXIT_MMIO: {
			/* TODO: Handle MMIO read/write. Data is available in the shared memory at 
			vcpu->kvm_run */
			handle_mmio(&vcpu, &vm);
			continue;
		}
		case KVM_EXIT_IO: {
			/* TODO: Handle IO ports write (e.g. outb). Data is available in the shared memory
			at vcpu->kvm_run. The data is at vcpu->kvm_run + vcpu->kvm_run->io.data_offset; */
			handle_io(&vcpu, &vm);
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

	/* Verify that the guest has written 42 at 0x400 */
	// memcpy(&memval, &vm.mem[0x400], 4);
	// if (memval != 42) {
	// 	printf("Wrong result: memory at 0x400 is 0x%llx\n",
	// 	       (unsigned long long)memval);
	// 	return 0;
	// }

	if (probe_and_compare_guest_memory(&vm, 0x400, 4, 42))
		return 0;

	if (probe_and_compare_guest_memory(&vm, 0x180000, 1, (uint64_t) 'x'))
		return 0;

	printf("%s\n", "Finished vmm");
	return 0;
}
