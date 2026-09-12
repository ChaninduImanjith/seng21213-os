# SENG21213-OS — Stage 0: Kernel Foundations

> **Course**: SENG 21213 – Computer Architecture & Operating Systems  
> **Year**: 2nd Year, Software Engineering  
> **Assignment**: Build your own x86 Operating System

---

## What Is This?

This is **Stage 0** of your semester-long OS assignment. Over 5 lecture milestones
(Lectures 8–12), your team will transform this minimal kernel into a functioning
operating system with process management, threading, memory management, and a
file system.

```
seng21213-os/
├── boot/
│   └── boot.asm          ← MBR Bootloader (NASM, 16-bit → 32-bit transition)
├── kernel/
│   ├── kernel_entry.asm  ← Protected-mode entry, calls kernel_main()
│   ├── kernel.c          ← Main kernel: shell loop, command dispatch
│   ├── vga.c / vga.h     ← VGA 80×25 text-mode driver
│   ├── keyboard.c / .h   ← PS/2 keyboard polling driver
├── include/
│   └── types.h           ← Primitive types (no libc!)
├── linker.ld             ← Linker script (kernel at 0x10000)
├── Makefile              ← Build system
├── Dockerfile            ← Reproducible build environment
└── README.md             ← You are here
```

---

## Milestone Schedule

| Lecture | Milestone | Files to Add |
|---------|-----------|-------------|
| L08 | ✅ Stage 0 – Boot + VGA + Shell | *Given to you* |
| L09 | Process Management | `kernel/process.c`, `kernel/scheduler.c` |
| L10 | Threads & Synchronisation | `kernel/thread.c`, `kernel/mutex.c` |
| L11 | Memory Management | `kernel/pmm.c`, `kernel/vmm.c` |
| L12 | File System | `kernel/fs.c`, `kernel/ramdisk.c` |

---

## Quick Start

### Option A: Docker (Recommended for all platforms)

```bash
# 1. Install Docker Desktop (Windows/Mac) or Docker Engine (Linux)
# 2. Build the image once:
docker build -t seng21213-os-builder .

# 3. Build the OS:
docker run --rm -v "$(pwd)":/os seng21213-os-builder

# 4. Run in QEMU (install QEMU locally):
qemu-system-i386 -drive format=raw,file=seng21213-os.img -m 32M
```

### Option B: Native Linux/WSL2

```bash
# Ubuntu/Debian
sudo apt install nasm gcc gcc-multilib binutils qemu-system-x86 make

# Build
make all

# Run
make run
```

### Option C: macOS (Homebrew)

```bash
brew install nasm x86_64-elf-binutils qemu

# You also need an i686-elf-gcc cross-compiler:
# See: https://wiki.osdev.org/GCC_Cross-Compiler
make all
make run
```

---

## Understanding the Boot Process

```
Power On
  │
  ▼
BIOS (firmware in ROM)
  │  Loads 512-byte MBR from disk sector 1 into RAM at 0x7C00
  ▼
boot/boot.asm  (Real Mode, 16-bit)
  │  Prints "Loading SENG21213-OS..."
  │  Reads 64 sectors (kernel) from disk into RAM at 0x10000
  │  Sets up GDT (Global Descriptor Table)
  │  Switches CPU to 32-bit Protected Mode
  │  Far-jumps to 0x10000
  ▼
kernel/kernel_entry.asm  (Protected Mode, 32-bit)
  │  Calls kernel_main()
  ▼
kernel/kernel.c  →  kernel_main()
  │  vga_init()     – set up text display
  │  kb_init()      – set up keyboard
  │  print_splash() – welcome screen
  │  shell_run()    – interactive shell (infinite loop)
  ▼
Your code from here...
```

---

## Building Lecture 9: Process Management

When you reach Lecture 9, you'll add process support. Here's the interface to implement:

```c
/* kernel/process.h  — you write this! */

#define MAX_PROCESSES    16
#define STACK_SIZE     4096

typedef enum { READY, RUNNING, BLOCKED, TERMINATED } proc_state_t;

typedef struct pcb {
    uint32_t      pid;
    proc_state_t  state;
    uint32_t      esp;          /* Saved stack pointer */
    uint32_t      eip;          /* Saved instruction pointer */
    uint32_t      stack[STACK_SIZE / 4];
    struct pcb   *next;         /* For linked-list ready queue */
} pcb_t;

void   process_init(void);
pcb_t *process_create(void (*entry)(void));
void   process_yield(void);        /* Trigger context switch */
void   process_exit(void);
void   scheduler_tick(void);       /* Called by timer IRQ (Lecture 10) */
```

---

## Debugging Tips

```bash
# Debug with GDB
make run-debug
# In another terminal:
gdb
(gdb) target remote :1234
(gdb) set architecture i386
(gdb) symbol-file build/kernel.elf
(gdb) break kernel_main
(gdb) continue

# Inspect the disk image
xxd seng21213-os.img | head -32    # View MBR
xxd seng21213-os.img | grep -c aa55  # Verify boot signature
```

---

## Key Learning Resources

| Topic | Reference |
|-------|-----------|
| x86 Protected Mode | Intel IA-32 Manual, Vol 3, Chapter 3 |
| VGA Text Mode | OSDev Wiki: Text UI |
| Interrupts / IDT | Stallings Ch.1; OSDev: IDT |
| Process Management | Stallings Ch.3–4 (your lecture notes) |
| Memory Management | Stallings Ch.7–8 (your lecture notes) |
| OSDev community | https://wiki.osdev.org |

---

## Assessment Rubric (per milestone)

| Criterion | Weight |
|-----------|--------|
| Code compiles and kernel boots in QEMU | 30% |
| Feature implementation (correct behaviour) | 40% |
| Code quality and comments | 20% |
| Lab demo and viva questions | 10% |

---

*Happy hacking! Remember: every commercial OS started exactly like this.*

---

## Stage 1 Progress — Process Management (implemented)

**What was built:**
- `kernel/process.h` / `kernel/process.c` — `pcb_t` struct (PID, state, saved ESP/EIP,
  4KB private stack, `next` pointer), a static 16-slot process table, and a
  singly-linked ready queue (`enqueue`/`dequeue`).
- `kernel/idt.h` / `kernel/idt.c` — 256-entry IDT, `lidt` load, PIC remap
  (IRQ0-7 → vectors 32-39, IRQ8-15 → 40-47), and PIT channel 0 programmed
  to 100 Hz (10 ms time slice) via `pit_init()`.
- `kernel/isr.asm` — `irq0_handler`: `pushad` saves the interrupted process's
  registers, calls `scheduler_switch()` (C) with the old ESP, then loads the
  ESP it returns, `popad` + `iretd` resumes the next process.
- `kernel/scheduler.c` — `scheduler_switch()` implements round-robin:
  requeue the process that was just interrupted, dequeue the next one,
  send PIC EOI, return its saved ESP.
- `process_create()` pre-builds a **fake interrupt frame** on a new
  process's stack (dummy EDI/ESI/EBP/EBX/EDX/ECX/EAX, then EIP/CS/EFLAGS)
  so the very first context switch into it works through the same
  `popad`/`iretd` path as every subsequent switch.
- `ps` shell command lists PID + state, marking the currently running one.
- Two demo processes (`demo_process_a`, `demo_process_b`) print characters
  to fixed screen positions at different tick-based rates, proving
  concurrent scheduling.

**Bug fixed during development:** the initial PIC remap unmasked *all*
IRQs, including IRQ1 (keyboard), which has no IDT handler yet (keyboard is
polled, not interrupt-driven) — this froze the keyboard. Fixed by masking
everything except IRQ0 (`outb(0x21, 0xFE)`).

**How to test:**

    make clean && make run

