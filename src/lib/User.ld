OUTPUT_FORMAT("elf32-i386")
ENTRY(main1)

SECTIONS
{
    . = 0x00400000;

    .text :
    {
        *(.text)
        *(.text.*)
    }

    . = ALIGN(4096);
    .data :
    {
        *(.data)
        *(.data.*)
    }

    . = ALIGN(4096);
    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    }

    . = ALIGN(4096);
    .bss :
    {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
