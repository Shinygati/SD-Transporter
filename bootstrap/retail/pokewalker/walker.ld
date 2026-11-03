/* Position-independent blob: linked at 0, relocated by copying. */
SECTIONS {
	. = 0;
	.text : {
		*(.text.entry)
		*(.text*)
		*(.rodata*)
	}
	walker_end = .;
	/* No writable data may exist: the blob keeps all state in WalkerControl. */
	/DISCARD/ : { *(.comment*) *(.ARM.attributes*) *(.ARM.exidx*) }
}
