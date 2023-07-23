#define SERIAL_PORT 0x3F8

void serial_init() {
	outb(SERIAL_PORT + 1, 0); //Disable interrupts
	outb(SERIAL_PORT + 3, 0x80); //Enable DLAB (baud divisor set)
	outb(SERIAL_PORT, 1); //Set divisor low to 1
	outb(SERIAL_PORT + 1, 0); //Set divisor high to 0
	outb(SERIAL_PORT + 3, 0x03); //Set line protocol (8 bits, no parity, 1 stop bit)
	outb(SERIAL_PORT + 2, 0xC7); //Enable FIFO 
	//No point testing the controller - this is only for debug in emulators, where it is guaranteed to work
	outb(SERIAL_PORT + 4, 0x03); //Set Data Terminal Ready and Request To Send
	return;
}

void serial_put_char(u8 c) {
	while (!(inb(SERIAL_PORT + 5) & 0x20)) {} //Wait for transmit to be empty
	outb(SERIAL_PORT, c);
	return;
}

void debug(u8* str) {
	while (*str) {
		serial_put_char(*str);
		str++;
	}
	return;
}

void debug_hex(u64 val) {
	u8 buf[17];
	itoa(val, buf, 16);
	debug(buf);
	return;
}

void debug_dec(u64 val) {
	u8 buf[21];
	itoa(val, buf, 10);
	debug(buf);
	return;
}
