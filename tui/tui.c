#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(__unix__) || defined(__APPLE__)
struct TUIContext {
    struct termios old_tio;
    TUICell *back_buffer;   // 后台缓冲区：供应用层绘制 (tui_put_cell)
    TUICell *front_buffer;  // 前台缓冲区：记录当前屏幕上已有的画面
    int width;
    int height;
    TUIKeyCallback *callback;
    void *user_data;
    int cursor_x;
    int cursor_y;
    int cursor_visible;
};

// UTF-32 字符转换为 POSIX 终端通用的 UTF-8 编码助手
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

// 检查并处理窗口大小调整的内部助手
static void resize_buffer_if_needed(TUIContext *ctx) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        int new_w = w.ws_col;
        int new_h = w.ws_row;
        if (new_w != ctx->width || new_h != ctx->height) {
            TUICell *new_back = (TUICell *)calloc(new_w * new_h, sizeof(TUICell));
            TUICell *new_front = (TUICell *)calloc(new_w * new_h, sizeof(TUICell));
            if (new_back && new_front) {
                free(ctx->back_buffer);
                free(ctx->front_buffer);
                ctx->back_buffer = new_back;
                ctx->front_buffer = new_front;
                ctx->width = new_w;
                ctx->height = new_h;
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

    // 保存并设置原始模式 (Raw Mode)，关闭回显和缓冲输入
    tcgetattr(STDIN_FILENO, &ctx->old_tio);
    struct termios new_tio = ctx->old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO | ISIG);
    new_tio.c_cc[VMIN] = 0;  // 非阻塞读取
    new_tio.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    // 切换到终端备用屏幕缓冲区 (\x1b[?1049h)，避免污染用户的 shell 历史滚动条
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

    size_t capacity = ctx->width * ctx->height * 64 + 128;
    char *sb = (char *)malloc(capacity);
    if (!sb) return;

    size_t len = 0;
    TUIColor last_fg = 0xFFFFFFFF;
    TUIColor last_bg = 0xFFFFFFFF;
    int first_color = 1;

    // 追踪终端物理光标的当前位置 (-1 代表未知，必须先定位)
    int curr_out_x = -1;
    int curr_out_y = -1;

    for (int y = 0; y < ctx->height; ++y) {
        for (int x = 0; x < ctx->width; ++x) {
            int idx = y * ctx->width + x;
            TUICell back = ctx->back_buffer[idx];
            TUICell front = ctx->front_buffer[idx];

            // 【脏检查】对比后台和前台，完全一致则直接跳过
            if (back.ch != front.ch || back.fg != front.fg || back.bg != front.bg) {
                
                // 优化：只有当物理光标不在当前正要画的格子时，才发送光标重定位指令
                if (curr_out_x != x || curr_out_y != y) {
                    int n = snprintf(sb + len, capacity - len, "\x1b[%d;%dH", y + 1, x + 1);
                    if (n > 0) len += n;
                }

                // 动态色彩合并
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

                // 写入字符
                char utfs[5];
                int n_utf8 = encode_utf32_to_utf8(back.ch, utfs);
                for (int i = 0; i < n_utf8; ++i) {
                    if (len < capacity - 1) sb[len++] = utfs[i];
                }

                // 核心逻辑：写入 1 个单元格后，终端物理光标会自动向右移动 1 格
                curr_out_x = x + 1;
                curr_out_y = y;
            }
        }
    }

    // 恢复用户设置的逻辑光标位置与可见性
    if (ctx->cursor_visible) {
        int n = snprintf(sb + len, capacity - len, "\x1b[%d;%dH\x1b[?25h", ctx->cursor_y + 1, ctx->cursor_x + 1);
        if (n > 0) len += n;
    } else {
        int n = snprintf(sb + len, capacity - len, "\x1b[?25l");
        if (n > 0) len += n;
    }

    // 如果整个屏幕没有发生任何实质改变，len 会非常小（仅含光标指令），甚至为 0
    if (len > 0) {
        write(STDOUT_FILENO, sb, len);
    }
    free(sb);

    // 【同步缓冲区】将当前绘制完的后台同步至前台，供下一帧做差量对比
    memcpy(ctx->front_buffer, ctx->back_buffer, ctx->width * ctx->height * sizeof(TUICell));
}

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
        size.width = ctx->width;
        size.height = ctx->height;
    }
    return size;
}

