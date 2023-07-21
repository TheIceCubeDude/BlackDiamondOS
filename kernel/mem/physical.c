struct mmap_entry {
	u64 address;
	u64 length;
	u32 type;
	u32 attributes;
} __attribute__((packed));

struct phys_mem_block {
	struct phys_mem_block* next;
	struct phys_mem_block* prev;
	u64 size;
	u64 free_mem_following;
} __attribute__((packed));

void* mmap;
void* phys_mem_start;
struct phys_mem_block* phys_mem_final_block;

void _print_entry(struct mmap_entry* entry) {
	debug("E820 entry: addr=0x");
	u8 str[17];
	itoa(entry->address, str, 16);
	debug(str);
	debug(" len=0x");
	itoa(entry->length, str, 16);
	debug(str);
	debug(" type=");
	switch (entry->type) {
		case 1: debug("usable"); break;
		case 2:
		case 4: debug("unusable"); break;
		case 3: debug("reclaimable"); break;
		case 5: debug("bad"); break;
		default: debug("unknown"); break;
	}
	debug(";\n\r");
	return;
}

void dump_mem_info() {
	u64 total_mem = 0;
	void* mmap_ptr = mmap;
	while (*((u64*)mmap_ptr) != 0x55AA) {
		struct mmap_entry* entry = mmap_ptr;

		_print_entry(entry);
		if (entry->type == 1) {
			total_mem += entry->length;
		}

		mmap_ptr = (void*)(((u64)mmap_ptr) + 24);
	}
	debug("Total usable memory = ");
	u8 str[21];
	itoa(total_mem, str, 10);
	debug(str);
	debug(" bytes\n\r");
	return;
}

void dump_phys_mem() {
	struct phys_mem_block* block = phys_mem_start;
	u8 str[17];
	while (block) {
		debug("Physical memory entry: type=allocated size=0x");
		itoa(block->size, str, 16);
		debug(str);
		if (block->free_mem_following) {
			debug("\r\nPhysical memory entry: type=free size=0x");
			itoa(block->free_mem_following, str, 16);
			debug(str);
		}
		debug("\r\n");
		block = block->next;
	}

	return;
}

void* phys_alloc(u64 size) {
	//Allocates an object of a certain size, and returns a pointer to it
	//This goes backwards through the blocks, so it prefers larger addresses, and will create blocks at the end of free memory. This is because we want to reserve smaller addresses (those under 2 gigs) for code, which can be allocated using phys_alloc_2gb
	struct phys_mem_block* block = phys_mem_final_block;
	struct phys_mem_block* selected_block = 0;
	size += sizeof(struct phys_mem_block);
	while (block) {
		if (block->free_mem_following >= size && ((!selected_block) || block->free_mem_following < selected_block->free_mem_following)) {
			selected_block = block;
		}
		block = block->prev;
	}
	if (!selected_block) {return 0;}
	struct phys_mem_block* new_block = (struct phys_mem_block*)((u64)selected_block + selected_block->size + sizeof(struct phys_mem_block) + selected_block->free_mem_following - size);
	if (!selected_block->next) {phys_mem_final_block = new_block;}
	new_block->size = size - sizeof(struct phys_mem_block);
	new_block->free_mem_following = 0;
	new_block->next = selected_block->next;
	new_block->prev = selected_block;
	if (new_block->next) {new_block->next->prev = new_block;}
	selected_block->next = new_block;
	selected_block->free_mem_following -= size;
	void* ptr = (void*)((u64)new_block + sizeof(struct phys_mem_block));
	return ptr;
}

void* phys_alloc_2gb(u64 size) {
	//Allocates an object of a certain size, and returns a pointer to it
	//This goes forwards through the blocks, so it prefers smaller addresses, and will create blocks at the beginning of free memory. This is because we want to only return addresses under 2 gigs for code
	struct phys_mem_block* block = (struct phys_mem_block*)phys_mem_start;
	struct phys_mem_block* selected_block = 0;
	size += sizeof(struct phys_mem_block);
	while (block) {
		if (		block->free_mem_following >= size && 
				(u64)block + block->size + size + (sizeof(struct phys_mem_block) * 2) < 0x80000000 && //Make sure it is <2gb
			       	((!selected_block) || block->free_mem_following < selected_block->free_mem_following)
		) {
			selected_block = block;
		}
		block = block->next;
	}
	if (!selected_block) {return 0;}
	struct phys_mem_block* new_block = (struct phys_mem_block*)((u64)selected_block + selected_block->size + sizeof(struct phys_mem_block));
	if (!selected_block->next) {phys_mem_final_block = new_block;}
	new_block->size = size - sizeof(struct phys_mem_block);
	new_block->free_mem_following = selected_block->free_mem_following - size;
	new_block->next = selected_block->next;
	new_block->prev = selected_block;
	if (new_block->next) {new_block->next->prev = new_block;}
	selected_block->next = new_block;
	selected_block->free_mem_following = 0;
	void* ptr = (void*)((u64)new_block + sizeof(struct phys_mem_block));
	return ptr;
}

void phys_free(void* ptr) {
	struct phys_mem_block* block = (struct phys_mem_block*)((u64)ptr - sizeof(struct phys_mem_block));
	block->prev->next = block->next;
	block->next->prev = block->prev;
	block->prev->free_mem_following += block->size + sizeof(struct phys_mem_block) + block->free_mem_following;
	return;
}

void* prep_mem(void* mmap_arg, u64 kernel_size) {
	//Goes through the E820 mem map and sets up a best-fit memory management system using all the memory
	//Best fit is slow, but that shouldn't matter much
	//Best fit reduces fragmentation, and is very memory efficient
	mmap = mmap_arg;
	dump_mem_info();

	//Go through memory map and create a 0-size block on each usable section
	//Kernel will not be overwritten because the bootloader made sure to load it 32 bytes after the beginning of a memory region
	void* mmap_ptr = mmap;
	struct phys_mem_block* prev_block = 0;
	while (*((u64*)mmap_ptr) != 0x55AA) {
		struct mmap_entry* entry = mmap_ptr;
		if (entry->type == 1 && entry->address >= 0x100000 && entry->length >= sizeof(struct phys_mem_block)) {
			struct phys_mem_block* block = (struct phys_mem_block*)entry->address;
			if (prev_block) {prev_block->next = block;}
			else {phys_mem_start = (void*)entry->address;}
			block->prev = prev_block;
			block->size = 0;
			block->free_mem_following = entry->length - sizeof(struct phys_mem_block);
			prev_block = block;
		}
		mmap_ptr = (void*)(((u64)mmap_ptr) + 24);
	}
	prev_block->next = 0;
	phys_mem_final_block = prev_block;
	void* kernel = phys_alloc_2gb(kernel_size - sizeof(struct phys_mem_block)); //Allocate kernel memory so it cannot be overwritten
	return kernel;
}
