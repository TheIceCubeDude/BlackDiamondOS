struct pci_device {
	u8 bus;
	u8 slot;
	u8 function;
	struct pci_device* next;
};

struct pci_device* first_enumerated_device;
struct pci_device* last_enumerated_device;

void _pci_write_address(u8 bus, u8 slot, u8 function, u8 reg) {
	u8 offset = reg * 4; //Addresses must be 4 byte aligned
	slot &= 0b00011111; //Slot is 5 bits
	function &= 0b00000111; //Fuction is 3 bits
	outl(0xCF8, (1 << 31) | (bus << 16) | (slot << 11) | (function << 8) | offset); //Write to address register
	return;
}

u32 _pci_read(struct pci_device* device, u8 reg) {
	_pci_write_address(device->bus, device->slot, device->function, reg);
	return inl(0xCFC); //Read from data register
}

void _pci_write(struct pci_device* device, u8 reg, u32 data) {
	_pci_write_address(device->bus, device->slot, device->function, reg);
	outl(0xCFC, data); //Write to data register
	return;
}

u16 pci_get_deviceid(struct pci_device* device) {return _pci_read(device, 0) >> 16;}
u16 pci_get_vendor(struct pci_device* device) {return _pci_read(device, 0);}
u16 pci_get_status(struct pci_device* device) {return _pci_read(device, 1) >> 16;}
void pci_set_command(struct pci_device* device, u16 command) {_pci_write(device, 1, command); return;}
u16 pci_get_command(struct pci_device* device) {return _pci_read(device, 1);}
u8 pci_get_class(struct pci_device* device) {return _pci_read(device, 2) >> 24;}
u8 pci_get_subclass(struct pci_device* device) {return _pci_read(device, 2) >> 16;}
u8 pci_get_progif(struct pci_device* device) {return _pci_read(device, 2) >> 8;}
u8 pci_get_revision(struct pci_device* device) {return _pci_read(device, 2);}
u8 pci_get_header_type(struct pci_device* device) {return _pci_read(device, 3) >> 16;}
u8 pci_get_interrupt_pin(struct pci_device* device) {return _pci_read(device, 15) >> 8;}
u8 pci_get_interrupt_line(struct pci_device* device) {return _pci_read(device, 15);}

u32 pci_get_bar(struct pci_device* device, u8 bar) {return _pci_read(device, 4 + bar);}
void pci_set_bar(struct pci_device* device, u8 bar, u32 bar_data) {_pci_write(device, 4 + bar, bar_data); return;}
u32 pci_get_bar_address(struct pci_device* device, u8 bar) {
	u32 raw_bar = pci_get_bar(device, bar);
	if (raw_bar & 1) {return raw_bar & 0xFFFFFFFC;} //IO space bars are 4 bytes aligned
	return raw_bar & 0xFFFFFFF0; //Memory space bars are 16 bytes aligned
}

u8 _get_secondary_bus_number(struct pci_device* device) {return _pci_read(device, 6) >> 8;}

void _enumerate_bus(u8 bus);

void _enumerate_device(struct pci_device device) {
	debug("Found PCI device on bus:");
	debug_dec(device.bus);
	debug(" slot:");
	debug_dec(device.slot);
	debug(" function:");
	debug_dec(device.function);
	debug("\n");
	if ((pci_get_header_type(&device) & 0x7F) == 1) {_enumerate_bus(_get_secondary_bus_number(&device));} //If this is a PCI-PCI bridge, enumerate the secondary bus connected to it
	else {
		//Otherwise add this device to our linked list of devices
		struct pci_device* linked_device = malloc(sizeof(struct pci_device));
		*linked_device = device;
		if (!first_enumerated_device) {first_enumerated_device = linked_device;}
		else {last_enumerated_device->next = linked_device;}
		last_enumerated_device = linked_device;
	}
	return;
}

void _enumerate_bus(u8 bus) {
	for (u8 slot = 0; slot < 32; slot++) {
		struct pci_device device = {bus, slot, 0, 0};
		if (pci_get_vendor(&device) != 0xFFFF) { //If device exists
			if (pci_get_header_type(&device) & 0b10000000) {
				//The slot has multiple functions
				for (; device.function < 8; device.function++) {
					if (pci_get_vendor(&device) != 0xFFFF) { //If function exists
						_enumerate_device(device);
					}
				}
			} else {
				//The slot has one function
				_enumerate_device(device);
			}
		}
		
	}
	return;
}

struct pci_device* pci_get_first_device() {
	return first_enumerated_device;
}

void init_pci() {
	//For our purposes, we treat each function as a "device", and so have to rename devices to "slots"
	//Enumerates the PCI bus
	//Note that BIOS maps MTRRs as uncached for MMIO, so we don't worry about that
	_enumerate_bus(0);
	return;
}
