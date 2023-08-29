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

u8 strcmp(u8* str1, u8* str2, u16 len) {
	//Checks if 2 strings match
	//If len=0, continue until NULL terminator
	if (!len) {
		while (*str1 == *str2) {
			if (!*str1) {return 1;}
			str1++;
			str2++;
		}
		return 0;
	}
	for (; len; len--) {
		if (*str1 != *str2) {return 0;}
		str1++;
		str2++;
	}
	return 1;
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
