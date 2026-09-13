/* =============================================================================
 * SENG21213-OS :: PS/2 Keyboard Driver
 * File   : kernel/keyboard.h + keyboard.c
 * ============================================================================*/
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../include/types.h"

#define KB_BUF_SIZE 256

/* Special key codes for keys with no ASCII value, returned by
 * kb_getchar(). Chosen above 0xFF (max real char value) so they can
 * never collide with an actual translated key. */
#define KB_KEY_UP     0x100
#define KB_KEY_DOWN   0x101
#define KB_KEY_LEFT   0x102
#define KB_KEY_RIGHT  0x103

void kb_init(void);

/* Read one key (blocks until pressed). Returns an ASCII value (0-255)
 * for printable keys/Enter/Backspace, or one of the KB_KEY_* codes
 * above for arrow keys. */
int  kb_getchar(void);

/* Read a line into buf (up to len-1 chars), NUL-terminated. Supports
 * Left/Right to move within the line, Up/Down to browse a 20-entry
 * command history, and Backspace anywhere in the line (not just at
 * the end). Returns number of chars read. */
int  kb_readline(char *buf, int len);

#endif /* KEYBOARD_H */
