/* Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Everything (code, read-only data, initialised data, BSS, heap) lives
 * in main_ram. The integrated SRAM region defined by LiteX is too small
 * to host a mquickjs heap (typically 8 KiB) so it is intentionally
 * unused here. */

INCLUDE generated/output_format.ld
ENTRY(_start)

__DYNAMIC = 0;

INCLUDE generated/regions.ld

SECTIONS
{
	.text :
	{
		_ftext = .;
		*crt0*(.text)
		KEEP(*crt0*(.text))
		KEEP(*(.text.isr))
		*(.text .stub .text.* .gnu.linkonce.t.*)
		_etext = .;
	} > main_ram

	.rodata :
	{
		. = ALIGN(8);
		_frodata = .;
		*(.rodata .rodata.* .gnu.linkonce.r.*)
		*(.rodata1)
		*(.got .got.*)
		*(.toc .toc.*)
		. = ALIGN(8);
		_erodata = .;
	} > main_ram

	/* Align the whole section so LOADADDR(.data) matches _fdata.
	 * Do *not* ALIGN inside the section — that would offset `_fdata`
	 * past the section start, and crt0 would copy `SIZEOF(.data)` bytes
	 * from `LOADADDR` to `_fdata`, silently clobbering the first words
	 * of .data/.sdata (e.g. picolibc's stdout FILE object). */
	.data : ALIGN(16)
	{
		_fdata = .;
		*(.data .data.* .gnu.linkonce.d.*)
		*(.data1)
		_gp = ALIGN(16);
		*(.sdata .sdata.* .gnu.linkonce.s.*)
		. = ALIGN(8);
		_edata = .;
	} > main_ram

	.bss : ALIGN(8)
	{
		_fbss = .;
		*(.dynsbss)
		*(.sbss .sbss.* .gnu.linkonce.sb.*)
		*(.scommon)
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(8);
		_ebss = .;
		_end = .;
	} > main_ram
}

/* Since .data is already in main_ram, the load-address / virtual-address
 * copy loop in crt0.S becomes a no-op (src == dst). Keep the symbols
 * defined so crt0 links. */
PROVIDE(_fdata_rom = LOADADDR(.data));
PROVIDE(_edata_rom = LOADADDR(.data) + SIZEOF(.data));

/* Stack grows down from the top of main_ram. mquickjs uses very little
 * C stack (the VM runs on its own stack inside the heap), so a modest
 * reserve is enough — leave the last 16 KiB for us. */
PROVIDE(_fstack = ORIGIN(main_ram) + LENGTH(main_ram) - 4);

PROVIDE(_emain_ram = ORIGIN(main_ram) + LENGTH(main_ram));
