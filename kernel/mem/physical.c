#define LEAD_SIG 0x5EA90516AABBCC01
#define TRAIL_SIG 0x5EA90516AABBCC02

struct mmap_entry {
	u64 address;
	u64 length;
	u32 type;
	u32 attributes;
} __attribute__((packed));

struct phys_mem_block {
	u64 lead_sig;
	struct phys_mem_block* next;
	struct phys_mem_block* prev;
	u64 size;
	u64 free_mem_following;
	u64 trail_sig;
} __attribute__((packed));

void* mmap;
struct phys_mem_block* phys_first_block;
struct phys_mem_block* phys_final_block;

void _print_entry(struct mmap_entry* entry) {
	debug("E820 entry: addr=0x");
	debug_hex(entry->address);
	debug(" len=0x");
	debug_hex(entry->length);
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
	debug_dec(total_mem);
	debug(" bytes\n\r");
	return;
}

void dump_phys_mem() {
	struct phys_mem_block* block = phys_first_block;
	while (block) {
		debug("Physical memory entry: type=allocated size=0x");
		debug_hex(block->size);
		debug(" addr=0x");
		debug_hex((u64)block + sizeof(struct phys_mem_block));
		if (block->free_mem_following) {
			debug("\r\nPhysical memory entry: type=free size=0x");
			debug_hex(block->free_mem_following);
		}
		debug("\r\n");
		block = block->next;
	}

	return;
}

void _check_phys_block(struct phys_mem_block* block) {
	if ((block->lead_sig != LEAD_SIG) || (block->trail_sig != TRAIL_SIG)) {kpanic("physical memory integrity validation failed - an overflow has occured");}
	return;
}

struct phys_mem_block* _make_new_block(u64 size, struct phys_mem_block* selected_block) {
	struct phys_mem_block* new_block = (struct phys_mem_block*)((u64)selected_block + selected_block->size + sizeof(struct phys_mem_block));
	new_block->size = size - sizeof(struct phys_mem_block);
	new_block->free_mem_following = selected_block->free_mem_following - size;
	new_block->next = selected_block->next;
	new_block->prev = selected_block;
	new_block->lead_sig = LEAD_SIG;
	new_block->trail_sig = TRAIL_SIG;
	if (selected_block == phys_final_block) {phys_final_block = new_block;} 
	else {new_block->next->prev = new_block;}
	selected_block->next = new_block;
	selected_block->free_mem_following = 0;
	return new_block;
}

void* phys_alloc(u64 size) {
	//Allocates an object of a certain size, and returns a pointer to it
	//This goes forwards through the blocks, so it prefers smaller addresses, and will create blocks at the beginning of free memory.
	if (size < 16) {size = 16;}
	if (size % 16) {size += 16 - (size % 16);} //Preserve 16 byte alignments
	struct phys_mem_block* block = phys_first_block;
	struct phys_mem_block* selected_block = 0;
	size += sizeof(struct phys_mem_block);
	while (block) {
		_check_phys_block(block);
		if (		block->free_mem_following >= size && 
			       	((!selected_block) || block->free_mem_following < selected_block->free_mem_following)
		) {
			selected_block = block;
		}
		block = block->next;
	}
	if (!selected_block) {return 0;}
	struct phys_mem_block* new_block = _make_new_block(size, selected_block);
	void* ptr = (void*)((u64)new_block + sizeof(struct phys_mem_block));
	return ptr;
}

void phys_free(void* ptr) {
	struct phys_mem_block* block = (struct phys_mem_block*)((u64)ptr - sizeof(struct phys_mem_block));
	_check_phys_block(block);
	if (block == phys_final_block) {phys_final_block = block->prev;}
	else {block->next->prev = block->prev;}
	block->prev->next = block->next;
	block->prev->free_mem_following += block->size + sizeof(struct phys_mem_block) + block->free_mem_following;
	return;
}

void prep_mem(void* mmap_arg, void* kernel, u64 kernel_size) {
	//Goes through the E820 mem map and sets up a best-fit memory management system using all the memory
	//Best fit is slow, but that shouldn't matter much
	//Best fit reduces fragmentation, and is very memory efficient
	mmap = mmap_arg;
	dump_mem_info();

	//Go through memory map and create a 0-size block on each usable section
	//Kernel will not be overwritten by headers because the bootloader made sure to load it 96 bytes after the beginning of a memory region
	struct mmap_entry* entry = mmap;
	while (*((u64*)entry) != 0x55AA) {
		u8 alignment = 0;
		if (entry->address % 16) {alignment = 16 - (entry->address % 16);}
		if (entry->type == 1 && entry->address >= 0x100000 && entry->length > sizeof(struct phys_mem_block) + alignment) {
			struct phys_mem_block* block = (void*)(entry->address + alignment);
			if (!phys_first_block) {
				phys_first_block = block; 
				phys_final_block = block;
				block->next = 0; 
				block->prev = 0;
			} else {
				if ((u64)block < (u64)phys_first_block) {
					block->next = phys_first_block;
					block->prev = 0;
					phys_first_block = block;
				} else {
					struct phys_mem_block* prev_block = phys_first_block;
					while (prev_block->next && (u64)prev_block->next < (u64)block) {prev_block = prev_block->next;}
					block->prev = prev_block;
					if (prev_block == phys_final_block) {block->next = 0; phys_final_block = block;}
					else {block->next = prev_block->next;}
					prev_block->next = block;
				}
			}
			block->lead_sig = LEAD_SIG;
			block->trail_sig = TRAIL_SIG;
			block->size = 0;
			block->free_mem_following = entry->length - sizeof(struct phys_mem_block) - alignment;
		}
		entry++;
	}
	//Manually allocates kernel, so it cannot be overwritten
	struct phys_mem_block* block = phys_first_block;
	while (block && ((u64)block + (sizeof(struct phys_mem_block) * 2) != (u64)kernel)) {block = block->next;}
	if (!block) {serial_kpanic("invalid kernel location (must be 64 bytes after start of memory region)");}
	debug("Kernel offset: 0x");
	debug_hex((u64)_make_new_block(kernel_size, block));
	debug("\n");
	return;
}
