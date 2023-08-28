//This heap implementation is a slab allocator
#define SLAB_SIZE 4096
struct heap {
	u64 padding;
	struct heap_cache* free_space_for_cache; //Free list of caches
	struct heap_slab* free_space_for_slab;  //Free list of slabs
	//TODO if variable size slabs are added (for allocations larger than slab_size bytes) to this in the future, free space will have to be managed using another underlying memory allocator (buddy allocator?)
	u8* free_space;
	struct heap_cache* first_cache;
	void* heap_end;
} __attribute__((packed));
struct heap_cache {
	struct heap_cache* next;
	u32 padding;
	u32 size;
	struct heap_slab* avaliable_slabs;
	struct heap_slab* full_slabs;
}__attribute__((packed));
struct heap_slab {
	struct heap_slab* next;
	u64 padding;
	u32 allocated_objects;
	u32 max_objects;
	void* first_free_object;
	u8 objects[SLAB_SIZE];
}__attribute__((packed));

struct heap* active_heap;

void heap_init(u32 size) {
	active_heap->free_space_for_cache = 0;
	active_heap->free_space_for_slab = 0;
	active_heap->free_space = (u8*)active_heap + sizeof(struct heap);
	active_heap->first_cache = 0;
	active_heap->heap_end = (void*)((u64)active_heap + size);
	return;
}

void set_active_heap(void* heap) {
	active_heap = (struct heap*)heap;
	return;
}

struct heap_cache* _make_cache(u32 size) {
	//If a cache has previously been freed, allocate the free space left behind and remove it from the free list of freed caches
	//Otherwise allocate a cache from free_space, as long as we don't pass the end of the heap
	struct heap_cache* cache;
	if (active_heap->free_space_for_cache) {
		cache = active_heap->free_space_for_cache;
		active_heap->free_space_for_cache = *(struct heap_cache**)active_heap->free_space_for_cache;
	} else {
		if (active_heap->free_space + sizeof(struct heap_cache) > (u8*)active_heap->heap_end) {return 0;}
		cache = (struct heap_cache*)active_heap->free_space;
		active_heap->free_space += sizeof(struct heap_cache);
	}
	//Initalise cache elements
	cache->next = 0;
	cache->size = size;
	cache->avaliable_slabs = 0;
	cache->full_slabs = 0;
	return cache;
}

struct heap_slab* _make_slab(u32 size) {
	//If a slab has previously been freed, allocate the free space left behind and remove it from the free list of freed slabs
	//Otherwise allocate a slab from free_space, as long as we don't pass the end of the heap
	struct heap_slab* slab;
	if (active_heap->free_space_for_slab) {
		slab = active_heap->free_space_for_slab;
		active_heap->free_space_for_slab = *(struct heap_slab**)active_heap->free_space_for_slab;
	} else {
		if (active_heap->free_space + sizeof(struct heap_slab) > (u8*)active_heap->heap_end) {return 0;}
		slab = (struct heap_slab*)active_heap->free_space;
		active_heap->free_space += sizeof(struct heap_slab);
	}
	//Initalise slab elements
	slab->next = 0;
	slab->allocated_objects = 0;
	slab->max_objects = SLAB_SIZE / size;
	slab->first_free_object = slab->objects;
	//Initalise object free list
	for (u16 i = 0; i < slab->max_objects; i++) {
		void** object = (void**)(slab->objects + (i * size));
		if (i < slab->max_objects) {*object = slab->objects + ((i + 1) * size);}
		else {*object = 0;}
	}
	return slab;
}

void* malloc(u32 size) {
	//Make sure object is larger than 8 bytes so a pointer could fit in it
	if (size < 8) {size = 8;}
	//Make sure object is no larger than slab_size, otherwise pass the request to the physical memory manager (maybe change to a budd allocator later?)
	if (size > SLAB_SIZE) {return phys_alloc(size);}
	//Find cache of the desired size, otherwise make it, and if we can't make it we have run out of memory (so return 0)
	if (!active_heap->first_cache) {active_heap->first_cache = _make_cache(size);}
	struct heap_cache* cache = active_heap->first_cache;
	while (cache->next && cache->size != size) {cache = cache->next;}
	if (cache->size != size) {cache->next = _make_cache(size); cache = cache->next;}
	if (!cache) {return 0;}
	//Allocate object onto the avaliable slab, and if an avaliable slab doesn't exist, make it
	if (!cache->avaliable_slabs) {
		cache->avaliable_slabs = _make_slab(size);
		if (!cache->avaliable_slabs) {return 0;}
	}
	struct heap_slab* slab = cache->avaliable_slabs;
	void* object = slab->first_free_object;
	slab->first_free_object = *(void**)slab->first_free_object;
	slab->allocated_objects++;
	//If slab is now full, move it to the full list
	if (slab->allocated_objects == slab->max_objects) {
		cache->avaliable_slabs = cache->avaliable_slabs->next;
		if (!cache->full_slabs) {
			cache->full_slabs = slab;
		} else {
			struct heap_slab* final_full_slab = cache->full_slabs;
			while (final_full_slab->next) {final_full_slab = final_full_slab->next;}
			final_full_slab->next = slab;
		}
		slab->next = 0;
	}
	return object;
}	

void* cmalloc(u32 size) {
	//Malloc but initalises everything to 0
	void* ptr = malloc(size);
	if (ptr) {memset(ptr, 0, size);}
	return ptr;
}

