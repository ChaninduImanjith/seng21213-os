OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

PHDRS
{
    text   PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data   PT_LOAD FLAGS(6);
}

SECTIONS
{
    /* Conventional Multiboot kernel load address. */
    . = 0x100000;

    .text :
    {
        *(.text)
        *(.text.*)
    } :text

    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    } :rodata

    .data :
    {
        *(.data)
        *(.data.*)
    } :data

    .bss :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    } :data

    kernel_end = .;

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.eh_frame.*)
        *(.comment)
        *(.note.GNU-stack)
    }
}