The background counters (cyan letters / red digits, rows 22-23) update on
their own while the shell stays fully responsive — type `ps` to see all
three processes and their state.

**Tag:** `v0.2-stage1`

---

## Stage 2 Progress — Threads, Mutex & Semaphore (implemented)

**What was built:**
- `kernel/process.h`/`.c` refactored: `process_alloc()` is now the shared
  low-level allocator (builds the fake interrupt frame), used by both
  `process_create()` and the new `thread_create()`.
- `kernel/thread.h`/`.c` — kernel threads via a **trampoline pattern**:
  a thread's saved EIP points at `thread_trampoline()`, which reads
  `fn`/`arg` off the current PCB and calls the real function — this is
  how an argument gets passed through when `iretd` can't pass one directly.
- `kernel/mutex.h`/`.c` — spinlock mutex using `cli`/`sti` to make the
  "check-then-set" atomic w.r.t. the timer interrupt, `hlt` while waiting.
- `kernel/semaphore.h`/`.c` — counting semaphore, same `cli`/`sti` pattern.
- `race` / `racesafe` shell commands — two threads increment a shared
  `myglobal` 30 times each. To make the race *deterministic* rather than
  a rare timing accident, each increment is split as
  `load -> int $32 (forced context switch) -> store`, so the interleaving
  is guaranteed every run. Without a mutex this reliably loses updates
  (myglobal ends at 30, not 60); with a mutex it's always correct (60).
- `pc` shell command — classic bounded-buffer producer/consumer with
  3 semaphores (`empty`, `full`, `mutex`), 20 items through a 5-slot
  buffer, verified consumed in order with no corruption.
- `threads` shell command — lists only PCBs with `thread_fn` set,
  distinguishing kernel threads from ordinary processes in `ps`.

**Design notes:**
- Forcing the race with `int $32` instead of a busy-wait delay loop was
  a deliberate choice: at `-O2` a plain `myglobal++` can compile to a
  single atomic memory instruction, and even a split load/delay/store can
  miss the 10ms tick window often enough to look "correct" by luck. An
  explicit software interrupt between load and store guarantees the
  interleaving every single run.

**How to test:**

    make clean && make run

Then in the shell: `race` (expect LOST UPDATES), `racesafe` (expect
CORRECT), `pc` (expect PASS), `threads` (lists active kernel threads).

**Tag:** `v0.3-stage2`

---

## Stage 3 Progress — Physical Memory Manager (implemented)

**What was built:**
- `boot/boot.asm` — `detect_memory` runs BIOS INT 0x15 EAX=0xE820 in Real
  Mode (before the switch to Protected Mode, since BIOS calls aren't
  available afterward). Stores the entry count at 0x8000 and the raw
  24-byte SMAP entries starting at 0x8004.
- `linker.ld` — added a `kernel_end` symbol marking the first byte of
  physical memory not occupied by the kernel image, so the PMM never
  hands out a frame the kernel itself is using.
- `kernel/pmm.h`/`.c` — parses the E820 map, builds a bitmap (1 bit per
  4KB frame) over the first 32MB (matching QEMU's `-m 32M`), starting
  pessimistic (everything marked used) and freeing only BIOS-reported
  usable regions above `kernel_end`. `pmm_alloc_frame()` / `pmm_free_frame()`
  do a first-fit bitmap scan/clear.
- `mem` shell command — reports real total/used/free memory from the PMM
  (previously a hardcoded stub).
- `memtest` shell command — allocates 100 frames, verifies all addresses
  are distinct, frees them all, and confirms the free-frame count returns
  to its exact starting value (proof of no leaks / no double-allocation).

**Design notes:** `kernel_end` is a linker symbol, not a C variable — it
has no value of its own, so it's always read as `&kernel_end` (its
address), never `kernel_end` (which would read whatever bytes happen to
sit at that address).

**How to test:**

    make clean && make run

Then in the shell: `mem` (shows total/used/free), `memtest` (expect PASS).

**Tag:** `v0.4-stage3`
