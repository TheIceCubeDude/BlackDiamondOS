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
extern void halt();

void serial_kpanic(u8* str) {
	debug("\n\rKernel panic: ");
	debug(str);
	halt();
}

void kpanic(u8* str) {
	debug("\n\rKernel panic: ");
	debug(str);
	while (1) {} 
	struct framebuffer active_buffer = get_screen_buffer();
	set_active_buffer(active_buffer);
	put_rect(active_buffer.pixel_width / 10 - active_buffer.pixel_width / 100, active_buffer.pixel_height / 10 - active_buffer.pixel_height / 100, active_buffer.pixel_width / 10 * 8 + active_buffer.pixel_width / 50, active_buffer.pixel_height / 10 * 8 + active_buffer.pixel_height / 50, 0x00FF0000);
	put_rect(active_buffer.pixel_width / 10, active_buffer.pixel_height / 10, active_buffer.pixel_width / 10 * 8, active_buffer.pixel_height / 10 * 8, 0);

	struct framebuffer text_box = {0, 350, 350};
	text_box.address = phys_alloc(text_box.pixel_width * text_box.pixel_height * 4); //TODO replace with malloc
	if (!text_box.address) {halt();}
	set_active_buffer(text_box);
	put_rect(0, 0, 350, 350, 0);
	set_bg_colour(0xFF000000);
	set_fg_colour(0x00FF0000);
	put_str("Kernel panic:", get_font_width(), get_font_height(), text_box.pixel_width);
	put_str(str, get_font_width(), get_font_height() * 3, text_box.pixel_width);

	struct framebuffer scaled_text_box = {0, active_buffer.pixel_width / 10 * 8, active_buffer.pixel_height / 10 * 8};
	scaled_text_box.address = phys_alloc(scaled_text_box.pixel_width * scaled_text_box.pixel_height * 4);
	if (!scaled_text_box.address) {halt();}
	scale_buffer(scaled_text_box, text_box);

	set_active_buffer(active_buffer);
	composite_buffer(scaled_text_box, active_buffer.pixel_width / 10, active_buffer.pixel_height / 10);
	swap_bufs();
	phys_free(scaled_text_box.address);
	phys_free(text_box.address);
	halt();
}

void draw_logo(void* logo_ptr) {
	struct framebuffer screen = get_screen_buffer();
	struct framebuffer logo_buf = get_targa_size(logo_ptr);
	struct framebuffer logo_resized_buf = {0, 0, 0};
	if (screen.pixel_width > screen.pixel_height) {
		logo_resized_buf.pixel_width = screen.pixel_height / 5;
	       	logo_resized_buf.pixel_height = screen.pixel_height / 5;
	} else {
		logo_resized_buf.pixel_width = screen.pixel_width / 5;
	       	logo_resized_buf.pixel_height = screen.pixel_width / 5;
	}
	logo_buf.address = phys_alloc(logo_buf.pixel_width * logo_buf.pixel_height * 4); //TODO change to malloc
	logo_resized_buf.address = phys_alloc(logo_resized_buf.pixel_width * logo_resized_buf.pixel_height * 4); //TODO change to malloc
	if ((!logo_buf.address) || (!logo_resized_buf.address)) {serial_kpanic("not enough memory to load logo");}

	set_active_buffer(logo_buf);
	if(!put_targa(0, 0, logo_ptr)) {serial_kpanic("could not load logo");}
	set_active_buffer(screen);
	scale_buffer(logo_resized_buf, logo_buf);

	put_rect(0, 0, active_buffer.pixel_width, active_buffer.pixel_height, 0x007d8ca3);
	composite_buffer(logo_resized_buf, (screen.pixel_width / 2) - (logo_resized_buf.pixel_width / 2), (screen.pixel_height / 2) - (logo_resized_buf.pixel_height / 2));
	swap_bufs();
	phys_free(logo_buf.address);
	phys_free(logo_resized_buf.address);
	return;
}

void kmain(void* video_info, void* mmap, u64 booted_partition_lba, u64 kernel_size) {
	serial_init();
	debug("HaematiteOS kernel RS232 serial debug log @ 115200 baud:\n\r");
	void* kernel = prep_mem(mmap, kernel_size);
	void* logo_ptr = (void*)((u64)&logo + (u64)kernel - 32);
	void* font_ptr = (void*)((u64)&font + (u64)kernel - 32);
	init_video(video_info);
	draw_logo(logo_ptr); //TODO change to malloc
	if (!set_font(font_ptr)) {serial_kpanic("font not supported (must not be Unicode, and must be PSF version 2)");}
	
	kpanic("System halted"); //TODO change to malloc

	//TODO: malloc & free, interrupts, pci, disk, fat32, ps2 mouse/keyboard, scheduler, dynamic linking/function table patching, genesis program/userland/desktop environment, more drivers (timers, audio, etc)
}
