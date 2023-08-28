void itoa(u64 i, u8* buf, u8 base) {
        //Supports binary, hexadecimal, and everything in between
        if (!i) {buf[0] = '0'; buf[1] = 0; return;}
        u64 tmp_i;
        u8* tmp_buf = buf;
        buf[0] = 0;
        buf++;
        do {
                tmp_i = i;
                i /= base;
                *buf = "0123456789ABCDEF"[tmp_i - (i * base)];
                buf++;
        } while (i);

        //Flip buf
        while (tmp_buf < buf) {
                buf--;
                u8 reserve = *tmp_buf;
                *tmp_buf = *buf;
                *buf = reserve;
                tmp_buf++;
        }
        return;
}

u32 round(f32 i) {
	if (i > 0) {
		return (u32)(i + 0.5);
	} else if (i < 0) {
		return (u32)(i - 0.5);

	} else {return (u32)i;}
}

void pwait() {
	//Port wait (1-4ms delay)
	//Port 0x80 is unused after POST
	outb(0x80, 0);
	return;
}

void* align(void* address, u32 alignment) {
	//Aligns a memory address
	//Useful for aligning memory allocations for PCI DMA, and similar things
	u32 remainder = ((u64)address) % alignment;
	if (remainder) {
		address = (void*)((u64)address + (alignment - remainder));
	}
	return address;
}