void tui_poll_events(TUIContext *ctx) {
    if (!ctx || !ctx->callback) return;

    resize_buffer_if_needed(ctx);

    unsigned char buf[16];
    // 读取输入流数据
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return;

    ssize_t i = 0;
    while (i < n) {
        TUIKeyCode code = TUI_KEY_UNKNOWN;
        TUIKeyMods mods = 0;
        TUIKeyAction action = TUI_KEY_STATE_PRESS; // 默认置为按下

        if (buf[i] == 0x1b) { // 检测到 Escape 引导序列
            if (i + 1 >= n) {
                code = TUI_KEY_ESCAPE;
                i += 1;
            } else if (buf[i + 1] == '[') {
                if (i + 2 >= n) { i += 2; continue; }
                
                // 1. 标准方向键映射
                if (buf[i + 2] == 'A') { code = TUI_KEY_UP; i += 3; }
                else if (buf[i + 2] == 'B') { code = TUI_KEY_DOWN; i += 3; }
                else if (buf[i + 2] == 'C') { code = TUI_KEY_RIGHT; i += 3; }
                else if (buf[i + 2] == 'D') { code = TUI_KEY_LEFT; i += 3; }
                else if (buf[i + 2] == 'Z') { code = TUI_KEY_TAB; mods |= TUI_MOD_SHIFT; i += 3; } // Shift + Tab
                else {
                    // 解析复杂的转义序列，例如 F 键或带修饰键的方向键
                    ssize_t j = i + 2;
                    while (j < n && buf[j] != '~' && !(buf[j] >= 'A' && buf[j] <= 'Z') && !(buf[j] >= 'a' && buf[j] <= 'z')) {
                        j++;
                    }
                    if (j < n) {
                        if (buf[j] == '~') { // 基于波浪号结尾的序列 (如 F1-F12)
                            int num = 0;
                            for (ssize_t k = i + 2; k < j; k++) {
                                if (buf[k] >= '0' && buf[k] <= '9') num = num * 10 + (buf[k] - '0');
                            }
                            if (num >= 11 && num <= 14) code = (TUIKeyCode)(TUI_KEY_F1 + (num - 11));
                            else if (num == 15) code = TUI_KEY_F5;
                            else if (num >= 17 && num <= 21) code = (TUIKeyCode)(TUI_KEY_F6 + (num - 17));
                            else if (num >= 23 && num <= 24) code = (TUIKeyCode)(TUI_KEY_F11 + (num - 23));
                        } else if (buf[j] >= 'A' && buf[j] <= 'D') {
                            // 解析类似 \x1b[1;5A 这种带修饰键的格式 (5 代表 Ctrl)
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
            } else if (buf[i + 1] == 'O') { // vt100 兼容模式下的 F1-F4 (\x1bOP - \x1bOS)
                if (i + 2 >= n) { i += 2; continue; }
                char f = buf[i + 2];
                if (f >= 'P' && f <= 'S') code = (TUIKeyCode)(TUI_KEY_F1 + (f - 'P'));
                i += 3;
            } else {
                // 组合键：Alt + 普通键
                mods |= TUI_MOD_ALT;
                unsigned char next_ch = buf[i + 1];
                if (next_ch >= 'a' && next_ch <= 'z') code = (TUIKeyCode)(TUI_KEY_A + (next_ch - 'a'));
                else if (next_ch >= 'A' && next_ch <= 'Z') { code = (TUIKeyCode)(TUI_KEY_A + (next_ch - 'A')); mods |= TUI_MOD_SHIFT; }
                else if (next_ch >= '0' && next_ch <= '9') code = (TUIKeyCode)(TUI_KEY_0 + (next_ch - '0'));
                i += 2;
            }
        } else {
            // 2. 单字节普通 ASCII 映射
            unsigned char ch = buf[i];
            if (ch == '\n' || ch == '\r') {
                code = TUI_KEY_ENTER;
            } else if (ch == '\t') {
                code = TUI_KEY_TAB;
            } else if (ch == 0x7f || ch == 0x08) {
                code = TUI_KEY_BACKSPACE;
            } else if (ch >= 1 && ch <= 26) {
                // Ctrl + A 到 Ctrl + Z (排除已经特殊处理的 Tab/Enter)
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
            }
            i += 1;
        }

        // 激发注册的按键事件
        if (code != TUI_KEY_UNKNOWN) {
            TUIKeyEvent event = { .key = code, .action = action, .mods = mods };
            ctx->callback(&event, ctx->user_data);
        }
    }
}

#elif defined(_WIN32)
struct TUIContext {
    HANDLE hIn;
    HANDLE hOut;
    DWORD oldInMode;
    DWORD oldOutMode;
    TUICell *back_buffer;   // 后台
    TUICell *front_buffer;  // 前台
    int width;
    int height;
    TUIKeyCallback *callback;
    void *user_data;
    int cursor_x;
    int cursor_y;
    int cursor_visible;
    BOOL key_states[256];
};

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

static void resize_buffer_if_needed(TUIContext *ctx) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(ctx->hOut, &csbi)) {
        int new_w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        int new_h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        if (new_w != ctx->width || new_h != ctx->height) {
            TUICell *new_back = (TUICell *)calloc(new_w * new_h, sizeof(TUICell));
            TUICell *new_front = (TUICell *)calloc(new_w * new_h, sizeof(TUICell));
            if (new_back && new_front) {
                free(ctx->back_buffer);
                free(ctx->front_buffer);
                ctx->back_buffer = new_back;
                ctx->front_buffer = new_front;
                ctx->width = new_w;
                ctx->height = new_h;
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

    // 启用 Windows 虚拟终端序列支持 (VT100 / RGB 颜色)
    DWORD outMode = ctx->oldOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(ctx->hOut, outMode);

    // 启用无阻塞的原始键盘输入捕获
    DWORD inMode = ctx->oldInMode;
    inMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_MOUSE_INPUT);
    inMode |= ENABLE_WINDOW_INPUT;
    SetConsoleMode(ctx->hIn, inMode);

    // 获取初始控制台区域大小
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(ctx->hOut, &csbi)) {
        ctx->width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        ctx->height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        ctx->width = 80;
        ctx->height = 25;
    }

    ctx->back_buffer = (TUICell *)calloc(ctx->width * ctx->height, sizeof(TUICell));
    ctx->front_buffer = (TUICell *)calloc(ctx->width * ctx->height, sizeof(TUICell));
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

    size_t capacity = ctx->width * ctx->height * 64 + 128;
    wchar_t *sb = (wchar_t *)malloc(capacity * sizeof(wchar_t));
    if (!sb) return;

    size_t len = 0;
    TUIColor last_fg = 0xFFFFFFFF; 
    TUIColor last_bg = 0xFFFFFFFF;
    BOOL first_color = TRUE;

    int curr_out_x = -1;
    int curr_out_y = -1;

    for (int y = 0; y < ctx->height; ++y) {
        for (int x = 0; x < ctx->width; ++x) {
            int idx = y * ctx->width + x;
            TUICell back = ctx->back_buffer[idx];
            TUICell front = ctx->front_buffer[idx];

            // 【脏检查】
            if (back.ch != front.ch || back.fg != front.fg || back.bg != front.bg) {
                
                // 光标跳跃优化
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

                // 写入 UTF-16 字符
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

    // 同步双缓冲区
    memcpy(ctx->front_buffer, ctx->back_buffer, ctx->width * ctx->height * sizeof(TUICell));
}

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
        size.width = ctx->width;
        size.height = ctx->height;
    }
    return size;
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

                // 1. 映射虚拟键码到统一的 TUIKeyCode
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
                        default:           code = TUI_KEY_UNKNOWN; break;
                    }
                }

                // 2. 判定按键状态动作 (PRESS / RELEASE / REPEAT)
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

                // 3. 映射修饰键状态 (CTRL, ALT, SHIFT)
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

                // 4. 执行注册的回调
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

#endif

void tui_put_cell(TUIContext *ctx, int x, int y, char32_t ch, TUIColor fg, TUIColor bg) {
    if (!ctx || !ctx->back_buffer) return;
    if (x >= 0 && x < ctx->width && y >= 0 && y < ctx->height) {
        int idx = y * ctx->width + x;
        ctx->back_buffer[idx].ch = ch;
        ctx->back_buffer[idx].fg = fg;
        ctx->back_buffer[idx].bg = bg;
        ctx->back_buffer[idx].flags = 0;
    }
}

void tui_clear(TUIContext *ctx, TUIColor bg) {
    if (!ctx || !ctx->back_buffer) return;
    int total = ctx->width * ctx->height;
    for (int i = 0; i < total; ++i) {
        ctx->back_buffer[i].ch = ' ';
        ctx->back_buffer[i].fg = 0xFFFFFF;
        ctx->back_buffer[i].bg = bg;
        ctx->back_buffer[i].flags = 0;
    }
}

void tui_register_key_callback(TUIContext *ctx, TUIKeyCallback callback, void *user_data) {
    if (!ctx) return;
    ctx->callback = callback;
    ctx->user_data = user_data; // 挂载的用户自定义数据指针
}

void tui_put_string(TUIContext *ctx, int x, int y, const char *str, TUIColor fg, TUIColor bg) {
    for (size_t i = 0; str[i] != '\0'; ++i) {
        tui_put_cell(ctx, x + (int)i, y, (char32_t)(unsigned char)str[i], fg, bg);
    }
}

void tui_sleep_ms(int milliseconds) {
#if defined(__unix__) || defined(__APPLE__)
    usleep(milliseconds * 1000);
#elif defined(_WIN32)
    Sleep(milliseconds);
#endif
}