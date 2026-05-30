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
 * File: tui/tui_win32.c
 * 
 * Description:
 *      Windows-specific implementation of the Terminal User Interface (TUI) library.
 * 
 * This file is part of TCG.
 */

#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN /* Exclude rarely-used Windows headers */
#include <windows.h>

/* Structure for the TUI context */
struct TUIContext {
    HANDLE hIn;             /* Console input handle */
    HANDLE hOut;            /* Console output handle */
    DWORD oldInMode;        /* Original console input mode (for restoration) */
    DWORD oldOutMode;       /* Original console output mode (for restoration) */
    /* Dynamic buffers for the TUI */
    TUICell *back_buffer;   /* Back buffer for drawing */
    TUICell *front_buffer;  /* Front buffer for dirty checking */
    TUISize size;           /* Current size of the console */
    TUIKeyCallback *callback;
    void *user_data;
    int cursor_x;
    int cursor_y;
    int cursor_visible;
    BOOL key_states[256];
};

/* Static function to encode a UTF-32 character to UTF-16 */
static int encode_utf32_to_utf16(char32_t cp, wchar_t *out) {
    if (cp <= 0xFFFF) {
        out[0] = (wchar_t)cp;
        return 1;
    } else if (cp <= 0x10FFFF) {
        cp -= 0x10000;
        out[0] = (wchar_t)((cp >> 10) + 0xD800);
        out[1] = (wchar_t)((cp & 0x3FF) + 0xDC00);
        return 2;
    }
    out[0] = L'?';
    return 1;
}

/* Static function to resize the buffers if the console size has changed */
static void resize_buffer_if_needed(TUIContext *ctx) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(ctx->hOut, &csbi)) {
        int new_w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        int new_h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
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

    ctx->hIn = GetStdHandle(STD_INPUT_HANDLE);
    ctx->hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    GetConsoleMode(ctx->hIn, &ctx->oldInMode);
    GetConsoleMode(ctx->hOut, &ctx->oldOutMode);

    /* Set the console mode to enable virtual terminal processing (VT100 / RGB Colors) */
    DWORD outMode = ctx->oldOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(ctx->hOut, outMode);

    /* Set the console mode to enable raw keyboard input capture */
    DWORD inMode = ctx->oldInMode;
    inMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_MOUSE_INPUT);
    inMode |= ENABLE_WINDOW_INPUT;
    SetConsoleMode(ctx->hIn, inMode);

    /* Get the initial console window size */
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(ctx->hOut, &csbi)) {
        ctx->size.width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        ctx->size.height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        ctx->size.width = 80;
        ctx->size.height = 25;
    }

    ctx->back_buffer = (TUICell *)calloc(ctx->size.width * ctx->size.height, sizeof(TUICell));
    ctx->front_buffer = (TUICell *)calloc(ctx->size.width * ctx->size.height, sizeof(TUICell));
    ctx->cursor_visible = 1;

    return ctx;
}

void tui_shutdown(TUIContext *ctx) {
    if (!ctx) return;
    DWORD written;
    WriteConsoleW(ctx->hOut, L"\x1b[?25h", 6, &written, NULL);
    SetConsoleMode(ctx->hIn, ctx->oldInMode);
    SetConsoleMode(ctx->hOut, ctx->oldOutMode);

    if (ctx->back_buffer) free(ctx->back_buffer);
    if (ctx->front_buffer) free(ctx->front_buffer);
    free(ctx);
}

void tui_present(TUIContext *ctx) {
    if (!ctx || !ctx->back_buffer || !ctx->front_buffer) return;

    resize_buffer_if_needed(ctx);

    /* Allocate a buffer for the output string */
    size_t capacity = ctx->size.width * ctx->size.height * 64/* UTF-16 characters per cell */ + 128/* Additional space for escape sequences */;
    wchar_t *sb = (wchar_t *)malloc(capacity * sizeof(wchar_t));
    if (!sb) return;

    size_t len = 0;
    TUIColor last_fg = 0xFFFFFFFF; 
    TUIColor last_bg = 0xFFFFFFFF;
    BOOL first_color = TRUE;

    int curr_out_x = -1;
    int curr_out_y = -1;

    for (int y = 0; y < ctx->size.height; ++y) {
        for (int x = 0; x < ctx->size.width; ++x) {
            int idx = y * ctx->size.width + x;
            TUICell back = ctx->back_buffer[idx];
            TUICell front = ctx->front_buffer[idx];

            /* Dirty check */
            if (back.ch != front.ch || back.fg != front.fg || back.bg != front.bg) {
                
                /* Move cursor if necessary */
                if (curr_out_x != x || curr_out_y != y) {
                    int n = swprintf(sb + len, capacity - len, L"\x1b[%d;%dH", y + 1, x + 1);
                    if (n > 0) len += n;
                }

                if (first_color || back.fg != last_fg) {
                    uint8_t r = (back.fg >> 16) & 0xFF;
                    uint8_t g = (back.fg >> 8) & 0xFF;
                    uint8_t b = back.fg & 0xFF;
                    int n = swprintf(sb + len, capacity - len, L"\x1b[38;2;%d;%d;%dm", r, g, b);
                    if (n > 0) len += n;
                    last_fg = back.fg;
                }
                if (first_color || back.bg != last_bg) {
                    uint8_t r = (back.bg >> 16) & 0xFF;
                    uint8_t g = (back.bg >> 8) & 0xFF;
                    uint8_t b = back.bg & 0xFF;
                    int n = swprintf(sb + len, capacity - len, L"\x1b[48;2;%d;%d;%dm", r, g, b);
                    if (n > 0) len += n;
                    last_bg = back.bg;
                }
                first_color = FALSE;

                /* Write the UTF-16 character */
                wchar_t wch[2];
                int n_utf16 = encode_utf32_to_utf16(back.ch, wch);
                for (int i = 0; i < n_utf16; ++i) {
                    if (len < capacity - 1) sb[len++] = wch[i];
                }

                curr_out_x = x + 1;
                curr_out_y = y;
            }
        }
    }

    if (ctx->cursor_visible) {
        int n = swprintf(sb + len, capacity - len, L"\x1b[%d;%dH\x1b[?25h", ctx->cursor_y + 1, ctx->cursor_x + 1);
        if (n > 0) len += n;
    } else {
        int n = swprintf(sb + len, capacity - len, L"\x1b[?25l");
        if (n > 0) len += n;
    }

    if (len > 0) {
        DWORD written;
        WriteConsoleW(ctx->hOut, sb, (DWORD)len, &written, NULL);
    }
    free(sb);

    /* Synchronize the double buffers */
    memcpy(ctx->front_buffer, ctx->back_buffer, ctx->size.width * ctx->size.height * sizeof(TUICell));
}

