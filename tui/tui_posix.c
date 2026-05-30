/*
 * .___________.  ______   _______
 * |           | /      | /  _____|
 * `---|  |----`|  ,----'|  |  __
 *     |  |     |  |     |  | |_ |
 *     |  |     |  `----.|  |__| |
 *     |__|      \______| \______|
 * 
 * TCG - Terminal Chess Game
 * Copyright (c) 2026   Steve Pan
 * 
 * File: tui/tui_posix.c
 * 
 * Description:
 *      POSIX-specific implementation of the Terminal User Interface (TUI) library.
 * 
 * This file is part of TCG.
 */

#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

struct TUIContext {
    struct termios old_tio;
    /* Dynamic buffers for the TUI */
    TUICell *back_buffer;   /* Back buffer for drawing */
    TUICell *front_buffer;  /* Front buffer for dirty checking */
    TUISize size;           /* Current size of the console */
    TUIKeyCallback *callback;
    void *user_data;
    int cursor_x;
    int cursor_y;
    int cursor_visible;
};

/* Static helper function to encode a UTF-32 character to UTF-8 */
static int encode_utf32_to_utf8(char32_t cp, char *out) {
    if (cp <= 0x7F) {
        out[0] = (char)cp;
        return 1;
    } else if (cp <= 0x7FF) {
        out[0] = (char)(0xC0 | ((cp >> 6) & 0x1F));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp <= 0xFFFF) {
        out[0] = (char)(0xE0 | ((cp >> 12) & 0x0F));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else if (cp <= 0x10FFFF) {
        out[0] = (char)(0xF0 | ((cp >> 18) & 0x07));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    out[0] = '?';
    return 1;
}

/* Static helper function to check and handle window size adjustments */
static void resize_buffer_if_needed(TUIContext *ctx) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        int new_w = w.ws_col;
        int new_h = w.ws_row;
        if (new_w != ctx->size.width || new_h != ctx->size.height) {
            TUICell *new_back = (TUICell *)calloc(new_w * new_h, sizeof(TUICell));
            TUICell *new_front = (TUICell *)calloc(new_w * new_h, sizeof(TUICell));
            if (new_back && new_front) {
                free(ctx->back_buffer);
                free(ctx->front_buffer);
                ctx->back_buffer = new_back;
                ctx->front_buffer = new_front;
                ctx->size.width = new_w;
                ctx->size.height = new_h;
            } else {
                free(new_back);
                free(new_front);
            }
        }
    }
}

TUIContext* tui_init(void) {
    TUIContext *ctx = (TUIContext *)malloc(sizeof(TUIContext));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(TUIContext));

    /* Save and set the original terminal mode (Raw Mode), disable echo and buffering */
    tcgetattr(STDIN_FILENO, &ctx->old_tio);
    struct termios new_tio = ctx->old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO | ISIG);
    new_tio.c_cc[VMIN] = 0;  /* Non-blocking read */
    new_tio.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    /* Switch to the terminal's alternate screen buffer (\x1b[?1049h), avoiding pollution of the user's shell history scrollback */
    printf("\x1b[?1049h\x1b[?25l");
    fflush(stdout);

    ctx->cursor_visible = 1;
    resize_buffer_if_needed(ctx);

    return ctx;
}

void tui_shutdown(TUIContext *ctx) {
    if (!ctx) return;
    printf("\x1b[?25h\x1b[?1049l");
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSANOW, &ctx->old_tio);

    if (ctx->back_buffer) free(ctx->back_buffer);
    if (ctx->front_buffer) free(ctx->front_buffer);
    free(ctx);
}

