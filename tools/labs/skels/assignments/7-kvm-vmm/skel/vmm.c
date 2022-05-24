#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
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
extern const unsigned char guest64[], guest64_end[];

int main(int argc, char **argv) {
	UNUSED_PARAMETER(argc);
	UNUSED_PARAMETER(argv);
	struct vm vm;
	struct vcpu vcpu;
	struct kvm_regs regs;
	uint64_t memval = 0;
	size_t sz;
	int api_ver;
	int rc;

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
	setup_2_lvl_paging(&vcpu, &vm);

	memcpy(vm.mem, guest16, guest16_end - guest16);
	// memcpy(vm.mem, guest64, guest64_end - guest64);

	// for (unsigned char *p = guest64, i = 1; p != guest64_end; p++, i++) {
	// 	printf("0x%02x%s", *p, i % 4 == 0 ? "\n" : " ");
	// }
	// printf("\n");
	// for (unsigned char *p = guest16, i = 1; p != guest16_end; p++, i++) {
	// 	printf("0x%02x%s", *p, i % 4 == 0 ? "\n" : " ");
	// }
	// printf("\n");

	/* TODO: IF real mode works all right. We can try to set up long mode */
	for (;;) {
		
		/* TODO: Run the VCPU with KVM_RUN */
		rc = ioctl(vcpu.fd, KVM_RUN, 0);
		if (rc) {
			perror("KVM_RUN");
			exit(1);
		}

		/* TODO: Handle VMEXITs */
		switch (vcpu.kvm_run->exit_reason) {
		case KVM_EXIT_HLT: { goto check; }
		case KVM_EXIT_FAIL_ENTRY: { 
			printf("Exit qualification [0x%16x]", vcpu.kvm_run->fail_entry.hardware_entry_failure_reason);
		}
		case KVM_EXIT_MMIO: {
			/* TODO: Handle MMIO read/write. Data is available in the shared memory at 
			vcpu->kvm_run */
		}
		case KVM_EXIT_IO: {
			/* TODO: Handle IO ports write (e.g. outb). Data is available in the shared memory
			at vcpu->kvm_run. The data is at vcpu->kvm_run + vcpu->kvm_run->io.data_offset; */
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
	memcpy(&memval, &vm.mem[0x400], sizeof(char));
	if (memval != 42) {
		printf("Wrong result: memory at 0x400 is %lld\n",
		       (unsigned long long)memval);
		return 0;
	}

	printf("%s\n", "Finished vmm");
	return 0;
} 
