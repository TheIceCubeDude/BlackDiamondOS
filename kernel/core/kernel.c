#include "stdint.h"
#include "ports.h"
#include "mem.h"
#include "utils.c"

#include "serial.c"
#include "../mem/mem.c"
#include "../video/video.c"

extern u8 logo; //This is not a pointer to the logo!

void kmain(void* video_info, void* mmap, u64 booted_partition_lba, u64 kernel_size) {
	serial_init();
	debug("HaematiteOS kernel RS232 serial debug log @ 115200 baud:\n\r");
	void* kernel = prep_mem(mmap, kernel_size);
	void* logo_ptr = (void*)((u64)&logo + (u64)kernel - 32);
	init_video(video_info);

	put_rect(0, 0, active_buffer.pixel_width, active_buffer.pixel_height, 0x007d8ca3);
	put_targa(active_buffer.pixel_width/2 - 270, active_buffer.pixel_height/2 - 305, logo_ptr, 1);
	swap_bufs();

	//TODO: put_text, graphics function to scale a framebuffer into another framebuffer, malloc & free, pci, disk, fat32, ps2 mouse/keyboard, scheduler, dynamic linking/function table patching, genesis program/userland/desktop environment, more drivers (audio, timers, etc)
}
