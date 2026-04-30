#ifndef TUI_H
#define TUI_H

typedef struct {
	uint8_t r, g, b;
} Color;

#define WHITE (Color){255, 255, 255}
#define BLACK (Color){0, 0, 0}

typedef struct {
	char32_t ch;
	Color bg, fg;
} Cell;

#ifndef W
#define W 62
#endif
#ifndef H
#define H 15
#endif

extern Cell screen[H][W];

void init_screen();
void set_pixel(size_t x, size_t y, Cell p);
void set_fg(size_t x, size_t y, Color fg);
void set_bg(size_t x, size_t y, Color bg);
void set_ch(size_t x, size_t y, char32_t ch);
void draw_string(size_t x, size_t, char32_t *str, Color bg, Color fg);
void draw_frame();
void disable_rew_mode();
void enable_raw_mode();

#endif
