#include "vga.h"
#include "keyboard.h"
#include "process.h"
#include "scheduler.h"
#include "thread.h"
#include "mutex.h"
#include "semaphore.h"
#include "pmm.h"
#include "rwlock.h"
#include "deadlock.h"
#include "kmalloc.h"
#include "buddy.h"
#include "ramdisk.h"
#include "fs.h"
#include "vfs.h"
#include "journal.h"
#include "multiboot.h"
#include "../include/types.h"

static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_memtest(void);
static void cmd_ls(void);
static void cmd_ansi(void);
static void cmd_forktest(void);
static void cmd_cpuhog(void);
static void cmd_priotest(void);
static void cmd_rwlocktest(void);
static void cmd_deadlocktest(void);
static void cmd_deadlockcheck(void);
static void cmd_kmalloctest(void);
static void cmd_buddytest(void);
static void cmd_journaltest(void);
static void cmd_indirecttest(void);
static void cmd_mkdir(const char *name);
static void cmd_cd(const char *name);
static void cmd_pwd(void);
static void cmd_dirtest(void);
static void cmd_vfstest(void);
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
    vga_puts("  mkdir    - mkdir <name> - Create a subdirectory\n");
    vga_puts("  cd       - cd <name|..|/> - Change directory\n");
    vga_puts("  pwd      - Print current working directory\n");
    vga_puts("  dirtest  - Test nested subdirectories\n");
    vga_puts("  vfstest  - Test file_ops_t VFS dispatch layer\n");
    vga_puts("  ansi     - Demo ANSI escape-code colours\n");
    vga_puts("  forktest - Demo fork() (duplicate PCB + stack)\n");
    vga_puts("  cpuhog   - Spawn a CPU-bound process (watch it demote in ps)\n");
    vga_puts("  priotest - Demo mutex priority inheritance\n");
    vga_puts("  rwtest   - Demo read-write lock (concurrent readers, exclusive writer)\n");
    vga_puts("  deadlocktest  - Spawn a classic AB-BA deadlock\n");
    vga_puts("  deadlockcheck - Scan the resource graph for a deadlock\n");
    vga_puts("  kmtest        - Demo kmalloc/kfree heap allocator\n");
    vga_puts("  buddytest     - Test buddy split/coalesce allocator\n");
    vga_puts("  journaltest   - Test write-ahead journal recovery\n");
    vga_puts("  indirecttest  - Test Stage 4 single-indirect file blocks (>32KB)\n");
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
    vga_puts_color("\n  PID   STATE       KIND      PRIO\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  -----------------------------------------\n");
    for (i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *p = process_get(i);
        if (!p) continue;
        vga_printf("  %u     %s     %s   %d", p->pid, state_name(p->state),
                   p->thread_fn ? "thread" : "process", p->priority);
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

/* Extension: rewritten to use sleep_ms() instead of busy-checking
 * scheduler_ticks() -- these processes now spend most of their time
 * BLOCKED (visible in `ps`), not burning CPU in a polling loop. */
static void demo_process_a(void) {
    uint32_t n = 0;
    for (;;) {
        vga_putchar_at(22, 10 + (int)(n % 20), (char)('A' + (n % 26)),
                        VGA_LIGHT_CYAN, VGA_BLACK);
        n++;
        sleep_ms(200);
    }
}

static void demo_process_b(void) {
    uint32_t n = 0;
    for (;;) {
        vga_putchar_at(23, 10 + (int)(n % 20), (char)('0' + (n % 10)),
                        VGA_LIGHT_RED, VGA_BLACK);
        n++;
        sleep_ms(500);
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
    while (vfs_list(idx, name, &size)) {
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
    if (vfs_size(name) != (uint32_t)-1) {
        vga_puts("  OK (already exists)\n");
        return;
    }
    if (vfs_write(name, "", 0) < 0) {
        vga_puts_color("  Error: could not create file\n", VGA_LIGHT_RED, VGA_BLACK);
    } else {
        vga_puts("  OK\n");
    }
}

static void cmd_cat(const char *name) {
    /* Do NOT size this buffer with FS_MAX_FILE_SIZE. With the indirect
     * extension that value is ~4MB and would make the kernel .bss overlap
     * physical regions used elsewhere. The shell only needs a text preview. */
    static char buf[4096];

    if (k_strlen(name) == 0) {
        vga_puts("  Usage: cat <file>\n");
        return;
    }

    uint32_t file_size = vfs_size(name);
    if (file_size == (uint32_t)-1) {
        vga_puts_color("  File not found\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    int n = vfs_read(name, buf, sizeof(buf) - 1);
    if (n < 0) {
        vga_puts_color("  File not found\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    buf[n] = 0;
    vga_puts("  ");
    vga_puts(buf);

    if (file_size > (uint32_t)n) {
        vga_puts("\n  [cat output truncated to 4095 bytes]");
    }

    vga_puts("\n");
}

static void cmd_write(const char *args) {
    char fname[FS_MAX_FILENAME];
    int i = 0;
    while (args[i] && args[i] != ' ' && i < FS_MAX_FILENAME - 1) { fname[i] = args[i]; i++; }
    fname[i] = 0;
    if (i == 0) { vga_puts("  Usage: write <file> <text>\n"); return; }
    const char *text = k_ltrim(args + i);
    int n = vfs_write(fname, text, (uint32_t)k_strlen(text));
    if (n < 0) {
        vga_puts_color("  Error: could not write file\n", VGA_LIGHT_RED, VGA_BLACK);
    } else {
        vga_printf("  OK (%d bytes)\n", n);
    }
}

static void cmd_rm(const char *name) {
    if (k_strlen(name) == 0) { vga_puts("  Usage: rm <file>\n"); return; }
    if (vfs_unlink(name) < 0) {
        vga_puts_color("  File not found\n", VGA_LIGHT_RED, VGA_BLACK);
    } else {
        vga_puts("  OK\n");
    }
}

/* Stage 4 bonus: single-indirect inode test.
 *
 * 40KB + 123 bytes crosses the old 32KB direct-block limit:
 *   first 8 blocks  -> inode direct pointers
 *   remaining data -> single-indirect pointer table
 *
 * One static buffer is reused for both write and read verification so the
 * test does not require a large kernel stack allocation.
 */
#define INDIRECT_TEST_SIZE (40U * 1024U + 123U)

static uint8_t indirect_test_buf[INDIRECT_TEST_SIZE];

static uint8_t indirect_test_pattern(uint32_t i) {
    return (uint8_t)((i * 37U + 11U) & 0xFFU);
}

static void cmd_indirecttest(void) {
    const char *name = "indirect.bin";
    uint32_t i;
    int n;

    vga_puts_color("\n  Single-indirect block test\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");
    vga_printf("  Test size: %u bytes (> 32768 direct limit)\n",
               (uint32_t)INDIRECT_TEST_SIZE);

    /* Remove leftovers from a previous interrupted test. */
    vfs_unlink(name);

    /* Generate deterministic binary data. */
    for (i = 0; i < INDIRECT_TEST_SIZE; i++) {
        indirect_test_buf[i] = indirect_test_pattern(i);
    }

    n = vfs_write(name,
                 (const char *)indirect_test_buf,
                 (uint32_t)INDIRECT_TEST_SIZE);

    if (n != (int)INDIRECT_TEST_SIZE) {
        vga_printf("  FAIL: write returned %d bytes\n", n);
        vfs_unlink(name);
        return;
    }

    if (vfs_size(name) != (uint32_t)INDIRECT_TEST_SIZE) {
        vga_puts_color("  FAIL: file size mismatch\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        vfs_unlink(name);
        return;
    }

    /* Destroy the original contents before reading them back. */
    for (i = 0; i < INDIRECT_TEST_SIZE; i++) {
        indirect_test_buf[i] = 0;
    }

    n = vfs_read(name,
                (char *)indirect_test_buf,
                (uint32_t)INDIRECT_TEST_SIZE);

    if (n != (int)INDIRECT_TEST_SIZE) {
        vga_printf("  FAIL: read returned %d bytes\n", n);
        vfs_unlink(name);
        return;
    }

    for (i = 0; i < INDIRECT_TEST_SIZE; i++) {
        if (indirect_test_buf[i] != indirect_test_pattern(i)) {
            vga_printf("  FAIL: data mismatch at byte %u\n", i);
            vfs_unlink(name);
            return;
        }
    }

    if (vfs_unlink(name) < 0) {
        vga_puts_color("  FAIL: could not unlink test file\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    if (vfs_size(name) != (uint32_t)-1) {
        vga_puts_color("  FAIL: inode still visible after unlink\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    vga_puts_color(
        "  PASS: 40KB+ write/read crossed direct -> indirect boundary\n",
        VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts_color(
        "  PASS: data verified byte-for-byte and blocks released on unlink\n\n",
        VGA_LIGHT_GREEN, VGA_BLACK);
}


/* --------------------------------------------------------------------------
 * Stage 4 bonus -- hierarchical filesystem shell commands.
 * -------------------------------------------------------------------------- */

static void cmd_mkdir(const char *name) {
    if (k_strlen(name) == 0) {
        vga_puts("  Usage: mkdir <name>\n");
        return;
    }

    if (vfs_mkdir(name) < 0) {
        vga_puts_color(
            "  Error: could not create directory (exists/invalid/full)\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    vga_puts("  OK\n");
}

static void cmd_cd(const char *name) {
    char cwd[256];

    if (k_strlen(name) == 0) {
        vga_puts("  Usage: cd <name|..|/>\n");
        return;
    }

    if (vfs_chdir(name) < 0) {
        vga_puts_color(
            "  Error: directory not found\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    if (vfs_getcwd(cwd, sizeof(cwd)) == 0) {
        vga_puts("  ");
        vga_puts(cwd);
        vga_puts("\n");
    }
}

static void cmd_pwd(void) {
    char cwd[256];

    if (vfs_getcwd(cwd, sizeof(cwd)) < 0) {
        vga_puts_color(
            "  Error: could not construct current path\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    vga_puts("  ");
    vga_puts(cwd);
    vga_puts("\n");
}

/* Automated hierarchy test.
 *
 * Creates:
 *
 *   /alpha/
 *       beta/
 *           note
 *
 * Then verifies:
 *   - cwd == /alpha/beta
 *   - nested file write/read works
 *   - cd .. returns to /alpha
 *   - cd / returns to root
 *
 * The test is intentionally idempotent enough for repeated use:
 * existing alpha/beta directories are accepted.
 */
static void cmd_dirtest(void) {
    static char read_buf[64];
    char cwd[256];
    const char *msg = "NestedDirectoryWorks";
    int n;
    int i;

    vga_puts_color("\n  Hierarchical directory test\n",
                   VGA_LIGHT_CYAN,
                   VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");

    if (vfs_chdir("/") < 0) {
        vga_puts_color("  FAIL: could not enter root\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    if (vfs_mkdir("alpha") < 0 && !vfs_is_dir("alpha")) {
        vga_puts_color("  FAIL: could not create /alpha\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    if (vfs_chdir("alpha") < 0) {
        vga_puts_color("  FAIL: could not cd /alpha\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    if (vfs_mkdir("beta") < 0 && !vfs_is_dir("beta")) {
        vga_puts_color("  FAIL: could not create /alpha/beta\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        vfs_chdir("/");
        return;
    }

    if (vfs_chdir("beta") < 0) {
        vga_puts_color("  FAIL: could not cd /alpha/beta\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        vfs_chdir("/");
        return;
    }

    if (vfs_getcwd(cwd, sizeof(cwd)) < 0 ||
        k_strcmp(cwd, "/alpha/beta") != 0) {
        vga_puts_color("  FAIL: pwd mismatch\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        vfs_chdir("/");
        return;
    }

    n = vfs_write("note", msg, (uint32_t)k_strlen(msg));

    if (n != (int)k_strlen(msg)) {
        vga_puts_color("  FAIL: nested file write failed\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        vfs_chdir("/");
        return;
    }

    for (i = 0; i < (int)sizeof(read_buf); i++) {
        read_buf[i] = 0;
    }

    n = vfs_read("note", read_buf, sizeof(read_buf) - 1);

    if (n != (int)k_strlen(msg)) {
        vga_puts_color("  FAIL: nested file read failed\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        vfs_chdir("/");
        return;
    }

    read_buf[n] = 0;

    if (k_strcmp(read_buf, msg) != 0) {
        vga_puts_color("  FAIL: nested file contents differ\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        vfs_chdir("/");
        return;
    }

    if (vfs_chdir("..") < 0 ||
        vfs_getcwd(cwd, sizeof(cwd)) < 0 ||
        k_strcmp(cwd, "/alpha") != 0) {
        vga_puts_color("  FAIL: cd .. did not reach /alpha\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        vfs_chdir("/");
        return;
    }

    if (vfs_chdir("/") < 0 ||
        vfs_getcwd(cwd, sizeof(cwd)) < 0 ||
        k_strcmp(cwd, "/") != 0) {
        vga_puts_color("  FAIL: cd / did not reach root\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    vga_puts_color(
        "  PASS: mkdir + nested cd + pwd hierarchy works\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK);

    vga_puts_color(
        "  PASS: nested file write/read and parent navigation work\n\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK);
}


/* --------------------------------------------------------------------------
 * Stage 4 bonus -- VFS abstraction test.
 *
 * IMPORTANT: every filesystem operation below goes through vfs_*(), not
 * directly through fs_*(). The VFS layer dispatches through file_ops_t to
 * the currently mounted RAM-disk filesystem backend.
 * -------------------------------------------------------------------------- */
static void cmd_vfstest(void) {
    static char buf[64];
    const char *name = "vfsdemo";
    const char *msg  = "VFS dispatch works";
    int expected = (int)k_strlen(msg);
    int n;
    int i;

    vga_puts_color("\n  VFS file_ops_t dispatch test\n",
                   VGA_LIGHT_CYAN,
                   VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");

    vga_puts("  Backend: ");
    vga_puts(vfs_backend_name());
    vga_puts("\n");

    if (vfs_chdir("/") < 0) {
        vga_puts_color("  FAIL: VFS could not enter root\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    /* Remove an old test file if a previous run left one behind. */
    vfs_unlink(name);

    n = vfs_write(name, msg, (uint32_t)expected);

    if (n != expected) {
        vga_puts_color("  FAIL: vfs_write dispatch failed\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    if (vfs_size(name) != (uint32_t)expected) {
        vga_puts_color("  FAIL: vfs_size dispatch failed\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        vfs_unlink(name);
        return;
    }

    for (i = 0; i < (int)sizeof(buf); i++) {
        buf[i] = 0;
    }

    n = vfs_read(name, buf, sizeof(buf) - 1);

    if (n != expected) {
        vga_puts_color("  FAIL: vfs_read dispatch failed\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        vfs_unlink(name);
        return;
    }

    buf[n] = 0;

    if (k_strcmp(buf, msg) != 0) {
        vga_puts_color("  FAIL: VFS read-back data mismatch\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        vfs_unlink(name);
        return;
    }

    if (vfs_unlink(name) < 0) {
        vga_puts_color("  FAIL: vfs_unlink dispatch failed\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    if (vfs_size(name) != (uint32_t)-1) {
        vga_puts_color("  FAIL: file remained visible after VFS unlink\n",
                       VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    vga_puts_color(
        "  PASS: write/read/size/unlink dispatched through file_ops_t\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK);

    vga_puts_color(
        "  PASS: RAM filesystem is hidden behind generic VFS API\n\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK);
}

/* Extension: priority inheritance demo state. */
static mutex_t  pi_mutex;
static uint32_t pi_high_wait_start;

static void pi_low_worker(void *arg) {
    (void)arg;
    mutex_lock(&pi_mutex);
    vga_puts_color("\n  [LOW]    acquired pi_mutex, doing CPU-bound work...\n",
                   VGA_LIGHT_RED, VGA_BLACK);
    /* Hold the lock for at least 30 SCHEDULER TICKS (~300ms of
     * scheduled time), not a fixed instruction count -- a raw
     * iteration count finishes in well under 10ms on real hardware,
     * releasing the mutex before HIGH even starts waiting and making
     * the whole demo a no-op. Ticks are tied to the emulated PIT, so
     * this is consistent regardless of host CPU speed. */
    uint32_t start = scheduler_ticks();
    while (scheduler_ticks() - start < 30) {
        volatile uint32_t j;
        for (j = 0; j < 5000; j++) { }
    }
    vga_puts_color("  [LOW]    releasing pi_mutex\n", VGA_LIGHT_RED, VGA_BLACK);
    mutex_unlock(&pi_mutex);
}

static void pi_medium_worker(void *arg) {
    (void)arg;
    /* Pure CPU hog -- never touches pi_mutex at all. Without priority
     * inheritance, this is what would starve LOW out (and therefore
     * HIGH, indirectly) via ordinary MLFQ scheduling. Tick-bound (not
     * a fixed iteration count) so it keeps contending for the WHOLE
     * span of LOW's critical section, regardless of host CPU speed. */
    uint32_t start = scheduler_ticks();
    while (scheduler_ticks() - start < 40) {
        volatile uint32_t j;
        for (j = 0; j < 5000; j++) { }
    }
}

static void pi_high_worker(void *arg) {
    (void)arg;
    sleep_ms(50);   /* give LOW a head start so it grabs the mutex first,
                      * but still well inside LOW's ~300ms critical section */
    vga_puts_color("  [HIGH]   trying to lock pi_mutex...\n", VGA_LIGHT_GREEN, VGA_BLACK);
    pi_high_wait_start = scheduler_ticks();
    mutex_lock(&pi_mutex);
    uint32_t waited = scheduler_ticks() - pi_high_wait_start;
    vga_printf("  [HIGH]   acquired pi_mutex after %u ticks (~%u ms) waiting\n\n",
               waited, waited * 10);
    mutex_unlock(&pi_mutex);
}

/* Extension: rwlock demo state. */
static rwlock_t rw_lock;
static int      rw_shared_value = 0;

static void rw_reader_worker(void *arg) {
    int id = (int)(long)arg;
    rwlock_read_lock(&rw_lock);
    vga_printf("\n  [READER %d] acquired (active readers: %d), value=%d\n",
               id, rwlock_active_readers(&rw_lock), rw_shared_value);
    uint32_t start = scheduler_ticks();
    while (scheduler_ticks() - start < 15) {
        volatile uint32_t j;
        for (j = 0; j < 5000; j++) { }
    }
    vga_printf("  [READER %d] releasing\n", id);
    rwlock_read_unlock(&rw_lock);
}

static void rw_writer_worker(void *arg) {
    (void)arg;
    sleep_ms(30);
    vga_puts_color("  [WRITER]  waiting for exclusive access...\n", VGA_YELLOW, VGA_BLACK);
    rwlock_write_lock(&rw_lock);
    rw_shared_value += 100;
    vga_printf("  [WRITER]  acquired exclusively, wrote value=%d\n\n", rw_shared_value);
    rwlock_write_unlock(&rw_lock);
}

/* Extension: deadlock detector demo state -- a classic AB-BA
 * deadlock. T1 locks A then tries B; T2 locks B then tries A. */
static mutex_t dl_mutex_a, dl_mutex_b;

static void dl_thread1(void *arg) {
    (void)arg;
    vga_puts_color("\n  [T1] locking A...\n", VGA_LIGHT_CYAN, VGA_BLACK);
    mutex_lock(&dl_mutex_a);
    deadlock_note_owner(&dl_mutex_a, current_process);
    vga_puts_color("  [T1] got A, sleeping briefly...\n", VGA_LIGHT_CYAN, VGA_BLACK);
    sleep_ms(50);
    vga_puts_color("  [T1] locking B...\n", VGA_LIGHT_CYAN, VGA_BLACK);
    deadlock_note_waiting(current_process, &dl_mutex_b);
    mutex_lock(&dl_mutex_b);   /* blocks forever if T2 is holding B and wants A */
    deadlock_note_done_waiting(current_process);
    deadlock_note_owner(&dl_mutex_b, current_process);
    vga_puts_color("  [T1] got B (only reachable if NOT deadlocked)\n", VGA_LIGHT_CYAN, VGA_BLACK);
    mutex_unlock(&dl_mutex_b);
    mutex_unlock(&dl_mutex_a);
}

static void dl_thread2(void *arg) {
    (void)arg;
    vga_puts_color("  [T2] locking B...\n", VGA_LIGHT_MAGENTA, VGA_BLACK);
    mutex_lock(&dl_mutex_b);
    deadlock_note_owner(&dl_mutex_b, current_process);
    vga_puts_color("  [T2] got B, sleeping briefly...\n", VGA_LIGHT_MAGENTA, VGA_BLACK);
    sleep_ms(50);
    vga_puts_color("  [T2] locking A...\n", VGA_LIGHT_MAGENTA, VGA_BLACK);
    deadlock_note_waiting(current_process, &dl_mutex_a);
    mutex_lock(&dl_mutex_a);   /* blocks forever if T1 is holding A and wants B */
    deadlock_note_done_waiting(current_process);
    deadlock_note_owner(&dl_mutex_a, current_process);
    vga_puts_color("  [T2] got A (only reachable if NOT deadlocked)\n", VGA_LIGHT_MAGENTA, VGA_BLACK);
    mutex_unlock(&dl_mutex_a);
    mutex_unlock(&dl_mutex_b);
}



static void cmd_journaltest(void) {
    static char buf[64];
    const char *name = "jrecover";
    const char *msg = "RecoveredFromJournal";
    int expected = (int)k_strlen(msg);
    int n;
    int recovered;
    int i;

    vga_puts_color("\n  Write-ahead journal recovery test\n",
                   VGA_LIGHT_CYAN,
                   VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");

    vfs_chdir("/");

    /* Clean up a previous normal run if necessary. */
    vfs_unlink(name);

    /*
     * The next filesystem commit writes a complete COMMITTED redo log
     * but deliberately skips checkpointing it to home metadata.
     */
    journal_test_defer_next_commit();

    n = vfs_write(name, msg, (uint32_t)expected);

    if (n != expected) {
        vga_puts_color(
            "  FAIL: journaled write failed\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    if (journal_state() != JOURNAL_STATE_COMMITTED) {
        vga_puts_color(
            "  FAIL: committed journal record was not preserved\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    /*
     * Home metadata must still represent the pre-transaction filesystem.
     * The new file becomes visible only after redo recovery.
     */
    if (vfs_size(name) != (uint32_t)-1) {
        vga_puts_color(
            "  FAIL: metadata reached home before recovery\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    recovered = journal_recover();

    if (recovered != 1) {
        vga_puts_color(
            "  FAIL: committed journal was not replayed\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    if (journal_state() != JOURNAL_STATE_CLEAN) {
        vga_puts_color(
            "  FAIL: journal was not cleared after checkpoint\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    for (i = 0; i < (int)sizeof(buf); i++) {
        buf[i] = 0;
    }

    n = vfs_read(name, buf, sizeof(buf) - 1);

    if (n != expected) {
        vga_puts_color(
            "  FAIL: recovered file could not be read\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    buf[n] = 0;

    if (k_strcmp(buf, msg) != 0) {
        vga_puts_color(
            "  FAIL: recovered file data mismatch\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    if (vfs_unlink(name) < 0) {
        vga_puts_color(
            "  FAIL: cleanup unlink failed\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    vga_puts_color(
        "  PASS: committed redo log survived simulated crash point\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK);

    vga_puts_color(
        "  PASS: recovery replayed metadata before clearing journal\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK);

    vga_puts_color(
        "  PASS: recovered file data matched byte-for-byte\n\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK);
}

static void cmd_buddytest(void) {
    void *a;
    void *b;
    void *c;
    void *whole;
    uint32_t base = buddy_pool_base();

    vga_puts_color("\n  Buddy allocator test\n",
                   VGA_LIGHT_CYAN,
                   VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");

    if (!base) {
        vga_puts_color(
            "  FAIL: buddy pool could not be reserved from PMM\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    vga_puts("  Pool base: ");
    vga_printf("0x%x\n", base);

    if (buddy_free_pages() != BUDDY_POOL_PAGES ||
        buddy_free_block_count(BUDDY_MAX_ORDER) != 1) {
        vga_puts_color(
            "  FAIL: initial 1 MiB buddy pool is not fully coalesced\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    a = buddy_alloc(0);   /*  4 KiB */
    b = buddy_alloc(1);   /*  8 KiB */
    c = buddy_alloc(2);   /* 16 KiB */

    if (!a || !b || !c) {
        vga_puts_color(
            "  FAIL: split allocation returned NULL\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);

        if (c) buddy_free(c, 2);
        if (b) buddy_free(b, 1);
        if (a) buddy_free(a, 0);
        return;
    }

    if (buddy_free_pages() !=
        BUDDY_POOL_PAGES - 1U - 2U - 4U) {
        vga_puts_color(
            "  FAIL: buddy free-page accounting incorrect\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);

        buddy_free(c, 2);
        buddy_free(b, 1);
        buddy_free(a, 0);
        return;
    }

    /* Touch the allocated memory to prove each returned block is usable. */
    ((volatile uint32_t *)a)[0] = 0x11111111U;
    ((volatile uint32_t *)b)[0] = 0x22222222U;
    ((volatile uint32_t *)c)[0] = 0x33333333U;

    ((volatile uint32_t *)a)[(4096U / 4U) - 1U] =
        0xAAAAAAAAU;

    ((volatile uint32_t *)b)[(8192U / 4U) - 1U] =
        0xBBBBBBBBU;

    ((volatile uint32_t *)c)[(16384U / 4U) - 1U] =
        0xCCCCCCCCU;

    if (buddy_free(c, 2) < 0 ||
        buddy_free(b, 1) < 0 ||
        buddy_free(a, 0) < 0) {
        vga_puts_color(
            "  FAIL: buddy_free rejected a valid block\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    if (buddy_free_pages() != BUDDY_POOL_PAGES ||
        buddy_free_block_count(BUDDY_MAX_ORDER) != 1) {
        vga_puts_color(
            "  FAIL: buddies did not coalesce back to order 8\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);
        return;
    }

    /*
     * Final proof of coalescing: after freeing the small blocks, the
     * allocator must once again satisfy one full 1 MiB order-8 request.
     */
    whole = buddy_alloc(BUDDY_MAX_ORDER);

    if (!whole || (uint32_t)whole != base) {
        vga_puts_color(
            "  FAIL: full order-8 block was not reconstructed\n\n",
            VGA_LIGHT_RED,
            VGA_BLACK);

        if (whole) buddy_free(whole, BUDDY_MAX_ORDER);
        return;
    }

    buddy_free(whole, BUDDY_MAX_ORDER);

    vga_puts_color(
        "  PASS: order-0/1/2 allocations split larger buddy blocks\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK);

    vga_puts_color(
        "  PASS: free blocks coalesced back into one 1 MiB order-8 block\n\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK);
}

static void cmd_kmalloctest(void) {
    vga_puts("\n  kmalloc/kfree test...\n");
    char *a = (char *)kmalloc(32);
    char *b = (char *)kmalloc(64);
    char *c = (char *)kmalloc(16);
    if (!a || !b || !c) {
        vga_puts_color("  FAIL: kmalloc returned NULL\n\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }
    int i, ok = 1;
    for (i = 0; i < 32; i++) a[i] = 'A';
    for (i = 0; i < 64; i++) b[i] = 'B';
    for (i = 0; i < 16; i++) c[i] = 'C';

    kfree(b);
    char *d = (char *)kmalloc(20);   /* should reuse space freed by b */
    for (i = 0; i < 20; i++) d[i] = 'D';

    for (i = 0; i < 32; i++) if (a[i] != 'A') ok = 0;
    for (i = 0; i < 16; i++) if (c[i] != 'C') ok = 0;
    for (i = 0; i < 20; i++) if (d[i] != 'D') ok = 0;

    kfree(a); kfree(c); kfree(d);

    if (ok) {
        vga_puts_color("  PASS: allocations isolated, data intact after free/reuse\n\n",
                       VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        vga_puts_color("  FAIL: data corruption detected\n\n", VGA_LIGHT_RED, VGA_BLACK);
    }
}

static void cmd_deadlocktest(void) {
    mutex_init(&dl_mutex_a);
    mutex_init(&dl_mutex_b);
    deadlock_register_resource(&dl_mutex_a);
    deadlock_register_resource(&dl_mutex_b);
    vga_puts("\n  Deadlock demo: T1 locks A then wants B, T2 locks B then wants A.\n");
    vga_puts("  Wait a moment, then run `deadlockcheck`.\n");
    thread_create(dl_thread1, 0);
    thread_create(dl_thread2, 0);
}

static void cmd_deadlockcheck(void) {
    if (deadlock_check()) {
        vga_puts_color("\n  DEADLOCK DETECTED -- the resource-allocation graph has a cycle.\n\n",
                       VGA_LIGHT_RED, VGA_BLACK);
    } else {
        vga_puts_color("\n  No deadlock detected.\n\n", VGA_LIGHT_GREEN, VGA_BLACK);
    }
}

static void cmd_rwlocktest(void) {
    rwlock_init(&rw_lock);
    rw_shared_value = 0;
    vga_puts("\n  RW-lock demo: 3 readers (watch them overlap), 1 writer (exclusive)...\n");
    thread_create(rw_reader_worker, (void *)1);
    thread_create(rw_reader_worker, (void *)2);
    thread_create(rw_reader_worker, (void *)3);
    thread_create(rw_writer_worker, 0);
}

static void cmd_priotest(void) {
    mutex_init(&pi_mutex);
    vga_puts("\n  Priority inheritance demo: LOW holds pi_mutex, MEDIUM hogs\n");
    vga_puts("  the CPU (never touches the mutex), HIGH waits for it...\n");
    thread_create(pi_low_worker, 0);
    thread_create(pi_medium_worker, 0);
    thread_create(pi_high_worker, 0);
}

static void cpuhog_process(void) {
    uint32_t counter = 0;
    for (;;) {
        counter++;   /* pure CPU-bound busy loop -- never sleeps or blocks */
        if ((counter & 0xFFFFF) == 0) {
            /* Periodic screen update proves it's genuinely spinning,
             * and keeps the compiler from warning that counter is
             * "set but not used". */
            vga_putchar_at(24, 60, (char)('0' + ((counter >> 20) % 10)),
                            VGA_YELLOW, VGA_BLACK);
        }
    }
}

static void cmd_cpuhog(void) {
    pcb_t *p = process_create(cpuhog_process);
    if (p) {
        vga_printf("\n  Spawned CPU-bound PID %u -- never sleeps, watch it demote in ps.\n\n", p->pid);
    } else {
        vga_puts_color("  Could not spawn -- process table full\n\n", VGA_LIGHT_RED, VGA_BLACK);
    }
}

static void cmd_forktest(void) {
    vga_puts("\n  Calling fork()...\n");
    int result = fork();
    if (result == 0) {
        /* Child: prove it is genuinely a separate process, then exit --
         * otherwise it would fall back into shell_run's loop too,
         * competing with the parent for the same keyboard input. */
        vga_puts_color("  [CHILD]  fork() returned 0 -- I am the child. Exiting.\n",
                       VGA_LIGHT_CYAN, VGA_BLACK);
        process_exit();
    } else if (result > 0) {
        vga_printf("  [PARENT] fork() returned child PID %d -- shell continues normally.\n\n", result);
    } else {
        vga_puts_color("  [PARENT] fork() failed (out of process slots)\n\n", VGA_LIGHT_RED, VGA_BLACK);
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
        if (k_strncmp(cmd, "mkdir ", 6) == 0) { cmd_mkdir(k_ltrim(cmd + 6));    continue; }
        if (k_strncmp(cmd, "cd ", 3)    == 0) { cmd_cd(k_ltrim(cmd + 3));       continue; }
        if (k_strcmp(cmd, "pwd")        == 0) { cmd_pwd();                       continue; }
        if (k_strcmp(cmd, "dirtest")    == 0) { cmd_dirtest();                   continue; }
        if (k_strcmp(cmd, "vfstest")    == 0) { cmd_vfstest();                   continue; }
        if (k_strcmp(cmd, "ansi") == 0) { cmd_ansi(); continue; }
        if (k_strcmp(cmd, "forktest") == 0) { cmd_forktest(); continue; }
        if (k_strcmp(cmd, "cpuhog") == 0) { cmd_cpuhog(); continue; }
        if (k_strcmp(cmd, "priotest") == 0) { cmd_priotest(); continue; }
        if (k_strcmp(cmd, "rwtest") == 0) { cmd_rwlocktest(); continue; }
        if (k_strcmp(cmd, "deadlocktest") == 0) { cmd_deadlocktest(); continue; }
        if (k_strcmp(cmd, "deadlockcheck") == 0) { cmd_deadlockcheck(); continue; }
        if (k_strcmp(cmd, "kmtest") == 0) { cmd_kmalloctest(); continue; }
        if (k_strcmp(cmd, "buddytest") == 0) { cmd_buddytest(); continue; }
        if (k_strcmp(cmd, "journaltest") == 0) { cmd_journaltest(); continue; }
        if (k_strcmp(cmd, "indirecttest") == 0) { cmd_indirecttest(); continue; }

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
void kernel_main(uint32_t boot_magic, uint32_t boot_info) {
    int grub_boot;

    /* GRUB cannot execute our real-mode BIOS E820 routine, so import its
     * Multiboot memory map into the same buffer pmm_init() already uses.
     * On the custom boot path this simply returns 0 and leaves BIOS E820
     * data untouched.
     */
    grub_boot = multiboot_prepare_memory_map(boot_magic, boot_info);

    vga_init();
    kb_init();
    print_splash();

    if (grub_boot > 0) {
        vga_puts_color(
            "\n  [BOOT] GRUB2 Multiboot detected - memory map imported\n",
            VGA_LIGHT_GREEN,
            VGA_BLACK);
    }

    pmm_init();
    kmalloc_init();
    fs_init();
    vfs_init();
    buddy_init();
    mutex_init(&myglobal_mutex);

    process_init();
    process_create(shell_run);
    process_create(demo_process_a);
    process_create(demo_process_b);

    scheduler_init();

    for (;;) { __asm__ __volatile__("hlt"); }
}
