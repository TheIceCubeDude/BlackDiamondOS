struct acpi_rsdp {
	u8 signature[8];
	u8 checksum;
	u8 oem_id[6];
	u8 revision;
	u32 rsdt;
	u32 length;
	void* xsdt;
	u8 extended_checksum;
	u8 reserved[3];
}__attribute__((packed));

struct acpi_sdt_header {
	u8 signature[4];
	u32 length;
	u8 revision;
	u8 checksum;
	u8 oem_id[6];
	u8 oem_table_id[8];
	u32 oem_revision;
	u8 creator_id[4];
	u32 creator_revision;
}__attribute__((packed));

#include "madt.c"

u8 _calc_checksum(void* data, u8 len) {
	u64 sum = 0;
	for (u8 i = 0; i < len; i++) {sum += *(((u8*)data) + i);}
	if (sum % 0x100) {return 0;} //Lowest byte should be 0
	return 1;
}

void _check_sig(struct acpi_sdt_header* table) {
	void* sig = table->signature;
	if (strcmp(sig, "APIC", 4)) {__enumerate_madt(table);}
	else {} //...
	return;
}

void _enumerate_tables(struct acpi_sdt_header* rsdt, u8 adr_64bit) {
	void* entries = rsdt + 1;
	//Get number of entries in the RSDT/XSDT
	u16 entries_count = rsdt->length - sizeof(struct acpi_sdt_header);
	if (adr_64bit) {entries_count /= 8;}
	else {entries_count /= 4;}
	//Loop through the entries, and check the sig of each table
	for (u16 i = 0; i < entries_count; i++) {
		struct acpi_sdt_header* table;
		if (adr_64bit) {table = (struct acpi_sdt_header*)*(((u64*)entries) + i);}
		else {table = (struct acpi_sdt_header*)(u64)*(((u32*)entries) + i);}
		if (_calc_checksum(table, table->length)) {_check_sig(table);}
	}
	return;
}

u8 _check_rsdp(void* ptr) {
	//Check signature
	if (!strcmp(ptr, "RSD PTR ", 8)) {return 0;}
	//Check checksum
	struct acpi_rsdp* rsdp = ptr;
	if (!_calc_checksum(ptr, 20)) {return 0;}
	if (rsdp->revision) { //Version 1.0 ptr (rev 0) only contains first 20 bytes
		if (!_calc_checksum(ptr, sizeof(struct acpi_rsdp))) {return 0;}
	}
	//Get RSDT (or XSDT)
	struct acpi_sdt_header* rsdt;
	u8 adr_64bit;
	if (!rsdp->revision) {rsdt = (void*)rsdp->rsdt; adr_64bit = 0; debug("Found ACPI RSDT\n");}
	else {rsdt = rsdp->xsdt; adr_64bit = 1; debug("Found ACPI XSDT\n");}
	//Enumerate tables
	_enumerate_tables(rsdt, adr_64bit);
	return 1;
}

void init_acpi() {
	//Find the Root System Decription Pointer
	//Firstly get EBDA address and search the first 1KB of it
	u16 ebda_real_seg = *(u16*)0x40E;
	u8* ebda_ptr = (u8*)(ebda_real_seg * 0x10);
	for (u16 i = 0; i < 1024; i += 16) {
		if (_check_rsdp(ebda_ptr + i)) {return;}
	}
	//If that fails, search the BIOS ROM between E0000-FFFFF
	for (u64 ptr = 0xE0000; ptr < 0xFFFFF; ptr +=16) {
		if (_check_rsdp((void*)ptr)) {return;}
	}
	kpanic("Could not find ACPI RSDP");
}
