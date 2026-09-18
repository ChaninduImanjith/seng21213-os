; =============================================================================
; SENG21213-OS :: Stage 0 Bootloader
; File   : boot/boot.asm
; Author : SENG 21213 – Computer Architecture and Operating Systems
; Purpose: MBR (Master Boot Record) bootloader. Switches CPU from 16-bit Real
;          Mode to 32-bit Protected Mode, then loads and jumps to the kernel.
; =============================================================================

[BITS 16]           ; CPU starts in 16-bit Real Mode
[ORG 0x7C00]        ; BIOS loads the MBR at this fixed address

; ---------------------------------------------------------------------------
; Entry: Real Mode setup
; ---------------------------------------------------------------------------
start:
    cli                ; Disable interrupts during setup
    xor  ax, ax
    mov  ds, ax        ; Data Segment = 0
    mov  es, ax        ; Extra Segment = 0
    mov  ss, ax        ; Stack Segment = 0
    mov  sp, 0x7C00    ; Stack pointer just below our code
    sti                ; Re-enable interrupts

    ; Save drive number (BIOS stores it in dl)
    mov  [boot_drive], dl

    ; Print loading banner using BIOS int 0x10
    mov  si, msg_banner
    call print_rm
    mov  si, msg_load
    call print_rm

; ---------------------------------------------------------------------------
; Load kernel from disk into memory at 0x1000:0x0000.
; 96 sectors × 512 bytes = 48 KiB kernel load window.
; The larger window leaves room for later kernel extensions.
; ---------------------------------------------------------------------------
load_kernel:
    mov  bx, 0x1000        ; ES:BX = 0x10000 (kernel load address)
    mov  es, bx
    xor  bx, bx

    mov  ah, 0x02          ; BIOS read sectors
    mov  al, 96            ; Number of sectors to read (48 KiB load window)
    mov  ch, 0             ; Cylinder 0
    mov  cl, 2             ; Start from sector 2 (sector 1 is MBR)
    mov  dh, 0             ; Head 0
    mov  dl, [boot_drive]  ; Drive number
    int  0x13
    jc   disk_error        ; Carry flag set = error

    mov  si, msg_ok
    call print_rm

; ---------------------------------------------------------------------------
; Stage 3: detect physical memory via BIOS INT 0x15, EAX=0xE820, while we
; are still in Real Mode -- this is the only place we can call the BIOS.
; Results are stored at 0x8000 (count) / 0x8004 (entries) for pmm_init().
; ---------------------------------------------------------------------------
    call detect_memory

; ---------------------------------------------------------------------------
; Enter Protected Mode
; ---------------------------------------------------------------------------
enter_pm:
    cli
    lgdt [gdt_descriptor]  ; Load the Global Descriptor Table

    mov  eax, cr0
    or   eax, 0x1          ; Set PE (Protection Enable) bit
    mov  cr0, eax

    ; Far jump to flush the prefetch queue and load CS with code segment
    jmp  CODE_SEG:init_pm32

; ---------------------------------------------------------------------------
; 32-bit Protected Mode initialisation
; ---------------------------------------------------------------------------
[BITS 32]
init_pm32:
    ; Set all data segment registers to the data descriptor
    mov  ax, DATA_SEG
    mov  ds, ax
    mov  ss, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    ; Set up a proper kernel stack at 0x90000
    mov  ebp, 0x90000
    mov  esp, ebp

    ; Jump to the kernel entry point (loaded at 0x10000).
    ; EAX/EBX = 0 tells kernel_entry.asm this is the custom boot path,
    ; not a GRUB Multiboot invocation.
    xor  eax, eax
    xor  ebx, ebx
    call 0x10000

    ; Should never return, but halt if it does
    hlt

; ---------------------------------------------------------------------------
; Error handlers
; ---------------------------------------------------------------------------
[BITS 16]
disk_error:
    mov  si, msg_err
    call print_rm
    mov  si, msg_halt
    call print_rm
    jmp  $              ; Infinite loop

