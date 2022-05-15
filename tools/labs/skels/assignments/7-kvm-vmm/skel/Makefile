CFLAGS = -Wall -Wextra -Iinclude/

run64: kvm-hello-world
	./kvm-hello-world

kvm-hello-world: vmm.o payload.o
	$(CC) $^ -o $@

payload.o: payload.ld guest64.img.o guest16.o
	$(LD) -T $< -o $@

guest64.o: guest_32_bits/guest_code.c 
	$(CC) $(CFLAGS) -m64 -ffreestanding -fno-pic -c -o $@ $^

guest16.o: guest_16_bits/guest16.s
	$(CC) $(CFLAGS) -m64 -ffreestanding -fno-pic -c -o $@ $^

guest64.img: guest64.o
	$(LD) -T guest.ld $^ -o $@

%.img.o: %.img
	$(LD) -b binary -r $^ -o $@

.PHONY: clean
clean:
	$(RM) kvm-hello-world kvm-hello-world.o payload.o \
		*.o *.img
