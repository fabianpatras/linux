CFLAGS = -Wall -Wextra -Iinclude/

run: kvm-vmm
	./kvm-vmm

kvm-vmm: vmm.o vm.o vcpu.o payload.o
	$(CC) $^ -o $@

payload.o: payload.ld guest32.img.o guest64.img.o guest16.o
	$(LD) -T $< -o $@

# .o
guest16.o: guest_16_bits/guest16.s
	$(CC) $(CFLAGS) -ffreestanding -fno-pic -c -o $@ $^

guest32.o: guest_32_bits/guest_code.c 
	$(CC) $(CFLAGS) -m32 -ffreestanding -fno-pic -c -o $@ $^

guest64.o: guest_64_bits/guest_code.c 
	$(CC) $(CFLAGS) -m64 -ffreestanding -fno-pic -c -o $@ $^

# .img
guest32.img: guest32.o
	$(LD) -T guest.ld -m elf_i386 $^ -o $@

guest64.img: guest64.o
	$(LD) -T guest.ld $^ -o $@

%.img.o: %.img
	$(LD) -b binary -r $^ -o $@

.PHONY: clean
clean:
	$(RM) kvm-vmm kvm-vmm.o payload.o \
		*.o *.img
