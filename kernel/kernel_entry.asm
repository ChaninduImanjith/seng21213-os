; =============================================================================
; SENG21213-OS :: Kernel Entry Point
; Supports:
;   1. Existing custom BIOS bootloader
;   2. GRUB2 Multiboot v1
;
; Both paths install the same kernel-owned flat GDT before entering C.
; =============================================================================

[BITS 32]

[EXTERN kernel_main]
[GLOBAL _start]

SECTION .text

align 4
_start:
    ; Existing custom loader enters at the first byte of kernel.bin.
    ; GRUB enters at this ELF entry point.
    jmp short kernel_entry

; ---------------------------------------------------------------------------
; Multiboot v1 header -- must appear in first 8 KiB.
; ---------------------------------------------------------------------------
align 4
multiboot_header:
    dd 0x1BADB002
    dd 0x00000003
    dd 0xE4524FFB

kernel_entry:
    ; Preserve boot protocol values before touching the environment.
    ;
    ; GRUB:
    ;   EAX = 0x2BADB002
    ;   EBX = multiboot_info pointer
    ;
    ; Custom bootloader clears both registers.
    mov esi, eax
    mov edi, ebx

    ; Install our own flat GDT.
    ;
    ; CS override makes the descriptor lookup independent of the
    ; incoming data-segment selector.
    lgdt [cs:gdt_descriptor]

    ; Reload CS using our code descriptor.
    jmp 0x08:gdt_ready

gdt_ready:
    ; Reload all data/stack segments using our flat data descriptor.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Kernel-owned stack.
    mov esp, kernel_stack_top
    mov ebp, esp

    ; cdecl:
    ; kernel_main(uint32_t boot_magic, uint32_t boot_info)
    push edi
    push esi
    call kernel_main

    cli

kernel_halt:
    hlt
    jmp kernel_halt


SECTION .rodata
align 8

; ---------------------------------------------------------------------------
; Kernel-owned flat GDT
; ---------------------------------------------------------------------------
gdt_start:

gdt_null:
    dq 0x0000000000000000

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start


SECTION .bss
align 16

kernel_stack_bottom:
    resb 16384

kernel_stack_top:
