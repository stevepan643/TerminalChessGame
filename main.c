#include "tui/tui.h"
#include "chess_game.h"
#include <stdbool.h>
#include <string.h>

typedef struct {
	bool game_running;
	ChessGameState game_state;
	int buffer_len;
	char buffer[256]; // 用于输入命令或显示消息
} GameContext;

void quit_callback(TUIKeyEvent *event, void *user_data) {
	if (event->action == TUI_KEY_STATE_PRESS && event->key == TUI_KEY_ESCAPE) {
		GameContext *game = (GameContext *)user_data;
		game->game_running = false;
	}
	if (event->action == TUI_KEY_STATE_PRESS && (event->key <= TUI_KEY_Z && event->key >= TUI_KEY_A || event->key <= TUI_KEY_9 && event->key >= TUI_KEY_0)) {
		GameContext *game = (GameContext *)user_data;
		if (game->buffer_len < sizeof(game->buffer) - 1) {
			char ch = (char)(event->key - TUI_KEY_A + 'a');
			if (event->mods & TUI_MOD_SHIFT) {
				ch = (char)(event->key - TUI_KEY_A + 'A');
			} else if (event->key >= TUI_KEY_0 && event->key <= TUI_KEY_9) {
				ch = (char)(event->key - TUI_KEY_0 + '0');
				
			}
			game->buffer[game->buffer_len++] = ch;
			game->buffer[game->buffer_len] = '\0';
		}
	}
	if (event->action == TUI_KEY_STATE_PRESS && event->key == TUI_KEY_BACKSPACE) {
		GameContext *game = (GameContext *)user_data;
		if (game->buffer_len > 0) {
			game->buffer[--game->buffer_len] = '\0';
		}
	}
	if (event->action == TUI_KEY_STATE_PRESS && event->key == TUI_KEY_ENTER) {
		GameContext *game = (GameContext *)user_data;

		if (game->buffer_len > 0) {
			ChessMove move;

			if (chess_parse_single_pgn(
				&game->game_state,
				game->buffer,
				game->buffer_len,
				&move))
			{
				chess_execute_move(&game->game_state, move);
			}

			game->buffer_len = 0;
			game->buffer[0] = '\0';
		}
	}
}

int main() {
    TUIContext *ctx = tui_init();
    tui_hide_cursor(ctx); // 隐藏光标

	GameContext game = {0};
    chess_game_init(&game.game_state); // 初始化棋盘

	game.game_running = true;
	tui_register_key_callback(ctx, quit_callback, &game);

    while (game.game_running) {
        tui_poll_events(ctx);

        // ==========================================
        // 核心新增：AI 回合驱动状态机
        // ==========================================
        if (game.game_state.active_turn == COLOR_BLACK) { // 假设人类执白，AI 执黑
            TUISize size = tui_get_size(ctx);

            // 1. 先强制渲染一次玩家刚刚落子后的新局面，并提示 AI 正在思考
            tui_clear(ctx, 0x1E1E1E);
            tui_put_string(ctx, (size.width - 19) / 2, 0, "Terminal Chess Game", 0xFFFFFF, 0x1E1E1E);
            chess_game_render(ctx, &game.game_state, (size.width - 16) / 2, (size.height - 8) / 2);
            tui_put_string(ctx, (size.width - 14) / 2, size.height - 1, "AI is thinking...", 0x00FFFF, 0x1E1E1E);
            tui_present(ctx);

            // 2. 人工思考延迟 (2秒)
            tui_sleep_ms(2000);

            // 3. 生成 AI 走法
            ChessMove ai_move = chess_generate_ai_move(&game.game_state);

            // 检查 AI 是否还有路可走
            if (ai_move.from_x == 0 && ai_move.from_y == 0 && ai_move.to_x == 0 && ai_move.to_y == 0) {
                // 这里代表黑方无子可动。至于是将死还是逼和，可以结合当前王是否被攻击来判断
                game.game_running = false;
            } else {
                // 执行 AI 的走法。执行完后 active_turn 自动变回 COLOR_WHITE，下一帧又轮到人类输入
                chess_execute_move(&game.game_state, ai_move);
            }

            // AI 走完后直接 continue 重新进入下一帧，把新的事件交还给人类
            continue; 
        }

        // ==========================================
        // 以下为你原有的渲染与正常绘制逻辑（供人类回合使用）
        // ==========================================
        tui_clear(ctx, 0x1E1E1E);
        TUISize size = tui_get_size(ctx);

        if (size.width < 22 || size.height < 13) {
            tui_put_string(ctx, 0, 0, "Please resize the terminal to at least 22x13", 0xFF0000, 0x1E1E1E);
            tui_present(ctx);
            continue;
        }

        tui_put_string(ctx, (size.width - 19) / 2, 0, "Terminal Chess Game", 0xFFFFFF, 0x1E1E1E);
        chess_game_render(ctx, &game.game_state, (size.width - 16) / 2, (size.height - 8) / 2); 
        
        if (game.buffer_len > 0) {
            ChessMove move;
            bool moveable = chess_parse_single_pgn(&game.game_state, game.buffer, game.buffer_len, &move);
            tui_put_string(ctx, (size.width - game.buffer_len) / 2, size.height - 1, game.buffer, moveable ? 0x00FF00 : 0xAAAAAA, 0x1E1E1E);
        } else {
            tui_put_string(ctx, (size.width - 17) / 2, size.height - 1, "Press ESC to quit", 0xAAAAAA, 0x1E1E1E);
        }

        tui_present(ctx);
    }

    tui_shutdown(ctx);
    return 0;
}