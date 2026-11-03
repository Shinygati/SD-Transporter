/* Position-independent blob: linked at 0, relocated by copying. */
SECTIONS {
	. = 0;
	.text : {
		*(.text.entry)
		*(.text*)
		*(.rodata*)
	}
	slot2emu_end = .;
	/* No writable data may exist: the blob keeps all state in Slot2EmuControl. */
	/DISCARD/ : { *(.comment*) *(.ARM.attributes*) *(.ARM.exidx*) }
}
