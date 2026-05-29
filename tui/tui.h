#ifndef TCG_TUI_H
#define TCG_TUI_H

#include <stdint.h>
#include <uchar.h>

typedef struct TUIContext TUIContext;
typedef uint32_t TUIColor;

typedef enum
{
    TUI_KEY_UP, TUI_KEY_DOWN, TUI_KEY_LEFT, TUI_KEY_RIGHT,
    TUI_KEY_ENTER, TUI_KEY_ESCAPE, TUI_KEY_BACKSPACE, TUI_KEY_TAB,
    TUI_KEY_CAPS_LOCK, TUI_KEY_NUM_LOCK, TUI_KEY_SCROLL_LOCK,
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

typedef enum
{
    TUI_KEY_STATE_PRESS,
    TUI_KEY_STATE_RELEASE,
    TUI_KEY_STATE_REPEAT
} TUIKeyAction;

typedef uint8_t TUIKeyMods;

enum
{
    TUI_MOD_CTRL  = 1 << 0,
    TUI_MOD_ALT   = 1 << 1,
    TUI_MOD_SHIFT = 1 << 2
};

typedef struct
{
    TUIKeyCode key;
    TUIKeyAction action;
    TUIKeyMods mods;
} TUIKeyEvent;

typedef void (TUIKeyCallback)(TUIKeyEvent *event, void *user_data);

typedef struct {
    char32_t ch;
    TUIColor bg;
    TUIColor fg;
    uint16_t flags;
} TUICell;

typedef struct
{
    int width;
    int height;
} TUISize;

TUIContext* tui_init(void);
void tui_shutdown(TUIContext *ctx);

void tui_put_cell(TUIContext *ctx, int x, int y, char32_t ch, TUIColor fg, TUIColor bg);
void tui_clear(TUIContext *ctx, TUIColor bg);
void tui_present(TUIContext *ctx);

void tui_put_string(TUIContext *ctx, int x, int y, const char *str, TUIColor fg, TUIColor bg);

void tui_set_cursor(TUIContext *ctx, int x, int y);
void tui_hide_cursor(TUIContext *ctx);

TUISize tui_get_size(TUIContext *ctx);

void tui_poll_events(TUIContext *ctx);

void tui_register_key_callback(TUIContext *ctx, TUIKeyCallback callback, void *user_data);

void tui_sleep_ms(int milliseconds);

#endif /* TCG_TUI_H */