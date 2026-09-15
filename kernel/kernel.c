#include "vga.h"
#include "keyboard.h"
#include "process.h"
#include "scheduler.h"
#include "thread.h"
#include "mutex.h"
#include "semaphore.h"
#include "pmm.h"
#include "ramdisk.h"
#include "fs.h"
#include "../include/types.h"

static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_memtest(void);
static void cmd_ls(void);
static void cmd_ansi(void);
static void cmd_touch(const char *name);
static void cmd_cat(const char *name);
static void cmd_write(const char *args);
static void cmd_rm(const char *name);
static void cmd_ps(void);
static void cmd_threads(void);
static void cmd_race(bool use_mutex);
static void cmd_pc(void);

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
    vga_puts_color("  Stage 2: Threads & Synchronization", VGA_LIGHT_CYAN, VGA_BLACK);
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
    vga_puts("  help     - Show this help message\n");
    vga_puts("  clear    - Clear the screen\n");
    vga_puts("  about    - About this OS and course\n");
    vga_puts("  echo     - Echo text to screen\n");
    vga_puts("  mem      - Show physical memory usage\n");
    vga_puts("  memtest  - Stress-test the PMM (alloc/free 100 frames)\n");
    vga_puts("  ps       - List running processes\n");
    vga_puts("  threads  - List kernel threads\n");
    vga_puts("  race     - Run the myglobal race WITHOUT a mutex\n");
    vga_puts("  racesafe - Run the myglobal race WITH a mutex\n");
    vga_puts("  pc       - Run the producer-consumer demo (3 semaphores)\n");
    vga_puts("  ls       - List files on the RAM disk\n");
    vga_puts("  touch    - Create an empty file\n");
    vga_puts("  cat      - Print file contents\n");
    vga_puts("  write    - write <file> <text> - write text to a file\n");
    vga_puts("  rm       - Remove a file\n");
    vga_puts("  ansi     - Demo ANSI escape-code colours\n");
    vga_puts_color("\n  Milestones (to implement):\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  kill     - [L09] Terminate a process\n");
    vga_puts("  free     - [L11] Show free memory\n\n");
}

static void cmd_clear(void) { vga_clear(VGA_BLACK); }

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Scheduler    : Round-robin, 100Hz PIT tick\n");
    vga_puts("  Sync         : Kernel threads, mutex, semaphore\n");
    vga_puts("  Course       : SENG 21213 - Sem 2\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_mem(void) {
    uint32_t total = pmm_total_frames();
    uint32_t free_f = pmm_free_frames();
    uint32_t used  = pmm_used_frames();
    vga_puts_color("\n  Physical Memory Manager\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");
    vga_printf("  Total : %u KB  (%u frames)\n", total * 4, total);
    vga_printf("  Used  : %u KB  (%u frames)\n", used * 4, used);
    vga_printf("  Free  : %u KB  (%u frames)\n\n", free_f * 4, free_f);
}

static const char *state_name(proc_state_t s) {
    switch (s) {
        case READY:      return "READY";
        case RUNNING:    return "RUNNING";
        case BLOCKED:    return "BLOCKED";
        default:         return "TERMINATED";
    }
}

/* Stage 3 deliverable: allocate 100 frames, verify they are all
 * distinct (no double-allocation), free them all, and confirm the
 * free-frame count returns to exactly what it was before -- proof
 * the allocator has no leaks. */
