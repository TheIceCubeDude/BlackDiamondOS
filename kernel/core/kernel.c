#include "stdint.h"
#include "ports.h"
#include "utils.c"

#include "serial.c"
#include "../mem/physical.c"

void kmain(void* video_info, void* mmap, u64 booted_partition_lba, u64 kernel_size) {
	serial_init();
	debug("HaematiteOS kernel RS232 serial debug log @ 115200 baud:\n\r");
	prep_mem(mmap, kernel_size);
	while (1) {}
	//TODO: malloc & free -> graphics
}
