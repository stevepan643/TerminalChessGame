#include <time.h>
#include <math.h>
#include <uchar.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>

#include "tui.h"

Cell screen[H][W];

void init_screen()
{
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
		{
			screen[y][x] = (Cell){
				U' ',
				{0, 0, 0}, {255, 255, 255}
			};
		}
}

void set_pixel(size_t x, size_t y, Cell p)
{
	screen[y][x] = p;
}

void set_fg(size_t x, size_t y, Color fg)
{
	screen[y][x].fg = fg;
}

void set_bg(size_t x, size_t y, Color bg)
{
	screen[y][x].bg = bg;
}

void set_ch(size_t x, size_t y, char32_t ch)
{
	screen[y][x].ch = ch;
}

int utf8_encode(char32_t cp, char out[4])
{
	if (cp <= 0x7F)
	{
		out[0] = cp;
		return 1;
	}
	else if (cp <= 0x7FF)
	{
		out[0] = 0xC0 | (cp >> 6);
		out[1] = 0x80 | (cp &  0x3F);
		return 2;
	}
	else if ( cp <= 0xFFFF)
	{
		out[0] = 0xE0 | (cp >> 12);
		out[1] = 0x80 | ((cp >> 6) & 0x3F);
		out[2] = 0x80 | (cp &  0x3F);
		return 3;
	}
	else
	{
		out[0] = 0xF0 | (cp >> 18);
		out[1] = 0x80 | ((cp >> 12) & 0x3F);
		out[2] = 0x80 | ((cp >> 6 ) & 0x3F);
		out[3] = 0x80 | (cp &  0x3F);
		return 4;
	}
}

size_t c32len(char32_t *str)
{
	size_t n = 0;
	while (*str++) n++;
	return n;
}

void draw_string(size_t x, size_t y, char32_t *str, Color bg, Color fg)
{
	size_t l = c32len(str);
	for (int i = 0; i < l; i++)
	{
		set_pixel(x + i, y, (Cell){
					str[i], bg, fg
				});
	}
}

void draw_frame()
{
	printf("\x1b[1J\x1b[H");

    int last_fr=-1,last_fg=-1,last_fb=-1;
    int last_br=-1,last_bg=-1,last_bb=-1;

    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            Cell c = screen[y][x];

            int fr = c.fg.r;
            int fg = c.fg.g;
            int fb = c.fg.b;

            int br = c.bg.r;
            int bg = c.bg.g;
            int bb = c.bg.b;

            if (fr!=last_fr || fg!=last_fg || fb!=last_fb)
            {
                printf("\x1b[38;2;%d;%d;%dm", fr, fg, fb);
                last_fr=fr; last_fg=fg; last_fb=fb;
            }

            if (br!=last_br || bg!=last_bg || bb!=last_bb)
            {
                printf("\x1b[48;2;%d;%d;%dm", br, bg, bb);
                last_br=br; last_bg=bg; last_bb=bb;
            }

            char buf[4];
            int len = utf8_encode(c.ch, buf);
            fwrite(buf, 1, len, stdout);
        }
		if (y != H - 1) putchar('\n');
    }

    printf("\x1b[0m");
	fflush(stdout);
}

struct termios orig_termios;
void disable_rew_mode()
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode()
{
	tcgetattr(STDIN_FILENO, &orig_termios);
	atexit(disable_rew_mode);

	struct termios raw = orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
