struct framebuffer {
	u32* address;
	u16 pixel_width;
	u16 pixel_height;
};

struct framebuffer front_buffer;
u16 front_buffer_width_bytes;
struct framebuffer back_buffer;
struct framebuffer active_buffer; 

void init_video(void* video_info) {
	front_buffer.address = (u32*)*((u32*)((u64)video_info + 40));
	front_buffer.pixel_width = *(u16*)((u64)video_info + 18);
	front_buffer.pixel_height = *(u16*)((u64)video_info + 20);
	front_buffer_width_bytes = *(u16*)((u64)video_info + 50);
	
	back_buffer.address = phys_alloc(front_buffer.pixel_width * front_buffer.pixel_height * 4); 
	if (!back_buffer.address) {serial_kpanic("not enough memory to create a double-buffer");}
	back_buffer.pixel_width = front_buffer.pixel_width;
	back_buffer.pixel_height = front_buffer.pixel_height;
	active_buffer = back_buffer;
	memset(active_buffer.address, 0, active_buffer.pixel_width * active_buffer.pixel_height * 4);
	debug("Resolution: ");
	debug_dec(back_buffer.pixel_width);
	debug("x");
	debug_dec(back_buffer.pixel_height);
	debug("\n\r");
	return;
}

void set_active_buffer(struct framebuffer buf) {
	active_buffer = buf;
	return;
}

struct framebuffer get_active_buffer() {
	return active_buffer;
}

struct framebuffer get_screen_buffer() {
	return back_buffer;
}

u32* get_linear_address(u16 x, u16 y) {
	if (x >= active_buffer.pixel_width || y >= active_buffer.pixel_height) {return 0;}
	return active_buffer.address + (y * active_buffer.pixel_width) + x;
}

void swap_bufs() {
	//Copy each line individually, because there may be padding at the end of each line on the front buffer
	for (u16 i = 0; i < front_buffer.pixel_height; i++) {
		memcpy(		(void*)((u64)front_buffer.address + (i * front_buffer_width_bytes)), 
				(void*)((u64)back_buffer.address + (i * back_buffer.pixel_width * 4)),
			       	back_buffer.pixel_width * 4);
	}
	return;
}
