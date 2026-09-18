/* =============================================================================
 * SENG21213-OS :: PS/2 Keyboard Driver Implementation
 * File   : kernel/keyboard.c
 * ============================================================================*/
#include "keyboard.h"
#include "vga.h"
#include "../include/types.h"

#define KB_DATA_PORT   0x60
#define KB_STATUS_PORT 0x64
#define KB_STATUS_OBF  0x01

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static const char sc_ascii[128] = {
    0,   27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,   'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,   '\\','z','x','c','v','b','n','m',',','.','/',
    0,   '*', 0, ' ', 0,
    0,0,0,0,0,0,0,0,0,0,
    0, 0,
    '7','8','9','-','4','5','6','+','1','2','3','0','.',
    0,0,0,
    0,0
};

static const char sc_ascii_shift[128] = {
    0,   27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,   'A','S','D','F','G','H','J','K','L',':','"','~',
    0,   '|','Z','X','C','V','B','N','M','<','>','?',
    0,   '*', 0, ' ', 0,
};

static bool shift_held    = false;
static bool capslock_on   = false;
static bool altgr_held    = false;   /* right-Alt: E0-prefixed 0x38 */

void kb_init(void) {
    while (inb(KB_STATUS_PORT) & KB_STATUS_OBF) {
        inb(KB_DATA_PORT);
    }
}

int kb_getchar(void) {
    uint8_t sc;
    static bool extended = false;
    while (true) {
        while (!(inb(KB_STATUS_PORT) & KB_STATUS_OBF));
        sc = inb(KB_DATA_PORT);

        if (sc == 0xE0) {
            extended = true;
            continue;
        }

        if (sc & 0x80) {
            uint8_t release = sc & 0x7F;
            if (release == 0x2A || release == 0x36) shift_held = false;
            /* Right Alt's release is E0 B8 -- only clear altgr_held if
             * the E0 prefix actually preceded this 0x38, distinguishing
             * it from plain Left Alt (0x38 with no E0 prefix). */
            if (release == 0x38 && extended) altgr_held = false;
            extended = false;
            continue;
        }

        if (extended) {
            extended = false;
            switch (sc) {
                case 0x48: return KB_KEY_UP;
                case 0x50: return KB_KEY_DOWN;
                case 0x4B: return KB_KEY_LEFT;
                case 0x4D: return KB_KEY_RIGHT;
                case 0x38: altgr_held = true; continue;   /* Right Alt (AltGr) */
                case 0x49: return KB_KEY_PGUP;
                case 0x51: return KB_KEY_PGDN;
                default:   continue;   /* other extended keys: not handled */
            }
        }

        if (sc == 0x2A || sc == 0x36) { shift_held = true; continue; }
        if (sc == 0x3A) { capslock_on = !capslock_on; continue; }  /* CapsLock: toggle, not held */

        /* QEMU/laptop-friendly scrollback fallback.
         * F11 and F12 are ordinary (non-E0) PS/2 Set-1 scan codes.
         * Reuse the existing PageUp/PageDown events so kb_readline()
         * needs no additional scrollback logic.
         */
        if (sc == 0x57) return KB_KEY_PGUP;  /* F11 -> scroll up */
        if (sc == 0x58) return KB_KEY_PGDN;  /* F12 -> scroll down */

        /* AltGr demo mapping: AltGr+2 -> '@', the same combo many
         * European ISO keyboard layouts use. A full accented-character
         * table is layout-specific and out of scope; this proves the
         * modifier-detection mechanism and gives one worked example. */
        char c;
        if (altgr_held && sc == 0x03) {
            c = '@';
        } else {
            c = shift_held ? sc_ascii_shift[sc] : sc_ascii[sc];
            if (capslock_on) {
                if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
                else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            }
        }
        if (c) return (int)c;
    }
}

/* ---------------------------------------------------------------------------
 * Line editing: cursor movement (Left/Right) + command history (Up/Down)
 * --------------------------------------------------------------------------*/
#define KB_HISTORY_SIZE 20

static char kb_history[KB_HISTORY_SIZE][KB_BUF_SIZE];
static int  kb_history_count = 0;
static int  kb_history_next  = 0;

static void history_push(const char *line) {
    if (line[0] == '\0') return;   /* don't clutter history with empty lines */
    int i = 0;
    while (line[i] && i < KB_BUF_SIZE - 1) { kb_history[kb_history_next][i] = line[i]; i++; }
    kb_history[kb_history_next][i] = '\0';
    kb_history_next = (kb_history_next + 1) % KB_HISTORY_SIZE;
    if (kb_history_count < KB_HISTORY_SIZE) kb_history_count++;
}

