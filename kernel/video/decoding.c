struct targa_header {
	u8 id_length;
	u8 colour_map_type;
	u8 image_type;
	u16 colour_map_first_entry_index;
	u16 colour_map_length;
	u8 colour_map_entry_size;
	u16 x_origin;
	u16 y_origin;
	u16 width;
	u16 height;
	u8 bits_per_pixel;
	u8 image_descriptor;
} __attribute__((packed));

struct framebuffer get_targa_size(void* data) {
	struct framebuffer targa = {0, ((struct targa_header*)data)->width, ((struct targa_header*)data)->height};
	return targa;
}

//Draws a Truevision TGA (TARGA) file
//The image must have 1 byte per colour channel
u8 put_targa(u16 x, u16 y, void* data) {
	struct targa_header* header = data;
	switch(header->image_type) {
		case 1:{//Uncompressed colour-mapped
			//Check file has a colour map, and is using 1 byte per colour channel (24 bits or 32 bits)
			if (!header->colour_map_type || header->colour_map_entry_size == 16) {return 0;}
			//Find colour map region and indices region
			u64 colour_map = (u64)data + sizeof(struct targa_header) + header->id_length; 
			u64 image_data = colour_map + (header->colour_map_length * (header->colour_map_entry_size / 8));
			u32 pixel_number;
			//Draw image bottom-up if origin is bottom left
			if (!(header->image_descriptor & 32)) {pixel_number = header->width * (header->height - 1);} 
			else {pixel_number = 0;}
			for (u16 i = 0; i < header->height; i++) {
				for (u16 j = 0; j < header->width; j++) {
					u16 index;
					//Check if indices are 8 bits long, and grab the next index
					if (header->bits_per_pixel == 8) {index = *(u16*)(image_data + pixel_number); index &= 0x00FF;} 
					//Otherwise it is 15 or 16 bits long - either way treat as 16 bits long, and grab the next index
					else {index = *(u16*)(image_data + (pixel_number * 2));} 
					//Check if colours are 24 bits long, and grab the next colour
					u32 colour;
					if (header->colour_map_entry_size == 24) {colour = *(u32*)(colour_map + (index * 3)) & 0x00FFFFFF;}
					//Otherwise it is 32 bits long, and grab the next colour
					else {colour = *(u32*)(colour_map + (index * 4));} 
					//Draw pixel
					put_pixel(j + x, i + y, colour);
					pixel_number++;
				}
				//Draw image bottom-up if origin is bottom left
				if (!(header->image_descriptor & 32)) {pixel_number -= header->width * 2;}
			}
			break;
		       }
		case 2:{//Uncompressed true-colour
			//Check file is using 1 byte per colour channel (24 bits or 32 bits)
			if (header->bits_per_pixel == 16) {return 0;}
			//Find data region
			u64 image_data = (u64)data + sizeof(struct targa_header) + header->id_length; 
			if (header->colour_map_type) {image_data += header->colour_map_length * (header->colour_map_entry_size / 8);}
			u32 pixel_number;
			//Draw image bottom-up if origin is bottom left
			if (!(header->image_descriptor & 32)) {pixel_number = header->width * (header->height - 1);} 
			else {pixel_number = 0;}
			for (u16 i = 0; i < header->height; i++) {
				for (u16 j = 0; j < header->width; j++) {
					//Check if colours are 24 bits long, and grab the next colour
					u32 colour;
					if (header->bits_per_pixel == 24) {colour = *(u32*)(image_data + (pixel_number * 3)) & 0x00FFFFFF;}
					//Otherwise it is 32 bits long, and grab the next colour
					else {colour = *(u32*)(image_data + (pixel_number * 4));} 
					//Draw pixel
					put_pixel(j + x, i + y, colour);
					pixel_number++;
				}
				//Draw image bottom-up if origin is bottom left
				if (!(header->image_descriptor & 32)) {pixel_number -= header->width * 2;}
			}
			break;
		       }
		case 9:{//Run-length-encoded colour-mapped 
			//Check file has a colour map, and is using 1 byte per colour channel (24 bits or 32 bits)
			if (!header->colour_map_type || header->colour_map_entry_size == 16) {return 0;}
			//Find colour map region and indices region
			u64 colour_map = (u64)data + sizeof(struct targa_header) + header->id_length; 
			u64 image_data = colour_map + (header->colour_map_length * (header->colour_map_entry_size / 8));
			u32 pixel_x = 0;
			u32 pixel_y = 0;
			u8 end = 0;
			u8* index_ptr = (u8*)image_data;
			//Draw image bottom-up if origin is bottom left
			if (!(header->image_descriptor & 32)) {pixel_y = header->height;} 
			while (!end) {
				//Check if packet header indicates RLE
				if (*index_ptr & 0x80) {
					//Grab run-length 
					u8 run = (*index_ptr & 0x7F ) + 1;
					//Grab index
					index_ptr++;
					u16 index = *(u16*)index_ptr;
					//If indices are 8 bits, dispose of high 8 bits 
					//Increase index ptr
					if (header->bits_per_pixel == 8) {index &= 0x00FF; index_ptr++;} 
					else {index_ptr += 2;}
					//Check if colours are 24 bits long, and grab the next colour
					u32 colour;
					if (header->colour_map_entry_size == 24) {colour = *(u32*)(colour_map + (index * 3)) & 0x00FFFFFF;}
					//Otherwise it is 32 bits long, and grab the next colour
					else {colour = *(u32*)(colour_map + (index * 4));} 
					for (; run > 0; run--) {
						//Draw pixel
						put_pixel(pixel_x + x, pixel_y + y, colour);
						pixel_x++;
						if (pixel_x == header->width) {
							//Draw image bottom-up if origin is bottom left
							if (!(header->image_descriptor & 32)) {
								pixel_y--;
								if (!pixel_y) {end = 1;}
							} else {
								pixel_y++;
								if (pixel_y == header->height) {end = 1;}
							}
							pixel_x = 0;
						}
					}
				//Else it is raw
				} else {
					//Grab run-length
					u8 run = *index_ptr + 1;
					index_ptr++;
					for (; run > 0; run--) {
						//Grab index
						u16 index = *(u16*)index_ptr;
						//If indices are 8 bits, dispose of high 8 bits
						//Increase index ptr
						if (header->bits_per_pixel == 8) {index &= 0x00FF; index_ptr++;} 
						else {index_ptr += 2;}
						//Check if colours are 24 bits long, and grab the next colour
						u32 colour;
						if (header->colour_map_entry_size == 24) {colour = *(u32*)(colour_map + (index * 3)) & 0x00FFFFFF;}
						//Otherwise it is 32 bits long, and grab the next colour
						else {colour = *(u32*)(colour_map + (index * 4));} 
						//Draw pixel
						put_pixel(pixel_x + x, pixel_y + y, colour);
						pixel_x++;
						if (pixel_x == header->width) {
							//Draw image bottom-up if origin is bottom left
							if (!(header->image_descriptor & 32)) {
								pixel_y--;
								if (!pixel_y) {end = 1;}
							} else {
								pixel_y++;
								if (pixel_y == header->height) {end = 1;}
							}
							pixel_x = 0;
						}
					}
				}
			}
			break;
		       }
		case 10:{ //Run-length-encoded true-colour
			//Check file is using 1 byte per colour channel (24 bits or 32 bits)
			if (header->bits_per_pixel == 16) {return 0;}
			//Find data region
			u64 image_data = (u64)data + sizeof(struct targa_header) + header->id_length; 
			if (header->colour_map_type) {image_data += header->colour_map_length * (header->colour_map_entry_size / 8);}
			u32 pixel_x = 0;
			u32 pixel_y = 0;
			u8 end = 0;
			u8* data_ptr = (u8*)image_data;
			//Draw image bottom-up if origin is bottom left
			if (!(header->image_descriptor & 32)) {pixel_y = header->height;} 
			while (!end) {
				//Check if packet header indicates RLE
				if (*data_ptr & 0x80) {
					//Grab run-length 
					u8 run = (*data_ptr & 0x7F ) + 1;
					data_ptr++;
					//Check if colours are 24 bits long, and grab the next colour
					u32 colour;
					if (header->colour_map_entry_size == 24) {colour = *(u32*)data_ptr & 0x00FFFFFF; data_ptr += 3;}
					//Otherwise it is 32 bits long, and grab the next colour
					else {colour = *(u32*)data_ptr; data_ptr += 4;} 
					for (; run > 0; run--) {
						//Draw pixel
						put_pixel(pixel_x + x, pixel_y + y, colour);
						pixel_x++;
						if (pixel_x == header->width) {
							//Draw image bottom-up if origin is bottom left
							if (!(header->image_descriptor & 32)) {
								pixel_y--;
								if (!pixel_y) {end = 1;}
							} else {
								pixel_y++;
								if (pixel_y == header->height) {end = 1;}
							}
							pixel_x = 0;
						}
					}
				//Else it is raw
				} else {
					//Grab run-length
					u8 run = *data_ptr + 1;
					data_ptr++;
					for (; run > 0; run--) {
						//Check if colours are 24 bits long, and grab the next colour
						u32 colour;
						if (header->colour_map_entry_size == 24) {colour = *(u32*)data_ptr & 0x00FFFFFF; data_ptr += 3;}
						//Otherwise it is 32 bits long, and grab the next colour
						else {colour = *(u32*)data_ptr; data_ptr += 4;} 
						//Draw pixel
						put_pixel(pixel_x + x, pixel_y + y, colour);
						pixel_x++;
						if (pixel_x == header->width) {
							//Draw image bottom-up if origin is bottom left
							if (!(header->image_descriptor & 32)) {
								pixel_y--;
								if (!pixel_y) {end = 1;}
							} else {
								pixel_y++;
								if (pixel_y == header->height) {end = 1;}
							}
							pixel_x = 0;
						}
					}
				}
			}
			 break;
			}
		default: return 0;
	}
	return 1;
}
