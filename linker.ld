/* =============================================================================
 * SENG21213-OS :: Linker Script
 * File   : linker.ld
 *
 * Places the kernel binary starting at physical address 0x10000.
 * The bootloader loads the kernel to 0x1000:0x0000 = physical 0x10000.
 * kernel_entry.asm is the very first object, so _start is at 0x10000.
 * ============================================================================*/

OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

/* Give each class of section appropriate ELF permissions.
 *
 * text   : read + execute
 * rodata : read only
 * data   : read + write
 *
 * This removes the linker warning about an RWX LOAD segment.
 */
PHDRS
{
    text   PT_LOAD FLAGS(5);   /* PF_R | PF_X */
    rodata PT_LOAD FLAGS(4);   /* PF_R        */
    data   PT_LOAD FLAGS(6);   /* PF_R | PF_W */
}

SECTIONS
{
    . = 0x10000;          /* Must match bootloader load/jump address */

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

    /* Stage 3: first byte of physical memory NOT occupied by kernel. */
    kernel_end = .;

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.eh_frame.*)
        *(.comment)
        *(.note.GNU-stack)
    }
}
