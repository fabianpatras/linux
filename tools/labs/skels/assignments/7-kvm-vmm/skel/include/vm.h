#ifndef _VM_H_
#define _VM_H_


typedef struct vm {
	/* fd for interacting with the kvm itself e.g. for creating a VM */
	int sys_fd;
	/* fd of the actual VM */
	int fd;
	/* Memory of the VM */
	char *mem;
} virtual_machine;

#endif