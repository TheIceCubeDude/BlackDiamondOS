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