void tui_poll_events(TUIContext *ctx) {
    if (!ctx) return;

    DWORD numEvents = 0;
    if (!GetNumberOfConsoleInputEvents(ctx->hIn, &numEvents) || numEvents == 0) {
        return;
    }

    INPUT_RECORD *eventBuffer = (INPUT_RECORD *)malloc(numEvents * sizeof(INPUT_RECORD));
    if (!eventBuffer) return;

    DWORD eventsRead = 0;
    if (ReadConsoleInputW(ctx->hIn, eventBuffer, numEvents, &eventsRead)) {
        for (DWORD i = 0; i < eventsRead; ++i) {
            if (eventBuffer[i].EventType == WINDOW_BUFFER_SIZE_EVENT) {
                resize_buffer_if_needed(ctx);
            } else if (eventBuffer[i].EventType == KEY_EVENT) {
                KEY_EVENT_RECORD ker = eventBuffer[i].Event.KeyEvent;
                TUIKeyCode code = TUI_KEY_UNKNOWN;
                WORD vk = ker.wVirtualKeyCode;

                /* Map virtual key codes to unified TUI key codes */
                if (vk >= 'A' && vk <= 'Z') {
                    code = (TUIKeyCode)(TUI_KEY_A + (vk - 'A'));
                } else if (vk >= '0' && vk <= '9') {
                    code = (TUIKeyCode)(TUI_KEY_0 + (vk - '0'));
                } else if (vk >= VK_F1 && vk <= VK_F12) {
                    code = (TUIKeyCode)(TUI_KEY_F1 + (vk - VK_F1));
                } else {
                    switch (vk) {
                        case VK_UP:        code = TUI_KEY_UP; break;
                        case VK_DOWN:      code = TUI_KEY_DOWN; break;
                        case VK_LEFT:      code = TUI_KEY_LEFT; break;
                        case VK_RIGHT:     code = TUI_KEY_RIGHT; break;
                        case VK_RETURN:    code = TUI_KEY_ENTER; break;
                        case VK_ESCAPE:    code = TUI_KEY_ESCAPE; break;
                        case VK_BACK:      code = TUI_KEY_BACKSPACE; break;
                        case VK_TAB:       code = TUI_KEY_TAB; break;
                        case VK_CAPITAL:   code = TUI_KEY_CAPS_LOCK; break;
                        case VK_NUMLOCK:   code = TUI_KEY_NUM_LOCK; break;
                        case VK_SCROLL:    code = TUI_KEY_SCROLL_LOCK; break;
                        case VK_OEM_MINUS: code = TUI_KEY_MINUS; break;
                        default:           code = TUI_KEY_UNKNOWN; break;
                    }
                }

                /* Determine key action (PRESS / RELEASE / REPEAT) */
                TUIKeyAction action;
                if (ker.bKeyDown) {
                    if (vk < 256 && ctx->key_states[vk]) {
                        action = TUI_KEY_STATE_REPEAT;
                    } else {
                        action = TUI_KEY_STATE_PRESS;
                        if (vk < 256) ctx->key_states[vk] = TRUE;
                    }
                } else {
                    action = TUI_KEY_STATE_RELEASE;
                    if (vk < 256) ctx->key_states[vk] = FALSE;
                }

                /* Map control key states to unified TUI key mods */
                TUIKeyMods mods = 0;
                if (ker.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
                    mods |= TUI_MOD_CTRL;
                }
                if (ker.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
                    mods |= TUI_MOD_ALT;
                }
                if (ker.dwControlKeyState & SHIFT_PRESSED) {
                    mods |= TUI_MOD_SHIFT;
                }

                /* Execute registered callback */
                if (ctx->callback) {
                    TUIKeyEvent e;
                    e.key = code;
                    e.action = action;
                    e.mods = mods;
                    ctx->callback(&e, ctx->user_data);
                }
            }
        }
    }
    free(eventBuffer);
}

void tui_sleep_ms(int milliseconds) {
    Sleep(milliseconds);
}