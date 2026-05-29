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
 * File: tui/tui.c
 * 
 * Description:
 *      Implementation of the Terminal User Interface (TUI) library.
 * 
 * This file is part of TCG.
 */

#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Platform-specific implementations */
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#include "tui_posix.c"  /* POSIX-specific implementation */
#elif defined(_WIN32)
#include "tui_win32.c"  /* Windows-specific implementation */
#else
#error "Unsupported platform"
#endif

void tui_set_cursor(TUIContext *ctx, int x, int y) {
    if (!ctx) return;
    ctx->cursor_x = x;
    ctx->cursor_y = y;
}

void tui_hide_cursor(TUIContext *ctx) {
    if (!ctx) return;
    ctx->cursor_visible = 0;
}

TUISize tui_get_size(TUIContext *ctx) {
    TUISize size = {0, 0};
    if (ctx) {
        resize_buffer_if_needed(ctx);
        size.width = ctx->size.width;
        size.height = ctx->size.height;
    }
    return size;
}

void tui_put_cell(TUIContext *ctx, int x, int y, char32_t ch, TUIColor fg, TUIColor bg, uint16_t flags) {
    if (!ctx || !ctx->back_buffer) return;
    if (x >= 0 && x < ctx->size.width && y >= 0 && y < ctx->size.height) {
        int idx = y * ctx->size.width + x;
        ctx->back_buffer[idx].ch = ch;
        ctx->back_buffer[idx].fg = fg;
        ctx->back_buffer[idx].bg = bg;
        ctx->back_buffer[idx].flags = flags;
    }
}

void tui_clear(TUIContext *ctx, TUIColor bg) {
    if (!ctx || !ctx->back_buffer) return;
    int total = ctx->size.width * ctx->size.height;
    for (int i = 0; i < total; ++i) {
        ctx->back_buffer[i].ch = ' ';
        ctx->back_buffer[i].fg = 0xFFFFFF;
        ctx->back_buffer[i].bg = bg;
        ctx->back_buffer[i].flags = 0; /* No special flags */
    }
}

void tui_register_key_callback(TUIContext *ctx, TUIKeyCallback callback, void *user_data) {
    if (!ctx) return;
    ctx->callback = callback;
    ctx->user_data = user_data;
}

void tui_put_string(TUIContext *ctx, int x, int y, const char *str, TUIColor fg, TUIColor bg) {
    for (size_t i = 0; str[i] != '\0'; ++i) {
        tui_put_cell(ctx, x + (int)i, y, (char32_t)(unsigned char)str[i], fg, bg, 0);
    }
}