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

typedef struct {
	size_t x, y;
	size_t w, h;
} Rect;

typedef uint32_t TypeID;

typedef struct {
	TypeID id;
	TypeID parent;
	const char *name;
} TypeInfo;

TypeID type_register(const char *name, TypeID parent);

typedef struct Widget	Widget;
typedef struct Panel	Panel;
typedef struct Button	Button;
typedef struct Text	Text;
typedef struct ProgBar	ProgBar;
typedef struct Input	Input;

typedef struct {
	void (*draw)(Widget *self);
	void (*update)(Widget *self, float dt);
	void (*handle_input)(Widget *self, int key);
	void (*destroy)(Widget *self);
} WidgetVTable;

struct Widget {
	TypeID type;
	Rect rect;
	WidgetVTable *vtable;
};

typedef struct Panel {
	Widget base;

	Widget **children;
	size_t count;
} Panel;

Widget *widget_new(TypeID type);
void widget_draw(Widget *w);
void widget_update(Widget *w, float dt);
void widget_handle_input(Widget *w, int key);
void widget_destroy(Widget *w);

Widget *panel_new();
void panel_add(Panel *p, Widget *w, Rect r);

Widget *button_new(const char32_t *str, Color t, Color h, Color p);

Widget *text_new(const char32_t *str, Color t, Color b);

Widget *progbar_new(Color f, Color u);
void progbar_set_progress(ProgBar *pb, float p);

Widget *input_new(const char32_t *d);

#endif
