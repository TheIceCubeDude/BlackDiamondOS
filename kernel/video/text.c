struct psf_header {
	u32 magic;
	u32 version;
	u32 header_size;
	u32 flags;
	u32 glyph_count;
	u32 glyph_size;
	u32 height;
	u32 width;
};

const u32 transparent = 0xFF000000;

struct psf_header* active_font;
u32 background_colour = transparent;
u32 foreground_colour = 0x00FFFFFF;

//Set a PSF v2 font
u8 set_font(void* font_ptr) {
	//Only supports ASCII (will fail if font contains a unicode table)
	//Only supports PSF version 2  (will fail if font is PSF version 1)
	struct psf_header* font = font_ptr;
	if (font->magic != 0x864ab572 || !font->flags) {return 0;}
	active_font = font;
	return 1;
}

void set_bg_colour(u32 colour) {
	background_colour = colour;
	return;
}

void set_fg_colour(u32 colour) {
	foreground_colour = colour;
	return;
}

void put_char(u8 c, u16 x, u16 y) {
	u8* glyph = (u8*) active_font + active_font->header_size + (c * active_font->glyph_size);
	u32* target = active_buffer.address + (y * active_buffer.pixel_width) + x;
	for (u32 i = 0; i < active_font->height; i++) {
		for (u32 j = 0; j < active_font->width; j++) {
			//Width may span multiple bytes, but between lines is rounded to the nearest byte
			u32 byte_offset = (i * (active_font->width/8)) + (j/8);
			//Test bit for pixel we want to draw
			if (*(glyph + byte_offset) & 0x80>>(j%8)) {
				if (foreground_colour != transparent) {*target = foreground_colour;}
			} else {
				if (background_colour != transparent) {*target = background_colour;}
			}
			target++;
		}
		target += active_buffer.pixel_width - active_font->width;
	}
	return;
}

void put_str(u8* str, u16 x, u16 y, u16 width) {
	u16 x_cursor = x;
	while (*str) {
		put_char(*str, x_cursor, y);
		x_cursor += active_font->width + 1;
		if (x_cursor + active_font->width >= width) {y += active_font->height; x_cursor = x;}
		str++;
	}
	return;
}

u32 get_font_width() {return active_font->width;}
u32 get_font_height() {return active_font->height;}
