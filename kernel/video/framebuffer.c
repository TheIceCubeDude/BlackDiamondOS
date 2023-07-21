struct framebuffer {
	u32* address;
	u16 pixel_width;
	u16 pixel_height;
};

const u8 BPP = 4; //Bytes Per Pixel

struct framebuffer front_buffer;
u16 front_buffer_width_bytes;
struct framebuffer back_buffer;
struct framebuffer active_buffer; 

void init_video(void* video_info) {
	front_buffer.address = (u32*)*((u32*)((u64)video_info + 40));
	front_buffer.pixel_width = *(u16*)((u64)video_info + 18);
	front_buffer.pixel_height = *(u16*)((u64)video_info + 20);
	front_buffer_width_bytes = *(u16*)((u64)video_info + 50);
	
	back_buffer.address = phys_alloc(front_buffer.pixel_width * BPP * front_buffer.pixel_height); //This can vary greatly, so phys_alloc it rather than malloc it so we don't run out of heap mem
	if (!back_buffer.address) {debug("Kernel Panic: not enough memory to create a double-buffer!"); while (1) {}}
	back_buffer.pixel_width = front_buffer.pixel_width;
	back_buffer.pixel_height = front_buffer.pixel_height;
	active_buffer = back_buffer;
	memset(active_buffer.address, 0, active_buffer.pixel_width * active_buffer.pixel_height * BPP);

	debug("Resolution: ");
	u8 str[21];
	itoa(back_buffer.pixel_width, str, 10);
	debug(str);
	debug("x");
	itoa(back_buffer.pixel_height, str, 10);
	debug(str);
	debug("\n\r");
	return;
}

void set_active(struct framebuffer buf) {
	active_buffer = buf;
	return;
}

u32* get_linear_address(u16 x, u16 y) {
	if (x >= active_buffer.pixel_width || y >= active_buffer.pixel_height) {return 0;}
	return active_buffer.address + (y * active_buffer.pixel_width) + x;
}

void swap_bufs() {
	//Copy each line individually, because there may be padding at the end of each line
	for (u16 i = 0; i < front_buffer.pixel_height; i++) {
		memcpy(		(void*)((u64)front_buffer.address + (i * front_buffer_width_bytes)), 
				(void*)((u64)back_buffer.address + (i * back_buffer.pixel_width * BPP)),
			       	back_buffer.pixel_width * BPP);
	}
	return;
}
