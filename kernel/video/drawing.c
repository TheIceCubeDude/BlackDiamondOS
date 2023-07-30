u8 put_pixel(u16 x, u16 y, u32 colour) {
	u32* pixel = get_linear_address(x, y);
	if (!pixel) {return 0;}
	*pixel = colour;
	return 1;
}

u32 get_pixel(u16 x, u16 y) {
	return *get_linear_address(x, y);
}

void put_rect(u16 x, u16 y, u16 width, u16 height, u32 colour) {
	u32* base = get_linear_address(x, y);
	for (u16 i = 0; i < height; i++) {
		for (u16 j = 0; j < width; j++) {
			*(base + j) = colour;
		}
		base += active_buffer.pixel_width;
	}
	return;
}

