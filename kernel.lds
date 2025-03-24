/*
 * Set the architecture to mips.
 */
OUTPUT_ARCH(mips)

/*
 * Set the ENTRY point of the program to _start.
 */
ENTRY(_start)

SECTIONS {
	/* Exercise 3.10: Your code here. */

	/* fill in the correct address of the key sections: text, data, bss. */
	/* Hint: The loading address can be found in the memory layout. And the data section
	 *       and bss section are right after the text section, so you can just define
	 *       them after the text section.
	 */
	. = 0x80020000;
	.text : { *(.text) }
	.data : { *(.data) }
	bss_start = .;
	.bss : { *(.bss) }
	bss_end = .;
	. = 0x80400000;
	end = . ;
}
