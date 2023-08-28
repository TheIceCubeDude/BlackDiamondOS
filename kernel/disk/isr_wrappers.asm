global __ahci_isr_wrapper
extern __ahci_isr

__ahci_isr_wrapper:
save_regs
call near __ahci_isr
restore_regs
iretq
