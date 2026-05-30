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
 * File: tui/tui.h
 * 
 * Description:
 *      Header file for the Terminal User Interface (TUI) library.
 *      Provides cross-platform APIs for rendering text-based 
 *      graphics and handling keyboard input.
 * 
 * This file is part of TCG.
 */

#ifndef TCG_TUI_H
#define TCG_TUI_H

#include <stdint.h>     /* For uint32_t */
#include <uchar.h>      /* For char32_t */

/* Forward declaration of the TUI context structure */
typedef struct TUIContext TUIContext;
/* 
 * Type definition for UI colors 
 * Format: 0xRRGGBB (Red, Green, Blue components in hexadecimal)
 */
typedef uint32_t TUIColor;

/* Type definition for key codes */
typedef enum
{
    TUI_KEY_UP, TUI_KEY_DOWN, TUI_KEY_LEFT, TUI_KEY_RIGHT,
    TUI_KEY_ENTER, TUI_KEY_ESCAPE, TUI_KEY_BACKSPACE, TUI_KEY_TAB,
    TUI_KEY_CAPS_LOCK, TUI_KEY_NUM_LOCK, TUI_KEY_SCROLL_LOCK,
    TUI_KEY_MINUS, TUI_KEY_EQUAL, TUI_KEY_LEFT_BRACKET, TUI_KEY_RIGHT_BRACKET,
    TUI_KEY_F1, TUI_KEY_F2, TUI_KEY_F3, TUI_KEY_F4,
    TUI_KEY_F5, TUI_KEY_F6, TUI_KEY_F7, TUI_KEY_F8,
    TUI_KEY_F9, TUI_KEY_F10, TUI_KEY_F11, TUI_KEY_F12,
    TUI_KEY_A, TUI_KEY_B, TUI_KEY_C, TUI_KEY_D, TUI_KEY_E,
    TUI_KEY_F, TUI_KEY_G, TUI_KEY_H, TUI_KEY_I, TUI_KEY_J,
    TUI_KEY_K, TUI_KEY_L, TUI_KEY_M, TUI_KEY_N, TUI_KEY_O,
    TUI_KEY_P, TUI_KEY_Q, TUI_KEY_R, TUI_KEY_S, TUI_KEY_T,
    TUI_KEY_U, TUI_KEY_V, TUI_KEY_W, TUI_KEY_X, TUI_KEY_Y,
    TUI_KEY_Z,
    TUI_KEY_0, TUI_KEY_1, TUI_KEY_2, TUI_KEY_3, TUI_KEY_4,
    TUI_KEY_5, TUI_KEY_6, TUI_KEY_7, TUI_KEY_8, TUI_KEY_9,
    TUI_KEY_UNKNOWN
} TUIKeyCode;

/* Type definition for key actions */
typedef enum
{
    TUI_KEY_STATE_PRESS,
    TUI_KEY_STATE_RELEASE,
    TUI_KEY_STATE_REPEAT
} TUIKeyAction;

/* Type definition for key modifiers */
typedef uint8_t TUIKeyMods;

/* Key modifier flags */
enum
{
    TUI_MOD_CTRL  = 1 << 0,
    TUI_MOD_ALT   = 1 << 1,
    TUI_MOD_SHIFT = 1 << 2
};

/* Type definition for key events */
typedef struct
{
    TUIKeyCode key;
    TUIKeyAction action;
    TUIKeyMods mods;
} TUIKeyEvent;

/* Type definition for key callback function */
typedef void (TUIKeyCallback)(TUIKeyEvent *event, void *user_data);

/* Type definition for a single UI cell */
typedef struct {
    char32_t ch;
    TUIColor bg;
    TUIColor fg;
    uint16_t flags;
} TUICell;

/* Cell flag definitions */
#define TUI_CELL_BOLD         (1 << 0)  /* Bold */
#define TUI_CELL_DIM          (1 << 1)  /* Dim */
#define TUI_CELL_ITALIC       (1 << 2)  /* Italic */
#define TUI_CELL_UNDERLINE    (1 << 3)  /* Underline */
#define TUI_CELL_BLINK        (1 << 4)  /* Blink */
#define TUI_CELL_REVERSE      (1 << 5)  /* Reverse */
#define TUI_CELL_INVISIBLE    (1 << 6)  /* Invisible */

/* Type definition for UI size */
typedef struct
{
    int width;
    int height;
} TUISize;

/*
 * Initializes the TUI context.
 * Returns a pointer to the initialized TUI context, or NULL on failure.
 */
TUIContext* tui_init(void);
/*
 * Shuts down the TUI context.
 * Frees all associated resources.
 */
void tui_shutdown(/* Not NULL */TUIContext *ctx);

/*
 * Puts a single cell at the specified coordinates with the given character and colors.
 */
void tui_put_cell(/* Not NULL */TUIContext *ctx, int x, int y, char32_t ch, TUIColor fg, TUIColor bg, uint16_t flags); /**/

/*
 * Clears the entire screen with the specified background color.
 */
void tui_clear(/* Not NULL */TUIContext *ctx, TUIColor bg);

/*
 * Presents the current back buffer to the screen, performing dirty checking and double buffering.
 */
void tui_present(/* Not NULL */TUIContext *ctx);

/*
 * Puts a null-terminated string starting at the specified coordinates with the given colors.
 * TODO: Add flag parameter for text attributes (bold, underline, etc.) and support for UTF-32 input strings.
 */
void tui_put_string(/* Not NULL */TUIContext *ctx, int x, int y, /* Not NULL */const char *str/* Must end with a null terminator */, TUIColor fg, TUIColor bg);

/*
 * Sets the cursor position.
 */
void tui_set_cursor(/* Not NULL */TUIContext *ctx, int x, int y);

/*
 * Hides the cursor.
 */
void tui_hide_cursor(/* Not NULL */TUIContext *ctx);

/*
 * Shows the cursor.
 */
void tui_show_cursor(/* Not NULL */TUIContext *ctx);

/*
 * Gets the size of the TUI.
 */
TUISize tui_get_size(/* Not NULL */TUIContext *ctx);

/*
 * Polls for events.
 */
void tui_poll_events(/* Not NULL */TUIContext *ctx);

/*
 * Registers a key callback function.
 */
void tui_register_key_callback(/* Not NULL */TUIContext *ctx, TUIKeyCallback callback, void *user_data);

/*
 * Sleeps for the specified number of milliseconds.
 */
void tui_sleep_ms(int milliseconds);

#endif /* TCG_TUI_H */
