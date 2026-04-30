#include <time.h>
#include <math.h>
#include <uchar.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>

#define FPS 30
#define PERIOD_NS (1000000000L / FPS)

#define W 62
#define H 15

#include "tui.h"

void draw_prob(float p)
{
    int level = (int)(p * 64.0f + 0.5f);

    for (int y = 0; y < 8; y++)
    {
        int threshold = (8 - y) * 8;

        if (level >= threshold)
        {
			set_pixel(20, y+1, (Cell){
						U'█',
						BLACK, {0, 255, 100}
					});
        }
        else if (level > (7 - y) * 8)
        {
            const char32_t blocks[] = {
                U' ', U'▁', U'▂', U'▃',
				U'▄', U'▅', U'▆', U'▇'
            };

            int sub = level % 8;
			set_pixel(20, y+1, (Cell){
						blocks[sub],
						{60, 60, 60}, {0, 255, 100}
					});
        }
        else
        {
			set_pixel(20, y+1, (Cell){
						U'█',
						BLACK, {60, 60, 60}
					});
        }
    }
}

void draw_board()
{
	for (int y = 0; y < 8; y++)
	{
		set_ch(1, y + 1, (char32_t)('0' + 8 - y));
		for (int x = 0; x < 8; x++)
		{
			bool is_white = (x + y) % 2;
			int sc_x1 = 3 + x * 2;
			int sc_x2 = 4 + x * 2;
			int sc_y  = 1 + y;
			if (!is_white)
			{
				set_pixel(sc_x1, sc_y, (Cell){
							U' ',
							{200, 200, 200},{0, 0, 0}});
				set_pixel(sc_x2, sc_y, (Cell){
							U' ',
							{200, 200, 200},{0, 0, 0}});
			}
			else
			{
				set_pixel(sc_x1, sc_y, (Cell){
							U' ',
							{200, 128, 200},{0, 0, 0}});
				set_pixel(sc_x2, sc_y, (Cell){
							U' ',
							{200, 128, 200},{0, 0, 0}});
			}
		}
	}
	draw_string(3, 9, U"a b c d e f g h", BLACK, WHITE);
}

int main()
{
	printf("\033[?1049h\033[?25l");
	enable_raw_mode();

	init_screen();
	draw_board();
	
	float t = 0;
	while (1)
	{
		struct timespec start, end;
		clock_gettime(CLOCK_MONOTONIC, &start);

		char c = 0;
		read(STDIN_FILENO, &c, 1);
		if (c == 'q') break;
		float v = (sin(t) + 1) * 0.5;
		t += 0.3;
		draw_prob(v);
		draw_frame();

		clock_gettime(CLOCK_MONOTONIC, &end);
		long cost = (end.tv_sec - start.tv_sec) *
			1000000000L + (end.tv_nsec - start.tv_nsec);

		long sleep_ns = PERIOD_NS - cost;
		if (sleep_ns > 0)
		{
			struct timespec ts = {0, sleep_ns};
			nanosleep(&ts, NULL);
		}
	}
	printf("\033[?1049l\033[?25h");
}
