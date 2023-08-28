//Structure of an AHCI controller:
//			> Recieved FIS								> Command FIS
//	> Ports	--------|			> Command header -------> Command table --------|
// HBA	|		> Command list	--------|						> Physical Region Descriptor Table
//	> More ports				> More command headers
//

struct ahci_hba_port_prdt_entry {
	void* data_base;
	u32 reserved;
	u32 byte_count : 22;
	u32 reserved2 : 9;
	u32 interrupt : 1;
}__attribute__((packed));

struct ahci_hba_port_cmdtable {
	u8 command_fis[64];
	u8 atapi_cmd[16];
	u8 reserved[48];
	struct ahci_hba_port_prdt_entry prdt; //Only 1 entry is used because we typically use a file system, and clusters are < 4MB
}__attribute__((packed));

struct ahci_hba_port_cmdheader {
	u8 command_fis_length : 5;
	u8 atapi : 1;
	u8 write : 1;
	u8 prefetchable : 1;
	u8 reset : 1;
	u8 bist : 1;
	u8 clear_busy : 1;
	u8 reserved : 1;
	u8 port_multiplier_port : 4;
	u16 prdt_length;
	u32 prd_byte_count;
	struct ahci_hba_port_cmdtable* command_table_base;
	u32 reserved2[4];
}__attribute__((packed));

struct ahci_hba_port_regs {
	struct ahci_hba_port_cmdheader* command_list_base;
	void* recieved_fis_base;
	u32 interrupt_status;
	u32 interrupt_enable;
	u32 command_status;
	u32 reserved1;
	u32 task_file_data;
	u32 signature;
	u32 sata_status;
	u32 sata_control;
	u32 sata_error;
	u32 sata_active;
	u32 command_issue;
	u32 sata_notification;
	u32 fis_switching_control;
	u32 device_sleep;
	u32 reserved2[10];
	u32 vendor[4];
}__attribute__((packed));

struct ahci_hba_generic_host_ctrl {
	u32 capabilities;
	u32 global_control;
	u32 interrupt_status;
	u32 ports_implemented;
	u32 version;
	u32 command_completion_coalescing_control;
	u32 command_completion_coalescing_ports;
	u32 enclosure_management_location;
	u32 enclosure_management_control;
	u32 extended_capabilities;
	u32 bios_os_handoff;
	u8 reserved[0xA0-0x2C];
	u8 vendor_specific[0x100-0xA0];
	struct ahci_hba_port_regs ports;
}__attribute__((packed));

struct ahci_fis_reg_h2d {
	u8 type; //0x27
	u8 port_multiplier_port : 4;
	u8 reserved : 3;
	u8 command_control : 1;
	u8 command;
	u8 features_low;
	u8 lba_low_low;
	u8 lba_mid_low;
	u8 lba_hi_low;
	u8 device;
	u8 lba_low_hi;
	u8 lba_mid_hi;
	u8 lba_hi_hi;
	u8 features_hi;
	u16 sector_count;
	u8 isochronous_command_completion;
	u8 control;
	u32 reserved3;
}__attribute__((packed));

struct ahci_data {
	struct ahci_controller* controller;
	u8 port;
};

struct ahci_controller {
	void* abar;
	u32 ports; //Each bit represents a port
	struct ahci_controller* next; 
};

struct ahci_controller* first_ahci_controller;

void __ahci_isr_wrapper();

void __ahci_isr() {
	debug("AHCI IRQ\n");
	//irq_eoi?
	return;
}