u8 _check_slab_for_pointer(void* alloc, struct heap_slab* slab, u32 size) {
	//Check if this slab contains alloc
	if (((u64)slab < (u64)alloc) && ((u64)slab + sizeof(struct heap_slab) >= (u64)alloc)) {
		//If this slab is full, mark alloc as the first free object
		//Otherwise find the free object before alloc, make it point to alloc, and make alloc point to the free object after alloc
		if (slab->allocated_objects == slab->max_objects) {
			slab->first_free_object = alloc;
			*(u64*)alloc = 0;
		} else {
			void** object = (void**)slab->first_free_object;
			while ((!((u64)*object > (u64)alloc)) && object) {object = (void**)*object;}
			void* next_object = *object;
			*(void**)alloc = next_object;
			*object = alloc;
		}
		return 1;
	}
	return 0;
}

struct heap_slab* _search_slab_list(void* alloc, struct heap_slab* slab, u32 size) {
	while (slab) {
		if (_check_slab_for_pointer(alloc, slab, size)) {return slab;}
		slab = slab->next;
	}
	return 0;
}

void free(void* alloc) {
	//Search every cache
	struct heap_cache* cache = active_heap->first_cache;
	struct heap_slab* slab = 0;
	u8 full = 0;
	while (cache) {
		//Search every avaliable and full slab
		slab = _search_slab_list(alloc, cache->avaliable_slabs, cache->size);
		if (slab) {break;}
		slab = _search_slab_list(alloc, cache->full_slabs, cache->size);
		if (slab) {full = 1; break;}
		cache = cache->next;
	}
	if (!slab) {phys_free(alloc); return;} //We didn't find alloc in any slabs, so pass the request to the physical memory manager (see above)
	//Update slab elements
	slab->allocated_objects--;
	//If the slab was full, mark it as not longer full
	if (full) {
		if (slab != cache->full_slabs) {
			struct heap_slab* prev_slab = cache->full_slabs;
			while (prev_slab->next != slab) {prev_slab = prev_slab->next;}
			prev_slab->next = slab->next;
		} else {cache->full_slabs = slab->next;}
		if (!cache->avaliable_slabs) {cache->avaliable_slabs = slab;}
		else {
			struct heap_slab* final_slab = cache->avaliable_slabs;
			while (final_slab->next) {final_slab = final_slab->next;}
			final_slab->next = slab;
		}
	}
	//If the slab is now empty, remove it from the list of avaliable slabs and add it to the list of free slabs
	if (!slab->allocated_objects) {	
		if (slab != cache->avaliable_slabs) {
			struct heap_slab* prev_slab = cache->avaliable_slabs;
			while (prev_slab->next != slab) {prev_slab = prev_slab->next;}
			prev_slab->next = slab->next;
		} else {cache->avaliable_slabs = slab->next;}
		slab->next = 0;
		if (!active_heap->free_space_for_slab) {active_heap->free_space_for_slab = slab;}
		else {
			struct heap_slab* final_slab = active_heap->free_space_for_slab;
			while (final_slab->next) {final_slab = final_slab->next;}
			final_slab->next = slab;
		}
		//If the cache is now empty, remove it from the list of caches and add it to the list of free caches
		if ((!cache->avaliable_slabs) && (!cache->full_slabs)) {
			if (cache != active_heap->first_cache) {
				struct heap_cache* prev_cache = active_heap->first_cache;
				while (prev_cache->next != cache) {prev_cache = prev_cache->next;}
				prev_cache->next = cache->next;
			} else {active_heap->first_cache = cache->next;}
			cache->next = 0;
			if (!active_heap->free_space_for_cache) {active_heap->free_space_for_cache = cache;}
			else {
				struct heap_cache* final_cache = active_heap->free_space_for_cache;
				while (final_cache->next) {final_cache = final_cache->next;}
				final_cache->next = cache;
			}
		}
	}
	return;
}

void dump_heap() {
	debug("Heap dump: slab_size=");
	debug_dec(SLAB_SIZE);

	debug(" free_caches=");
	struct heap_cache* freed_cache = active_heap->free_space_for_cache;
	u16 free_caches = 0;
	while (freed_cache) {freed_cache = freed_cache->next; free_caches++;}
	debug_dec(free_caches);

	debug(" free_slabs=");
	struct heap_slab* freed_slab = active_heap->free_space_for_slab;
	u16 free_slabs = 0;
	while (freed_slab) {freed_slab = freed_slab->next; free_slabs++;}
	debug_dec(free_slabs);

	debug(" free_space=");
	debug_dec((u64)active_heap->heap_end - (u64)active_heap->free_space);

	struct heap_cache* cache = active_heap->first_cache;
	while (cache) {
		debug("\nCache: size=");
		debug_dec(cache->size);

		debug(" full slabs=");
		u16 slab_count = 0;
		struct heap_slab* slab = cache->full_slabs;
		while (slab) {slab = slab->next; slab_count++;}
		debug_dec(slab_count);
		
		slab = cache->avaliable_slabs;
		while (slab) {
			debug("\n\tSlab: objects=");
			debug_dec(slab->allocated_objects);
			debug(" max_objects=");
			debug_dec(slab->max_objects);
			slab = slab->next;
		}
		cache = cache->next;
	}
	debug("\n");
	return;
}
