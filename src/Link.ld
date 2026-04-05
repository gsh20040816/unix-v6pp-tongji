/*Link.ld*/
OUTPUT_FORMAT("elf32-i386")

ENTRY(greatstart)

SECTIONS
{
	.text 0xc0100000:
	{
		code = .; _code = .; __code = .;
		*(.text)
		*(.text.*)
		. = ALIGN(4096);
	}

	.rodata :
	{
		*(.rodata)
		*(.rodata.*)
		. = ALIGN(4096);
	}
	
	.data :
	{
		. = ALIGN(4);
		__init_array_start = .;
		KEEP(*(.init_array))
		KEEP(*(.init_array.*))
		__init_array_end = .;

		. = ALIGN(4);
		__fini_array_start = .;
		KEEP(*(.fini_array))
		KEEP(*(.fini_array.*))
		__fini_array_end = .;

		__CTOR_LIST__ = .;
		KEEP(*(.ctors))
		LONG(0)
		__CTOR_END__ = .;
		
		__DTOR_LIST__ = .;
		KEEP(*(.dtors))
		LONG(0)
		__DTOR_END__ = .;
		
		data = .; _data = .; __data = .;
		*(.data)
		*(.data.*)
		. = ALIGN(4096);
	}
	
	.bss :
	{
		bss = .; _bss = .; __bss = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		. = ALIGN(4096);
	}

	/DISCARD/ :
	{
		*(.note.gnu.build-id)
		*(.note.*)
		*(.comment)
		*(.eh_frame)
		*(.eh_frame.*)
		*(.eh_frame_hdr)
	}
	
	end = .; _end =.; __end = .;
}
