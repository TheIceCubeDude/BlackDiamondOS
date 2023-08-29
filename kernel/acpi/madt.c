struct acpi_sdt_header* madt;
void* lapic;

void __process_entry(u8* entry) {
	switch (*entry) {
		case 5: lapic = *(void**)(entry + 4); debug("LAPIC override detected\n"); break; //LAPIC 64 bit address override
	}
	return;
}

void __enumerate_madt(struct acpi_sdt_header* header) {
	madt = header;
	u8* data = (u8*)(header + 1);
	lapic = (void*)*(u32*)(data); //LAPIC 32 bit address
	//Search through each entry and perform appropriate action
	u32 i = 8;
	u32 length = header->length - sizeof(struct acpi_sdt_header) - 8;
	while (i < length) {
		u8* entry = data + i;
		__process_entry(entry);
		i += *(entry + 1);
	}
	return;
}

void* acpi_get_lapic() {return lapic;}