void tui_present(TUIContext *ctx) {
    if (!ctx || !ctx->back_buffer || !ctx->front_buffer) return;

    resize_buffer_if_needed(ctx);

    size_t capacity = ctx->size.width * ctx->size.height * 64 + 128;
    char *sb = (char *)malloc(capacity);
    if (!sb) return;

    size_t len = 0;
    TUIColor last_fg = 0xFFFFFFFF;
    TUIColor last_bg = 0xFFFFFFFF;
    int first_color = 1;

    /* Track the current position of the terminal's physical cursor (-1 means unknown, must be positioned first) */
    int curr_out_x = -1;
    int curr_out_y = -1;

    for (int y = 0; y < ctx->size.height; ++y) {
        for (int x = 0; x < ctx->size.width; ++x) {
            int idx = y * ctx->size.width + x;
            TUICell back = ctx->back_buffer[idx];
            TUICell front = ctx->front_buffer[idx];

            /* Dirty check */
            if (back.ch != front.ch || back.fg != front.fg || back.bg != front.bg) {
                
                /* Optimization: Only send cursor positioning command if the physical cursor is not at the current cell */
                if (curr_out_x != x || curr_out_y != y) {
                    int n = snprintf(sb + len, capacity - len, "\x1b[%d;%dH", y + 1, x + 1);
                    if (n > 0) len += n;
                }

                /* Dynamic color merging */
                if (first_color || back.fg != last_fg) {
                    uint8_t r = (back.fg >> 16) & 0xFF;
                    uint8_t g = (back.fg >> 8) & 0xFF;
                    uint8_t b = back.fg & 0xFF;
                    int n = snprintf(sb + len, capacity - len, "\x1b[38;2;%d;%d;%dm", r, g, b);
                    if (n > 0) len += n;
                    last_fg = back.fg;
                }
                if (first_color || back.bg != last_bg) {
                    uint8_t r = (back.bg >> 16) & 0xFF;
                    uint8_t g = (back.bg >> 8) & 0xFF;
                    uint8_t b = back.bg & 0xFF;
                    int n = snprintf(sb + len, capacity - len, "\x1b[48;2;%d;%d;%dm", r, g, b);
                    if (n > 0) len += n;
                    last_bg = back.bg;
                }
                first_color = 0;

                /* Write the character */
                char utfs[5];
                int n_utf8 = encode_utf32_to_utf8(back.ch, utfs);
                for (int i = 0; i < n_utf8; ++i) {
                    if (len < capacity - 1) sb[len++] = utfs[i];
                }

                /* Core logic: After writing one cell, the terminal's physical cursor automatically moves one position to the right */
                curr_out_x = x + 1;
                curr_out_y = y;
            }
        }
    }

    /* Restore the user's set logical cursor position and visibility */
    if (ctx->cursor_visible) {
        int n = snprintf(sb + len, capacity - len, "\x1b[%d;%dH\x1b[?25h", ctx->cursor_y + 1, ctx->cursor_x + 1);
        if (n > 0) len += n;
    } else {
        int n = snprintf(sb + len, capacity - len, "\x1b[?25l");
        if (n > 0) len += n;
    }

    /* If the entire screen has not changed, len will be very small (containing only cursor commands), or even 0 */
    if (len > 0) {
        write(STDOUT_FILENO, sb, len);
    }
    free(sb);

    /* Synchronize the double buffers */
    memcpy(ctx->front_buffer, ctx->back_buffer, ctx->size.width * ctx->size.height * sizeof(TUICell));
}

