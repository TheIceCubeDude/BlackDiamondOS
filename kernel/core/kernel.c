#include "stdint.h"
#include "ports.h"
#include "mem.h"
#include "utils.c"

void serial_kpanic(u8* str);
void kpanic(u8* str);

#include "serial.c"
#include "../mem/mem.c"
#include "../video/video.c"

extern u8 logo; //This is not a pointer to the logo!
extern u8 font; //This is not a pointer to the font!

void serial_kpanic(u8* str) {
	debug("\n\rKernel panic: ");
	debug(str);
	while (1) {}
}

void kpanic(u8* str) {
	put_rect(active_buffer.pixel_width / 10 - active_buffer.pixel_width / 50, active_buffer.pixel_height / 10 - active_buffer.pixel_height / 50, active_buffer.pixel_width / 10 * 8 + active_buffer.pixel_width / 25, active_buffer.pixel_height / 10 * 8 + active_buffer.pixel_height / 25, 0x00A00000);
	put_rect(active_buffer.pixel_width / 10, active_buffer.pixel_height / 10, active_buffer.pixel_width / 10 * 8, active_buffer.pixel_height / 10 * 8, 0x00FF0000);
	put_str("Kernel panic:", active_buffer.pixel_width / 10, active_buffer.pixel_height / 10, active_buffer.pixel_width / 10 * 8);
	put_str(str, active_buffer.pixel_width / 10, active_buffer.pixel_height / 10 * 2, active_buffer.pixel_width / 10 * 8);
	swap_bufs();
	while (1) {}
}

void kmain(void* video_info, void* mmap, u64 booted_partition_lba, u64 kernel_size) {
	serial_init();
	debug("HaematiteOS kernel RS232 serial debug log @ 115200 baud:\n\r");
	void* kernel = prep_mem(mmap, kernel_size);
	void* logo_ptr = (void*)((u64)&logo + (u64)kernel - 32);
	void* font_ptr = (void*)((u64)&font + (u64)kernel - 32);
	init_video(video_info);

	put_rect(0, 0, active_buffer.pixel_width, active_buffer.pixel_height, 0x007d8ca3);
	struct framebuffer logo_size = get_targa_size(logo_ptr);
	put_targa(active_buffer.pixel_width/2 - logo_size.pixel_width/2, active_buffer.pixel_height/2 - logo_size.pixel_height/2, logo_ptr);
	swap_bufs();

	if (!set_font(font_ptr)) {serial_kpanic("font not supported (must not be Unicode, and must be PSF version 2)");}
	kpanic("System halted");

	//TODO: graphics function to scale a framebuffer into another framebuffer, malloc & free, interrupts, pci, disk, fat32, ps2 mouse/keyboard, scheduler, dynamic linking/function table patching, genesis program/userland/desktop environment, more drivers (timers, audio, etc)
}