u8 __send_cmd(struct ahci_hba_port_regs* port, u8 write, u8 atapi, u8 fis_dword_count, void* cmd, void* data, u32 byte_count) {
	//Check byte_count is valid (even and <4MB)
	if (byte_count && (byte_count % 2 || byte_count > 4194304)) {return 0;}
	//Find an empty command slot
	u8 slot = 32;
	for (u8 i = 0; i < 32; i++) {
		u32 slot_mask = 1 << i;
		if ((!(port->sata_active & slot_mask)) && (!(port->command_issue & slot_mask))) {slot = i; break;}
	}
	debug_dec(slot);
	if (slot == 32) {return 0;}
	//Build the command header
	struct ahci_hba_port_cmdheader* header = port->command_list_base + slot;
	header->command_fis_length = fis_dword_count;
	header->atapi = atapi;
	header->write = write;
	//Build the command table
	struct ahci_hba_port_cmdtable* table = header->command_table_base;
	if (!atapi) {memcpy(table, cmd, fis_dword_count * 4);} //Set CFIS
	else {memcpy(table, cmd, fis_dword_count * 4 + 16);} //Set CFIS and ACMD (ATAPI Command)
	//Build the PRDT
	struct ahci_hba_port_prdt_entry *prdt = &(table->prdt);
	prdt->data_base = data;
	prdt->byte_count = byte_count - 1;
	prdt->interrupt = 1;
	//Queue and issue the command
	u32 slot_mask = 1 << slot;
	port->sata_active |= slot_mask;
	port->command_issue |= slot_mask;
	return 1;
}

void __register_ahci_disk(struct ahci_controller* controller, u8 port_no, struct ahci_hba_port_regs* port) {
	//Get disk type through the SIG field TODO use identify device instead???
	enum disk_type disk_type;
	switch (port->signature) {
		case 0x101: disk_type = sata; debug("SATA\n"); break;
		case 0xEB140101: disk_type = satapi; debug("SATAPI\n"); return; //TODO SATAPI at the moment is not supported
		default: kpanic("Unrecognised AHCI disk type connected"); 
	}
	//Adds a new disk to the list
	struct disk* disk = _make_disk();
	struct ahci_data* disk_data = malloc(sizeof(struct ahci_data));
	disk_data->controller = controller;
	disk_data->port = port_no;
	disk->data = disk_data;
	disk->type = disk_type;
	//Sends IDENTIFY command to the disk to get its size
	struct ahci_fis_reg_h2d* cfis = cmalloc(sizeof(struct ahci_fis_reg_h2d));
	void* data = cmalloc(512);
	cfis->type = 0x27;
	cfis->command_control = 1;
	cfis->command = 0xEC;
	__send_cmd(port, 0, 0, 5, cfis, data, 512);
	free(cfis);

	while (1) {}//TODO wait for irq
	free(data);
	return;
}

void __init_port(struct ahci_hba_port_regs* port) {
	//Disable port command execution and FIS recieving
	port->command_status &= ~1;
	while (port->command_status & 0b1000000000000000) {} //Wait until command execution engine stops
	port->command_status &= ~16;
	while (port->command_status & 0b100000000000000) {} //Wait until the FIS recieving engine stops
	//Setup data structures and their registers
	//Command tables are 128 bytes aligned, recieved fis 256 and command list 1024
	void* recieved_fis_unaligned = cmalloc(255 + 255);
	void* recieved_fis = align(recieved_fis_unaligned, 256);
	struct ahci_hba_port_cmdheader *command_list_unaligned = cmalloc(sizeof(struct ahci_hba_port_cmdheader) * 32 + 1023);
	struct ahci_hba_port_cmdheader *command_list = align(command_list_unaligned, 1024);
	for (u8 i = 0; i < 32; i++) {
		struct ahci_hba_port_cmdheader *header = command_list + i;
		struct ahci_hba_port_cmdtable *table_unaligned = cmalloc(sizeof(struct ahci_hba_port_cmdtable) + 127);
		struct ahci_hba_port_cmdtable *table = align(table_unaligned, 128);
		header->command_table_base = table;
		header->prdt_length = 1;
		header->clear_busy = 1;
	}
	port->recieved_fis_base = recieved_fis;
	port->command_list_base = command_list;
	//Reset the port
	u32 control = port->sata_control;
	control &= ~0b1111;
	control |= 1;
	port->sata_control = control;
	pwait();
	//Enable port command exeuction and FIS recieving
	port->command_status |= 16;
	while (!(port->command_status & 0b100000000000000)) {} //Wait until the FIS recieving engine starts
	port->command_status |= 1;
	while (!(port->command_status & 0b1000000000000000)) {} //Wait until command execution engine starts
	//Enable port interrupts
	port->interrupt_enable = 0b01111101100000000000000000110111;
	return;
}