/* back=1 is the most recent entry, back=2 the one before that, etc. */
static int history_get(int back, char *out) {
    if (back < 1 || back > kb_history_count) return 0;
    int idx = (kb_history_next - back + KB_HISTORY_SIZE) % KB_HISTORY_SIZE;
    int i = 0;
    while (kb_history[idx][i]) { out[i] = kb_history[idx][i]; i++; }
    out[i] = '\0';
    return 1;
}

/* Redraw the whole line from where it started, pad over any leftover
 * characters from a longer previous render, then put the hardware
 * cursor back at the logical edit position. Assumes the line fits on
 * one row -- fine for normal shell commands, but a very long `write`
 * command could wrap and throw the column math off. */
static void redraw_line(int start_row, int start_col, const char *buf,
                          int line_len, int pos, int *prev_max_len) {
    int i;
    vga_set_cursor(start_row, start_col);
    for (i = 0; i < line_len; i++) vga_putchar(buf[i]);
    for (i = line_len; i < *prev_max_len; i++) vga_putchar(' ');
    *prev_max_len = line_len;
    vga_set_cursor(start_row, start_col + pos);
}

int kb_readline(char *buf, int len) {
    int line_len = 0;
    int pos = 0;
    int prev_max_len = 0;
    int start_row, start_col;
    int hist_back = 0;   /* 0 = editing a fresh line, N = viewing history[N] */

    vga_get_cursor(&start_row, &start_col);
    buf[0] = '\0';

    for (;;) {
        int c = kb_getchar();

        if (c == KB_KEY_PGUP) { vga_scroll_view(10);  continue; }
        if (c == KB_KEY_PGDN) { vga_scroll_view(-10); continue; }
        if (vga_in_scrollback()) {
            /* Any other key means "I'm done browsing" -- snap back to
             * the live view before handling it normally, the same way
             * a real terminal returns to the prompt when you type. */
            vga_scroll_reset();
        }

        if (c == '\n' || c == '\r') {
            vga_putchar('\n');
            break;
        }

        if (c == KB_KEY_LEFT) {
            if (pos > 0) { pos--; vga_set_cursor(start_row, start_col + pos); }
            continue;
        }
        if (c == KB_KEY_RIGHT) {
            if (pos < line_len) { pos++; vga_set_cursor(start_row, start_col + pos); }
            continue;
        }

        if (c == KB_KEY_UP) {
            char tmp[KB_BUF_SIZE];
            if (history_get(hist_back + 1, tmp)) {
                hist_back++;
                int i = 0;
                while (tmp[i] && i < len - 1) { buf[i] = tmp[i]; i++; }
                buf[i] = '\0';
                line_len = i;
                pos = i;
                redraw_line(start_row, start_col, buf, line_len, pos, &prev_max_len);
            }
            continue;
        }
        if (c == KB_KEY_DOWN) {
            if (hist_back > 1) {
                hist_back--;
                char tmp[KB_BUF_SIZE];
                history_get(hist_back, tmp);
                int i = 0;
                while (tmp[i] && i < len - 1) { buf[i] = tmp[i]; i++; }
                buf[i] = '\0';
                line_len = i;
                pos = i;
                redraw_line(start_row, start_col, buf, line_len, pos, &prev_max_len);
            } else if (hist_back == 1) {
                hist_back = 0;
                buf[0] = '\0';
                line_len = 0;
                pos = 0;
                redraw_line(start_row, start_col, buf, line_len, pos, &prev_max_len);
            }
            continue;
        }

        if (c == '\b') {
            if (pos > 0) {
                int i;
                for (i = pos - 1; i < line_len - 1; i++) buf[i] = buf[i + 1];
                line_len--;
                pos--;
                buf[line_len] = '\0';
                redraw_line(start_row, start_col, buf, line_len, pos, &prev_max_len);
            }
            continue;
        }

        if (c >= 32 && c < 127 && line_len < len - 1) {
            int i;
            for (i = line_len; i > pos; i--) buf[i] = buf[i - 1];
            buf[pos] = (char)c;
            line_len++;
            pos++;
            buf[line_len] = '\0';
            redraw_line(start_row, start_col, buf, line_len, pos, &prev_max_len);
        }
    }

    history_push(buf);
    return line_len;
}