void tui_poll_events(TUIContext *ctx) {
    if (!ctx || !ctx->callback) return;

    resize_buffer_if_needed(ctx);

    unsigned char buf[16];
    /* Read input stream data */
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return;

    ssize_t i = 0;
    while (i < n) {
        TUIKeyCode code = TUI_KEY_UNKNOWN;
        TUIKeyMods mods = 0;
        TUIKeyAction action = TUI_KEY_STATE_PRESS; /* Default to PRESS, will adjust to RELEASE if we detect a key release event */

        if (buf[i] == 0x1b) { /* Escape character, start of an escape sequence or Alt + key combo */
            if (i + 1 >= n) {
                code = TUI_KEY_ESCAPE;
                i += 1;
            } else if (buf[i + 1] == '[') {
                if (i + 2 >= n) { i += 2; continue; }
                
                /* 1. Map standard direction keys */
                if (buf[i + 2] == 'A') { code = TUI_KEY_UP; i += 3; }
                else if (buf[i + 2] == 'B') { code = TUI_KEY_DOWN; i += 3; }
                else if (buf[i + 2] == 'C') { code = TUI_KEY_RIGHT; i += 3; }
                else if (buf[i + 2] == 'D') { code = TUI_KEY_LEFT; i += 3; }
                else if (buf[i + 2] == 'Z') { code = TUI_KEY_TAB; mods |= TUI_MOD_SHIFT; i += 3; } // Shift + Tab
                else {
                    /* Parse complex escape sequences, e.g., F keys or direction keys with modifiers */
                    ssize_t j = i + 2;
                    while (j < n && buf[j] != '~' && !(buf[j] >= 'A' && buf[j] <= 'Z') && !(buf[j] >= 'a' && buf[j] <= 'z')) {
                        j++;
                    }
                    if (j < n) {
                        if (buf[j] == '~') { /* Sequence ending with tilde (e.g., F1-F12) */
                            int num = 0;
                            for (ssize_t k = i + 2; k < j; k++) {
                                if (buf[k] >= '0' && buf[k] <= '9') num = num * 10 + (buf[k] - '0');
                            }
                            if (num >= 11 && num <= 14) code = (TUIKeyCode)(TUI_KEY_F1 + (num - 11));
                            else if (num == 15) code = TUI_KEY_F5;
                            else if (num >= 17 && num <= 21) code = (TUIKeyCode)(TUI_KEY_F6 + (num - 17));
                            else if (num >= 23 && num <= 24) code = (TUIKeyCode)(TUI_KEY_F11 + (num - 23));
                        } else if (buf[j] >= 'A' && buf[j] <= 'D') {
                            /* Direction keys with modifiers, e.g., \x1b[1;2A for Shift + Up */
                            if (buf[i + 2] == '1' && buf[i + 3] == ';') {
                                char mod_char = buf[i + 4];
                                if (mod_char == '2') mods |= TUI_MOD_SHIFT;
                                if (mod_char == '3') mods |= TUI_MOD_ALT;
                                if (mod_char == '5') mods |= TUI_MOD_CTRL;
                                
                                if (buf[j] == 'A') code = TUI_KEY_UP;
                                if (buf[j] == 'B') code = TUI_KEY_DOWN;
                                if (buf[j] == 'C') code = TUI_KEY_RIGHT;
                                if (buf[j] == 'D') code = TUI_KEY_LEFT;
                            }
                        }
                        i = j + 1;
                    } else {
                        i += 2;
                    }
                }
            } else if (buf[i + 1] == 'O') { /* VT100-compatible F1-F4 (\x1bOP - \x1bOS) */
                if (i + 2 >= n) { i += 2; continue; }
                char f = buf[i + 2];
                if (f >= 'P' && f <= 'S') code = (TUIKeyCode)(TUI_KEY_F1 + (f - 'P'));
                i += 3;
            } else {
                /* Combination key: Alt + regular key */
                mods |= TUI_MOD_ALT;
                unsigned char next_ch = buf[i + 1];
                if (next_ch >= 'a' && next_ch <= 'z') code = (TUIKeyCode)(TUI_KEY_A + (next_ch - 'a'));
                else if (next_ch >= 'A' && next_ch <= 'Z') { code = (TUIKeyCode)(TUI_KEY_A + (next_ch - 'A')); mods |= TUI_MOD_SHIFT; }
                else if (next_ch >= '0' && next_ch <= '9') code = (TUIKeyCode)(TUI_KEY_0 + (next_ch - '0'));
                i += 2;
            }
        } else {
            /* 2. Single-byte ASCII mapping */
            unsigned char ch = buf[i];
            if (ch == '\n' || ch == '\r') {
                code = TUI_KEY_ENTER;
            } else if (ch == '\t') {
                code = TUI_KEY_TAB;
            } else if (ch == 0x7f || ch == 0x08) {
                code = TUI_KEY_BACKSPACE;
            } else if (ch >= 1 && ch <= 26) {
                /* Ctrl + A to Ctrl + Z (excluding already special-handled Tab/Enter) */
                if (ch != 9 && ch != 10 && ch != 13) {
                    code = (TUIKeyCode)(TUI_KEY_A + (ch - 1));
                    mods |= TUI_MOD_CTRL;
                }
            } else if (ch >= 'a' && ch <= 'z') {
                code = (TUIKeyCode)(TUI_KEY_A + (ch - 'a'));
            } else if (ch >= 'A' && ch <= 'Z') {
                code = (TUIKeyCode)(TUI_KEY_A + (ch - 'A'));
                mods |= TUI_MOD_SHIFT;
            } else if (ch >= '0' && ch <= '9') {
                code = (TUIKeyCode)(TUI_KEY_0 + (ch - '0'));
            } else if (ch == '-') {
                code = TUI_KEY_MINUS;
            }
            i += 1;
        }

        /* Emit the registered key event */
        if (code != TUI_KEY_UNKNOWN) {
            TUIKeyEvent event = { .key = code, .action = action, .mods = mods };
            ctx->callback(&event, ctx->user_data);
        }
    }
}

void tui_sleep_ms(int milliseconds) {
    usleep(milliseconds * 1000);
}