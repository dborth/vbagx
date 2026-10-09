/*
 * Wii only. One contiguous MEM1 block, reserved by the linker, that serves as:
 *   - the load image of the embedded assets (DOL data, so the Homebrew Channel will load it), and
 *   - after boot, the core overlay: JIT arena (GBA) / heap (menu, GB).
 *
 * Placement: after .bss, NOT between .sdata and .sbss. Those two must stay adjacent because
 * small-data is addressed r13-relative with a +-32KB window (a gap there gives "relocation truncated
 * to fit: R_PPC_SDAREL16"). Everything loaded still ends far below HBC's 0x8132FBFF limit because the
 * only loaded part of the block is the assets; the 16MB tail is not in the DOL.
 *
 *        R = __jit_region_start                                   R + __jit_region_size
 *        |<---- assets load image ---->|<---------- NOLOAD (bss-like) tail ---------->|
 *        |   DOL DATA segment (MEM1)   |  not in the DOL                              |
 *
 * Nothing in MEM1 is wasted: once AssetsRelocate() (memmanager.cpp) has copied the assets to their
 * run address in MEM2, the whole of R..R+size is one 16MB (JIT_ARENA_MB) block.
 *
 * Why not simply load the assets into MEM2: the Homebrew Channel only loads DOL sections inside
 * MEM1, 0x80003400..0x8132FBFF (channelapp/config.h LD_MIN/MAX_ADDR + loader_reloc.c). So the
 * assets are *linked* (VMA) in MEM2 but *loaded* (LMA) in MEM1; elf2dol uses the LMA (p_paddr).
 *
 * The tail is deliberately not zeroed by crt0 (outside __bss_start..__bss_end): nothing may rely
 * on zero-init of the region. The mspace/JIT code initialises what it uses.
 *
 * Makefile.wii prefixes the sections of every bin2o object with .mem2 (.rodata -> .mem2.rodata) so
 * the stock .rodata rule does not claim them first.
 *
 * NOTE: ld requires this INSERT script before the main script (-T order).
 *
 * MEM2 side: toolchain rvl.ld declares apps may use [0x90002000, 0x933e0000) with the IOS IPC buffer
 * at [0x933e0000, 0x93400000). memmanager.cpp sets __myArena2Hi to __assets_mem2_start so libogc2's
 * MEM2 arena ends below the asset image.
 *
 * __jit_region_size comes from Makefile.wii (--defsym, from JIT_ARENA_MB); memmanager.cpp aborts at
 * startup if it disagrees with JIT_ARENA_SIZE.
 */
PROVIDE(__jit_region_size = 16M);

MEM2_ASSETS_BASE = 0x93180000;
MEM2_ASSETS_TOP  = 0x933e0000;	/* rvl.ld: __Arena2Hi, start of the IPC buffers */
HBC_LD_MAX_ADDR  = 0x8132fbff;	/* Homebrew Channel: highest loadable DOL byte */

PHDRS
{
	assets PT_LOAD FLAGS(4);
}

SECTIONS
{
	.assets MEM2_ASSETS_BASE : AT(ALIGN(ADDR(.bss) + SIZEOF(.bss), 32))
	{
		__assets_mem2_start = .;
		*(.mem2.*)
		. = ALIGN(32);
		__assets_mem2_end = .;
	} :assets

	__assets_lma_start = LOADADDR(.assets);
	__assets_lma_end   = LOADADDR(.assets) + SIZEOF(.assets);
	__jit_region_start = __assets_lma_start;

	/* The rest of the block is a pure address gap: MEM1 that no section occupies, so it is not in the
	   DOL, not zeroed by crt0 and not touched by anything until memmanager.cpp takes it.
	   rvl.ld computes __Arena1Lo from the location counter after .bss, so libogc2's MEM1 heap (and
	   the argv copy) start right above the block with no further override. */
	. = __jit_region_start + __jit_region_size;
	__jit_region_end = .;
}
INSERT AFTER .bss;

ASSERT(__assets_mem2_end <= MEM2_ASSETS_TOP, "MEM2 asset image overruns the IPC buffer region: lower MEM2_ASSETS_BASE in wii_mem.ld")
ASSERT(__assets_lma_end  <= HBC_LD_MAX_ADDR, "DOL load image extends past what the Homebrew Channel will load")
ASSERT(SIZEOF(.assets)   <= __jit_region_size, "embedded assets are larger than the MEM1 core region (raise JIT_ARENA_MB)")
