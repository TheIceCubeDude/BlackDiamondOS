#define PIC1_CMD 0x20
#define PIC1_DAT 0x21
#define PIC2_CMD 0xA0
#define PIC2_DAT 0xA1

struct idt_entry {
	u16 isr_low;
	u16 cs_selector;
	u8 ist;
	u8 attributes;
	u16 isr_mid;
	u32 isr_high;
	u32 reserved;
} __attribute__((packed));

struct idtr {
	u16 size;
	void* offset;
} __attribute__((packed));

struct idt_entry (*idt)[256];
struct idtr idtr;

void unhandled_int_wrapper();
void exception_wrapper();
extern u8 exception_wrapper_end;

void enable_irqs();
void disable_irqs();
void set_idt(void* idtr);

void unhandled_int() {
	debug("\nUnhandled interrupt\n");
	return;
}

u64 exception_handler(u8 interrupt) {
	switch (interrupt) {
		case 0: kpanic("exception caught: division error"); //TODO allow this to be hooked
		case 1: debug("\nexception caught: debug\n"); return 0;
		case 2: kpanic("exception caught: NMI (fatal hardware faliure");
		case 3: kpanic("exception caught: breakpoint");
		case 4: kpanic("exception caught: overflow"); //TODO allow this to be hooked
		case 5: kpanic("exception caught: bound checks failed"); //TODO allow this to be hooked
		case 6: kpanic("exception caught: invalid opcode"); //TODO plug this into the scheduler
		case 7: kpanic("exception caught: CPU extention unavaliable");
		case 8: kpanic("exception caught: double fault");
		case 11:
		case 12: kpanic("exception caught: segment does not exist");
		case 13: kpanic("exception caught: general protection fault");
		case 14: kpanic("exception caught: page fault (attempted to access memory >64GB)");
		case 16:
		case 19: kpanic("exception caught: floating point maths error"); //TODO allow this to be hooked
		default: kpanic("unhandled exception caught");
	};
	return 0;
}

void add_interrupt(u8 vector, u8 trap, void* isr) {
	u8 attributes;
	if (trap) {attributes = 0b10001111;}
	else {attributes = 0b10001110;}
	struct idt_entry *entry = &((*idt)[vector]);
	entry->isr_low = (u16)(u64)isr;
	entry->cs_selector = 0x28;
	entry->ist = 0;
	entry->attributes = attributes;
	entry->isr_mid = (u16)((u64)isr>>16);
	entry->isr_high = (u32)((u64)isr>>32);
	entry->reserved = 0;
	return;
}

void irq_eoi(u8 irq) {
	//Send end of interrupt to the PICs
	if (irq >= 8) {outb(PIC2_CMD, 0x20);}
	outb(PIC1_CMD, 0x20);
	return;
}

void irq_unmask(u8 irq) {
	if (irq < 8) {
		u8 mask = inb(PIC1_DAT);
		outb(PIC1_DAT, mask & ~(1<<irq));
	} else {
		irq -= 8;
		u8 mask = inb(PIC2_DAT);
		outb(PIC2_DAT, mask & ~(1<<irq));
	}
	return;
}

void _program_pic() {
	//Reprograms the PIC to map interrupts to int 32
	u8 icw1 = 0b00010001; //Reinitalise PIC, expect to recieve icw4
	u8 icw4 = 0b00000001; //Use 8086 mode

	//ICW1 start initalisation
	outb(PIC1_CMD, icw1); pwait();
	outb(PIC2_CMD, icw1); pwait();
	//ICW2 send new offsets
	outb(PIC1_DAT, 32); pwait();
	outb(PIC2_DAT, 40); pwait();
	//ICW3 link master + slave together via IRQ 2
	outb(PIC1_DAT, 0b00000100); pwait();
	outb(PIC2_DAT, 0b00000010); pwait();
	//ICW4 config data
	outb(PIC1_DAT, icw4); pwait();
	outb(PIC2_DAT, icw4); pwait();

	//Mask all irqs
	outb(PIC1_DAT, 0xFF);
	outb(PIC2_DAT, 0xFF);
	return;
}

void init_interrupts(void* kernel) {
	//Setup IRQs
	_program_pic();
	irq_unmask(2); //Enable PIC 2
	enable_irqs();
	//Setup IDT
	idt = malloc(sizeof(*idt));
	idtr.size = sizeof(*idt) - 1;
	idtr.offset = idt;
	for (u8 i = 0; i < 32; i++) {
		//Dynamically make exception handlers
		u64 exception_wrapper_size = (u64)&exception_wrapper_end - (u64)&exception_wrapper;
		u8* exception = malloc(exception_wrapper_size);
		if (!exception) {kpanic("could not set up exception");}
		memcpy(exception, (u8*)exception_wrapper + (u64)kernel, exception_wrapper_size);
		*(exception + 25) = i;
		*(u64*)(exception + 31) = (u64)exception_handler; //Do not add kernel offset because GCC generates PIC
		add_interrupt(i, 1, exception);
	}
	for (u8 i = 48; i < 255; i++) {
		add_interrupt(i, 0,(u8*)kernel + (u64)unhandled_int_wrapper);
	}
	set_idt(&idtr);
	return;
}
