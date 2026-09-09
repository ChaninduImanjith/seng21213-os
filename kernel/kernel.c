#include "vga.h"
#include "keyboard.h"
#include "process.h"
#include "scheduler.h"
#include "../include/types.h"

static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_ps(void);

static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static const char *k_ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

static void print_splash(void) {
    vga_clear(VGA_BLACK);
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);
    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);
    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 1: Process Management", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering - Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);
    vga_set_cursor(4, 2);
    vga_puts_color("  Round-robin scheduler is live.  Type 'help' to begin.",
                   VGA_LIGHT_GREEN, VGA_BLACK);
    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);
    vga_set_cursor(9, 0);
}

static void cmd_help(void) {
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");
    vga_puts("  help    - Show this help message\n");
    vga_puts("  clear   - Clear the screen\n");
    vga_puts("  about   - About this OS and course\n");
    vga_puts("  echo    - Echo text to screen\n");
    vga_puts("  mem     - Memory map (stub)\n");
    vga_puts("  ps      - List running processes\n");
    vga_puts_color("\n  Milestones (to implement):\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  kill    - [L09] Terminate a process\n");
    vga_puts("  threads - [L10] List kernel threads\n");
    vga_puts("  free    - [L11] Show free memory\n");
    vga_puts("  ls      - [L12] List files\n");
    vga_puts("  cat     - [L12] Print file contents\n\n");
}

static void cmd_clear(void) { vga_clear(VGA_BLACK); }

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  Scheduler    : Round-robin, 100Hz PIT tick\n");
    vga_puts("  Course       : SENG 21213 - Sem 2\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_mem(void) {
    vga_puts_color("\n  Memory Map (stub - implement PMM in Lecture 11)\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");
    vga_puts("  0x00000000 - 0x000FFFFF  :  First 1 MB (reserved/BIOS)\n");
    vga_puts("  0x00100000 - 0x00EFFFFF  :  Extended memory (usable ~14 MB)\n\n");
}

static const char *state_name(proc_state_t s) {
    switch (s) {
        case READY:      return "READY";
        case RUNNING:    return "RUNNING";
        case BLOCKED:    return "BLOCKED";
        default:         return "TERMINATED";
    }
}

static void cmd_ps(void) {
    extern pcb_t *current_process;
    int i;
    vga_puts_color("\n  PID   STATE\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  -----------------------------\n");
    for (i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *p = process_get(i);
        if (!p) continue;
        vga_printf("  %u     %s", p->pid, state_name(p->state));
        if (p == current_process) {
            vga_puts_color("  <-- running now", VGA_LIGHT_GREEN, VGA_BLACK);
        }
        vga_puts("\n");
    }
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Demo processes (Stage 1 deliverable): two concurrent processes, each
 * printing a character at a different rate, proving the scheduler works.
 * --------------------------------------------------------------------------*/
static void demo_process_a(void) {
    uint32_t last = 0;
    uint32_t n = 0;
    for (;;) {
        uint32_t now = scheduler_ticks();
        if (now - last >= 20) {   /* every ~200ms */
            last = now;
            vga_putchar_at(22, 10 + (int)(n % 20), (char)('A' + (n % 26)),
                            VGA_LIGHT_CYAN, VGA_BLACK);
            n++;
        }
    }
}

static void demo_process_b(void) {
    uint32_t last = 0;
    uint32_t n = 0;
    for (;;) {
        uint32_t now = scheduler_ticks();
        if (now - last >= 50) {   /* every ~500ms - a different rate */
            last = now;
            vga_putchar_at(23, 10 + (int)(n % 20), (char)('0' + (n % 10)),
                            VGA_LIGHT_RED, VGA_BLACK);
            n++;
        }
    }
}

/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char  shell_buf[256];
static char  prompt[] = "\n  ksh> ";

static void shell_run(void) {
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        const char *cmd = k_ltrim(shell_buf);
        if (k_strlen(cmd) == 0) continue;

        if (k_strcmp(cmd, "help")  == 0) { cmd_help();  continue; }
        if (k_strcmp(cmd, "clear") == 0) { cmd_clear(); continue; }
        if (k_strcmp(cmd, "about") == 0) { cmd_about(); continue; }
        if (k_strcmp(cmd, "mem")   == 0) { cmd_mem();   continue; }
        if (k_strcmp(cmd, "ps")    == 0) { cmd_ps();    continue; }

        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

        if (k_strcmp(cmd, "kill")    == 0 ||
            k_strcmp(cmd, "threads") == 0 ||
            k_strcmp(cmd, "free")    == 0 ||
            k_strcmp(cmd, "ls")      == 0 ||
            k_strcmp(cmd, "cat")     == 0) {
            vga_puts_color("  [TODO] This command is not yet implemented.\n",
                           VGA_YELLOW, VGA_BLACK);
            continue;
        }

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}

/* ---------------------------------------------------------------------------
 * Kernel entry point - called from kernel_entry.asm
 * --------------------------------------------------------------------------*/
void kernel_main(void) {
    vga_init();
    kb_init();
    print_splash();

    process_init();
    process_create(shell_run);
    process_create(demo_process_a);
    process_create(demo_process_b);

    scheduler_init();   /* enables interrupts - the scheduler takes over from here */

    for (;;) { __asm__ __volatile__("hlt"); }
}