; ---------------------------------------------------------------------------
; Subroutine: print_rm – print NUL-terminated string in SI (Real Mode)
; ---------------------------------------------------------------------------
print_rm:
    lodsb               ; Load byte at [SI] into AL, advance SI
    or   al, al
    jz   .done
    mov  ah, 0x0E       ; BIOS teletype output
    xor  bh, bh
    int  0x10
    jmp  print_rm
.done:
    ret

; ---------------------------------------------------------------------------
; Subroutine: detect_memory -- BIOS INT 0x15, EAX=0xE820 memory map.
; Must run in Real Mode (before enter_pm). Walks the BIOS-provided memory
; map one 24-byte SMAP entry at a time; EBX carries the "continuation"
; value the BIOS expects back on the next call, and 0 in EBX after a call
; means "that was the last entry". Stores the entry count as a word at
; 0x8000, and the raw entries back-to-back starting at 0x8004, so
; pmm_init() (Stage 3 C code, after Protected Mode) can parse them.
; ---------------------------------------------------------------------------
detect_memory:
    push es
    xor  ax, ax
    mov  es, ax
    mov  di, 0x8004      ; buffer for entries (ES:DI)
    xor  ebx, ebx        ; 0 = start from the beginning
    xor  bp, bp          ; bp = entry count so far
.e820_loop:
    mov  eax, 0xE820
    mov  ecx, 24          ; ask for a full 24-byte entry
    mov  edx, 0x534D4150  ; 'SMAP' magic, required every call
    int  0x15
    jc   .e820_done       ; carry set = unsupported or finished
    cmp  eax, 0x534D4150  ; BIOS should echo the magic back in EAX
    jne  .e820_done
    inc  bp
    add  di, 24            ; advance to next entry slot
    test ebx, ebx          ; EBX == 0 means that was the last entry
    jnz  .e820_loop
.e820_done:
    mov  [0x8000], bp     ; store final entry count
    pop  es
    ret

; ---------------------------------------------------------------------------
; Data
; ---------------------------------------------------------------------------
boot_drive  db 0

msg_banner  db 13, 10, '  ================================', 13, 10
            db '  SENG21213-OS  |  Stage 0        ', 13, 10
            db '  Computer Architecture & OS       ', 13, 10
            db '  ================================', 13, 10, 0
msg_load    db '  [BOOT] Loading kernel...', 13, 10, 0
msg_ok      db '  [BOOT] Kernel loaded OK ', 13, 10, 0
msg_err     db '  [BOOT] DISK ERROR!       ', 13, 10, 0
msg_halt    db '  System halted.           ', 13, 10, 0

; ---------------------------------------------------------------------------
; GDT – Global Descriptor Table
; Two flat (0–4 GB) segments: Code and Data, Ring 0
; ---------------------------------------------------------------------------
gdt_start:
gdt_null:                   ; Mandatory null descriptor
    dd 0x00000000
    dd 0x00000000

gdt_code:                   ; Executable, readable, Ring 0
    dw 0xFFFF               ; Limit [15:0]
    dw 0x0000               ; Base  [15:0]
    db 0x00                 ; Base  [23:16]
    db 10011010b            ; Access byte: Present|Ring0|Type=1(code)|Exec|Read
    db 11001111b            ; Flags + Limit [19:16]: 4K granularity, 32-bit
    db 0x00                 ; Base  [31:24]

gdt_data:                   ; Readable, writable, Ring 0
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b            ; Access: Present|Ring0|Type=0(data)|Read|Write
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; GDT limit (size - 1)
    dd gdt_start                  ; GDT base address

; Segment selectors (byte offset into GDT)
CODE_SEG equ gdt_code - gdt_start   ; = 0x08
DATA_SEG equ gdt_data - gdt_start   ; = 0x10

; ---------------------------------------------------------------------------
; Boot signature – BIOS checks for 0xAA55 at bytes 510-511
; ---------------------------------------------------------------------------
times 510 - ($ - $$) db 0
dw 0xAA55