static void cmd_memtest(void) {
    #define MEMTEST_N 100
    static uint32_t frames[MEMTEST_N];
    uint32_t before_free = pmm_free_frames();
    int i, j, ok = 1, duplicates = 0;

    for (i = 0; i < MEMTEST_N; i++) {
        frames[i] = pmm_alloc_frame();
        if (frames[i] == 0) { ok = 0; break; }
    }

    for (i = 0; i < MEMTEST_N && ok; i++) {
        for (j = i + 1; j < MEMTEST_N; j++) {
            if (frames[i] == frames[j]) duplicates++;
        }
    }

    for (i = 0; i < MEMTEST_N; i++) {
        if (frames[i] != 0) pmm_free_frame(frames[i]);
    }

    uint32_t after_free = pmm_free_frames();

    vga_puts_color("\n  PMM stress test: alloc 100 frames, verify, free 100 frames\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_printf("  free frames before : %u\n", before_free);
    vga_printf("  free frames after  : %u\n", after_free);
    vga_printf("  duplicate addresses: %u\n", duplicates);

    if (ok && duplicates == 0 && after_free == before_free) {
        vga_puts_color("  -> PASS: no leaks, no double-allocation\n\n",
                       VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        vga_puts_color("  -> FAIL\n\n", VGA_LIGHT_RED, VGA_BLACK);
    }
}

static void cmd_ps(void) {
    int i;
    vga_puts_color("\n  PID   STATE       KIND\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  -----------------------------------\n");
    for (i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *p = process_get(i);
        if (!p) continue;
        vga_printf("  %u     %s     %s", p->pid, state_name(p->state),
                   p->thread_fn ? "thread" : "process");
        if (p == current_process) {
            vga_puts_color("  <-- running now", VGA_LIGHT_GREEN, VGA_BLACK);
        }
        vga_puts("\n");
    }
    vga_puts("\n");
}

static void cmd_threads(void) {
    int i, found = 0;
    vga_puts_color("\n  PID   STATE\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  -----------------------------\n");
    for (i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *p = process_get(i);
        if (!p || !p->thread_fn) continue;
        found = 1;
        vga_printf("  %u     %s\n", p->pid, state_name(p->state));
    }
    if (!found) vga_puts("  (no threads currently running)\n");
    vga_puts("\n");
}

static void demo_process_a(void) {
    uint32_t last = 0;
    uint32_t n = 0;
    for (;;) {
        uint32_t now = scheduler_ticks();
        if (now - last >= 20) {
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
        if (now - last >= 50) {
            last = now;
            vga_putchar_at(23, 10 + (int)(n % 20), (char)('0' + (n % 10)),
                            VGA_LIGHT_RED, VGA_BLACK);
            n++;
        }
    }
}

/* ---------------------------------------------------------------------------
 * Lecture 10 demo state: race condition + producer/consumer
 * --------------------------------------------------------------------------*/
#define RACE_ITERATIONS 30

static volatile int myglobal = 0;
static mutex_t       myglobal_mutex;
static semaphore_t   race_done;

#define PC_BUF_SIZE 5
#define PC_ITEMS    20

static int          pc_buffer[PC_BUF_SIZE];
static int          pc_in = 0, pc_out = 0;
static semaphore_t  pc_empty, pc_full, pc_mutex_sem, pc_done;
static int          pc_consumer_ok = 0;

/* Two threads increment myglobal RACE_ITERATIONS times each. Each
 * increment is deliberately split into load -> delay -> store so a
 * 10ms timer tick has a real chance to land in the middle of it,
 * exactly like a real unsynchronised race. With use_mutex, the whole
 * load-delay-store sequence becomes a critical section. */
typedef struct { bool use_mutex; } race_args_t;

static void race_worker(void *arg) {
    race_args_t *a = (race_args_t *)arg;
    int i;
    for (i = 0; i < RACE_ITERATIONS; i++) {
        if (a->use_mutex) mutex_lock(&myglobal_mutex);

        int temp = myglobal;
        __asm__ __volatile__("int $32");  /* force a real context switch here, every time */
        myglobal = temp + 1;

        if (a->use_mutex) mutex_unlock(&myglobal_mutex);
    }
    sem_signal(&race_done);
}

static void cmd_race(bool use_mutex) {
    static race_args_t args;
    myglobal = 0;
    args.use_mutex = use_mutex;
    sem_init(&race_done, 0);

    vga_printf("\n  Running %s mutex - two threads x %d increments each...\n",
               use_mutex ? "WITH" : "WITHOUT", RACE_ITERATIONS);

    thread_create(race_worker, &args);
    thread_create(race_worker, &args);

    sem_wait(&race_done);
    sem_wait(&race_done);

    int expected = RACE_ITERATIONS * 2;
    vga_printf("  myglobal = %d  (expected %d)", myglobal, expected);
    if (myglobal == expected) {
        vga_puts_color("  -> CORRECT\n\n", VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        vga_puts_color("  -> LOST UPDATES (race condition!)\n\n", VGA_LIGHT_RED, VGA_BLACK);
    }
}

/* Classic bounded-buffer producer/consumer with 3 semaphores:
 * pc_empty counts free slots, pc_full counts filled slots, pc_mutex_sem
 * protects the shared indices. */
static void producer(void *arg) {
    (void)arg;
    int i;
    for (i = 0; i < PC_ITEMS; i++) {
        sem_wait(&pc_empty);
        sem_wait(&pc_mutex_sem);
        pc_buffer[pc_in] = i;
        pc_in = (pc_in + 1) % PC_BUF_SIZE;
        sem_signal(&pc_mutex_sem);
        sem_signal(&pc_full);
    }
    sem_signal(&pc_done);
}

static void consumer(void *arg) {
    (void)arg;
    int i;
    int expected_next = 0;
    pc_consumer_ok = 1;
    for (i = 0; i < PC_ITEMS; i++) {
        sem_wait(&pc_full);
        sem_wait(&pc_mutex_sem);
        int val = pc_buffer[pc_out];
        pc_out = (pc_out + 1) % PC_BUF_SIZE;
        sem_signal(&pc_mutex_sem);
        sem_signal(&pc_empty);

        if (val != expected_next) pc_consumer_ok = 0;
        expected_next++;
    }
    sem_signal(&pc_done);
}

static void cmd_pc(void) {
    pc_in = 0;
    pc_out = 0;
    sem_init(&pc_empty, PC_BUF_SIZE);
    sem_init(&pc_full, 0);
    sem_init(&pc_mutex_sem, 1);
    sem_init(&pc_done, 0);

    vga_printf("\n  Producer-consumer: %d items through a %d-slot buffer...\n",
               PC_ITEMS, PC_BUF_SIZE);

    thread_create(producer, 0);
    thread_create(consumer, 0);

    sem_wait(&pc_done);
    sem_wait(&pc_done);

    if (pc_consumer_ok) {
        vga_puts_color("  All items consumed in order, no corruption -> PASS\n\n",
                       VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        vga_puts_color("  Corruption detected in consumed sequence -> FAIL\n\n",
                       VGA_LIGHT_RED, VGA_BLACK);
    }
}

/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static void cmd_ls(void) {
    char name[FS_MAX_FILENAME];
    uint32_t size;
    int idx = 0, found = 0;
    vga_puts_color("\n  NAME                          SIZE\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  -----------------------------------------\n");
    while (fs_list(idx, name, &size)) {
        vga_puts("  ");
        vga_puts(name);
        vga_puts("   ");
        vga_printf("%u\n", size);
        idx++;
        found++;
    }
    vga_printf("  %d file(s)\n\n", found);
}

static void cmd_touch(const char *name) {
    if (k_strlen(name) == 0) { vga_puts("  Usage: touch <file>\n"); return; }
    if (fs_size(name) != (uint32_t)-1) {
        vga_puts("  OK (already exists)\n");
        return;
    }
    if (fs_write(name, "", 0) < 0) {
        vga_puts_color("  Error: could not create file\n", VGA_LIGHT_RED, VGA_BLACK);
    } else {
        vga_puts("  OK\n");
    }
}

static void cmd_cat(const char *name) {
    static char buf[FS_MAX_FILE_SIZE + 1];
    if (k_strlen(name) == 0) { vga_puts("  Usage: cat <file>\n"); return; }
    int n = fs_read(name, buf, FS_MAX_FILE_SIZE);
    if (n < 0) {
        vga_puts_color("  File not found\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }
    buf[n] = 0;
    vga_puts("  ");
    vga_puts(buf);
    vga_puts("\n");
}

static void cmd_write(const char *args) {
    char fname[FS_MAX_FILENAME];
    int i = 0;
    while (args[i] && args[i] != ' ' && i < FS_MAX_FILENAME - 1) { fname[i] = args[i]; i++; }
    fname[i] = 0;
    if (i == 0) { vga_puts("  Usage: write <file> <text>\n"); return; }
    const char *text = k_ltrim(args + i);
    int n = fs_write(fname, text, (uint32_t)k_strlen(text));
    if (n < 0) {
        vga_puts_color("  Error: could not write file\n", VGA_LIGHT_RED, VGA_BLACK);
    } else {
        vga_printf("  OK (%d bytes)\n", n);
    }
}

static void cmd_rm(const char *name) {
    if (k_strlen(name) == 0) { vga_puts("  Usage: rm <file>\n"); return; }
    if (fs_unlink(name) < 0) {
        vga_puts_color("  File not found\n", VGA_LIGHT_RED, VGA_BLACK);
    } else {
        vga_puts("  OK\n");
    }
}

static void cmd_ansi(void) {
    vga_puts("\n");
    vga_puts_ansi("\x1b[31mRed \x1b[32mGreen \x1b[33mYellow \x1b[34mBlue \x1b[35mMagenta \x1b[36mCyan \x1b[37mWhite\x1b[0m\n");
    vga_puts_ansi("\x1b[1;31mBold Red \x1b[1;32mBold Green \x1b[1;34mBold Blue\x1b[0m\n\n");
}

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

        if (k_strcmp(cmd, "help")     == 0) { cmd_help();      continue; }
        if (k_strcmp(cmd, "clear")    == 0) { cmd_clear();     continue; }
        if (k_strcmp(cmd, "about")    == 0) { cmd_about();     continue; }
        if (k_strcmp(cmd, "mem")      == 0) { cmd_mem();       continue; }
        if (k_strcmp(cmd, "ps")       == 0) { cmd_ps();        continue; }
        if (k_strcmp(cmd, "threads")  == 0) { cmd_threads();   continue; }
        if (k_strcmp(cmd, "race")     == 0) { cmd_race(false); continue; }
        if (k_strcmp(cmd, "racesafe") == 0) { cmd_race(true);  continue; }
        if (k_strcmp(cmd, "pc")       == 0) { cmd_pc();        continue; }
        if (k_strcmp(cmd, "memtest")  == 0) { cmd_memtest();   continue; }
        if (k_strcmp(cmd, "ls")         == 0) { cmd_ls();                       continue; }
        if (k_strncmp(cmd, "touch ", 6) == 0) { cmd_touch(k_ltrim(cmd + 6));    continue; }
        if (k_strncmp(cmd, "cat ", 4)   == 0) { cmd_cat(k_ltrim(cmd + 4));      continue; }
        if (k_strncmp(cmd, "write ", 6) == 0) { cmd_write(k_ltrim(cmd + 6));    continue; }
        if (k_strncmp(cmd, "rm ", 3)    == 0) { cmd_rm(k_ltrim(cmd + 3));       continue; }
        if (k_strcmp(cmd, "ansi") == 0) { cmd_ansi(); continue; }

        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

        if (k_strcmp(cmd, "kill") == 0 ||
            k_strcmp(cmd, "free") == 0) {
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

    pmm_init();
    fs_init();
    mutex_init(&myglobal_mutex);

    process_init();
    process_create(shell_run);
    process_create(demo_process_a);
    process_create(demo_process_b);

    scheduler_init();

    for (;;) { __asm__ __volatile__("hlt"); }
}
