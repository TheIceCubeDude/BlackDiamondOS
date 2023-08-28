enum disk_type {
	sata = 0,
	satapi = 1, //Maybe one day...
	nvme = 2, //Maybe one day...
	usb = 3 //Maybe one eternity...
};

struct disk {
	u8 id;
	enum disk_type type;
	u64 size;
	void* data;
	struct disk* next;
};

struct disk* first_disk;

struct disk* _make_disk() {
	struct disk* disk = malloc(sizeof(struct disk));
	u8 disk_id = 0;
	if (!first_disk) {first_disk = disk;}
	else {
		struct disk* prev_disk = first_disk;
		while (prev_disk->next) {prev_disk = prev_disk->next; disk_id++;}
		prev_disk->next = disk;
	}
	disk->id = disk_id;
	disk->next = 0;
	return disk;
}

#include "ahci.c"

void init_disk(u64 kernel) {
	_ahci_init(kernel);
	return;
}

struct disk* get_disk(u8 disk_id) {
	struct disk* disk = first_disk;
	while (disk->id != disk_id) {disk = disk->next;}
	return disk;
}

void disk_read(u8 disk, u64 lba, u64 count, void* buf) {
	//TODO make asynchronous mode???
	struct disk* disk_desc = get_disk(disk);
	switch(disk_desc->type) {
		case sata:
			_ahci_read(disk_desc, lba, count, buf);
			break;
	}
	return;
}

void disk_write(u8 disk, u64 lba, u64 count, void* buf) {
	struct disk* disk_desc = get_disk(disk);
	switch(disk_desc->type) {
		case sata:
			_ahci_write(disk_desc, lba, count, buf);
			break;
	}
	return;
}