void __setup_ports(u32 ports, struct ahci_hba_generic_host_ctrl* hba, struct ahci_controller* controller) {
	//Initalise all implemented ports, and check if there is a disk attached to them
	for (u8 i = 0; i < 32; i++) {
		if (ports & (1 << i)) {
			struct ahci_hba_port_regs* port = &(hba->ports) + i;
			__init_port(port);
			u32 status = port->sata_status;
			if (((status & 0xF) == 3) && (((status >> 8) & 0xF) == 1)) { //Interface power active, and device detected with communication established
				debug("AHCI device found: type=");
				__register_ahci_disk(controller, i, port);
			} else if (status) {debug("Faulty/slow/unpowered/sleeping SATA drive detected\n");}
		}
	}
	return;
}

u8 __init_controller(struct pci_device* device, struct ahci_hba_generic_host_ctrl* hba, u64 kernel) {
	//Enable PCI interrupts, DMA, and memory space access
	u16 pci_cmd = pci_get_command(device);
	pci_cmd &= ~0b10000000000;
	pci_cmd |= 0b110;
	pci_set_command(device, pci_cmd);
	//Claim ownership from the BIOS (if supported)
	if (hba->extended_capabilities & 1) {
		hba->bios_os_handoff |= 0b00010;
		while (hba->bios_os_handoff & 0b00001) {} //Wait for the BIOS to disown the controller
		while (hba->bios_os_handoff & 0b10000) {} //Wait for BIOS activity to stop on the controller
	}
	//Reset the controller
	hba->global_control |= 1;
	while (hba->global_control & 1) {} //Wait until the bit resets
	//Register PCI IRQ handler
	//TODO replace with MSI/MSI-X after implementing APIC. Make sure to remove kernel parameter
	//add_interrupt(TODO, 0, __ahci_isr_wrapper + kernel);
	//Enable AHCI mode if this supports legacy mode
	if (!(hba->capabilities & 0x40000)) { //Bit 18
		hba->global_control |= 0x80000000; //Bit 31
	}
	//Enable controller interrupts
	hba->global_control |= 2;
	//Ensure controller supports 64 bit addressing
	if (!(hba->capabilities & 0x80000000)) {return 0;} //Bit 31
	return 1;
}

void __make_ahci_controller(struct pci_device* device, u64 kernel) {
	//Setup the controller
	void* abar = (void*)pci_get_bar_address(device, 5);
	struct ahci_hba_generic_host_ctrl* hba = abar;
	if (!__init_controller(device, hba, kernel)) {debug("AHCI controller cannot address 64 bits - will not use\n"); return;}
	//Register the controller
	debug("AHCI controller found!\n");
	struct ahci_controller* controller = malloc(sizeof(struct ahci_controller));
	controller->abar = abar;
	controller->next = 0;
	if (!first_ahci_controller) {first_ahci_controller = controller;}
	else {
		struct ahci_controller* prev_controller = first_ahci_controller;
		while (prev_controller->next) {prev_controller = prev_controller->next;}
		prev_controller->next = controller;
	}
	//Setup ports and check for devices
	u32 ports = hba->ports_implemented;
	controller->ports = ports;
	__setup_ports(ports, hba, controller);
	return;
}

void _ahci_init(u64 kernel) {
	//Find all AHCI controllers, and get all disks connected to them
	struct pci_device* device = pci_get_first_device();
	while (device) {
		if (pci_get_class(device) == 1 && pci_get_subclass(device) == 6) {
			__make_ahci_controller(device, kernel);
		}
		device = device->next;
	}
	return;
}

void _ahci_read(struct disk* disk, u64 lba, u64 count, void* buf) {
	return;
}

void _ahci_write(struct disk* disk, u64 lba, u64 count, void* buf) {
	return;
}
