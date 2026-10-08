/*
 * Wii only. Augments the toolchain's default ogc.ld (INSERT) with a MEM2 output section so large
 * read-only assets stay embedded in the DOL but do not occupy MEM1.
 *
 * Makefile.wii prefixes the sections of every bin2o object (png, ogg, lang, ttf, pcm) with .mem2
 * (.rodata -> .mem2.rodata), so the default script's .rodata rule does not claim them.
 *
 * NOTE: ld requires this INSERT script before the main script (-T order).
 *
 * The DOL loader (HBC) must accept data sections at 0x9xxxxxxx. memmanager.cpp sets __myArena2Hi
 * to __mem2_start so libogc2's MEM2 arena stays below this image.
 *
 * MEM2_BASE must sit in free MEM2: above anything the loader itself uses, below the loader's
 * arena hi.
 */
MEM2_BASE = 0x93300000;

PHDRS
{
	mem2 PT_LOAD FLAGS(4);
}

SECTIONS
{
	.mem2data MEM2_BASE : ALIGN(32)
	{
		__mem2_start = .;
		*(.mem2.rodata .mem2.rodata.*)
		. = ALIGN(32);
		__mem2_end = .;
	} :mem2

	. = ADDR(.bss) + SIZEOF(.bss);
}
INSERT AFTER .bss;

ASSERT(__mem2_end <= 0x935E0000, "MEM2 asset image overflows the top of MEM2")
