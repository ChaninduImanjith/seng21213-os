[bits 32]

global irq0_handler
extern scheduler_switch

; CPU has already pushed EFLAGS, CS, EIP for us (same-privilege interrupt).
irq0_handler:
    pushad                  ; save EAX..EDI of the process that was running
    push esp                ; pass old esp as the single argument (cdecl)
    call scheduler_switch   ; C: save old_esp, pick next process, returns new esp in EAX
    add esp, 4               ; pop the argument (caller cleans up, cdecl)
    mov esp, eax             ; switch onto the next process's stack
    popad                   ; restore EAX..EDI for the NEXT process
    iretd                   ; pops EIP, CS, EFLAGS -> resumes that process
