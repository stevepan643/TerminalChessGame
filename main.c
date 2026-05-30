#include "tui/tui.h"
#include "chess/chess.h"
#include "ai/ai.h"
#include <stdbool.h>
#include <string.h>

typedef struct {
	bool game_running;
	ChessGameContext game_state;
	int buffer_len;
	char buffer[256];
    AIContext *ai_context;
} GameContext;

void quit_callback(TUIKeyEvent *event, void *user_data) {
    if (event->action == TUI_KEY_STATE_PRESS && event->key == TUI_KEY_ESCAPE) {
        GameContext *game = (GameContext *)user_data;
        game->game_running = false;
    }
    
    if (event->action == TUI_KEY_STATE_PRESS && 
        ((event->key <= TUI_KEY_Z && event->key >= TUI_KEY_A) || 
         (event->key <= TUI_KEY_9 && event->key >= TUI_KEY_0) ||
         event->key == TUI_KEY_MINUS)) {
         
        GameContext *game = (GameContext *)user_data;
        if (game->buffer_len < sizeof(game->buffer) - 1) {
            char ch = '\0';
            
            if (event->key >= TUI_KEY_A && event->key <= TUI_KEY_Z) {
                if (event->mods & TUI_MOD_SHIFT) {
                    ch = (char)(event->key - TUI_KEY_A + 'A');
                } else {
                    ch = (char)(event->key - TUI_KEY_A + 'a');
                }
            } else if (event->key >= TUI_KEY_0 && event->key <= TUI_KEY_9) {
                ch = (char)(event->key - TUI_KEY_0 + '0');
            } else if (event->key == TUI_KEY_MINUS) {
                ch = '-';
            }

            if (ch != '\0') {
                game->buffer[game->buffer_len++] = ch;
                game->buffer[game->buffer_len] = '\0';
            }
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

            if (chess_parse_move(
                &game->game_state,
                game->buffer,
                game->buffer_len,
                &move))
            {
                chess_execute_move(&game->game_state, &move);
            }

            game->buffer_len = 0;
            game->buffer[0] = '\0';
        }
    }
}

int main() {
    TUIContext *ctx = tui_init();
    tui_hide_cursor(ctx);

	GameContext game = {0};
    chess_game_init(&game.game_state);

	game.game_running = true;
	tui_register_key_callback(ctx, quit_callback, &game);

    game.ai_context = ai_create((AIConfig){ .search_depth = 10 });

    while (game.game_running) {
        tui_poll_events(ctx);

        if (game.game_state.active_turn == COLOR_BLACK) {
            TUISize size = tui_get_size(ctx);

            tui_clear(ctx, 0x1E1E1E);
            tui_put_string(ctx, (size.width - 19) / 2, 0, "Terminal Chess Game", 0xFFFFFF, 0x1E1E1E);
            chess_game_render(ctx, &game.game_state, (size.width - 16) / 2, (size.height - 8) / 2);
            tui_put_string(ctx, (size.width - 14) / 2, size.height - 1, "AI is thinking...", 0x00FFFF, 0x1E1E1E);
            tui_present(ctx);

            ChessMove ai_move;
            ai_find_best_move(game.ai_context, &game.game_state, &ai_move);

            if (ai_move.from_x == 0 && ai_move.from_y == 0 && ai_move.to_x == 0 && ai_move.to_y == 0) {
                game.game_running = false;
            } else {
                chess_execute_move(&game.game_state, &ai_move);
            }

            continue; 
        }

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
            bool moveable = chess_parse_move(&game.game_state, game.buffer, game.buffer_len, &move);
            tui_put_string(ctx, (size.width - game.buffer_len) / 2, size.height - 1, game.buffer, moveable ? 0x00FF00 : 0xAAAAAA, 0x1E1E1E);
        } else {
            tui_put_string(ctx, (size.width - 17) / 2, size.height - 1, "Press ESC to quit", 0xAAAAAA, 0x1E1E1E);
        }

        tui_present(ctx);
    }

    tui_shutdown(ctx);
    return 0;
}