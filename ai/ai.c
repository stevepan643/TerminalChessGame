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
 * File: ai.c
 * 
 * Description:
 *      Implements the AI logic for the chess game.
 * 
 * This file is part of TCG.
 */

#include "ai/ai.h"

#include <stdlib.h>    /* For malloc and free */

#define EVAL_INFINITY 1000000
#define EVAL_CHECKMATE 30000

struct AIContext {
    AIConfig config;
};

static int ai_evaluate_board(const ChessGameContext *state) {
    if (state->status == STATUS_CHECKMATE_WHITE_WINS) return EVAL_CHECKMATE;
    if (state->status == STATUS_CHECKMATE_BLACK_WINS) return -EVAL_CHECKMATE;
    if (state->status == STATUS_DRAW_STALEMATE || 
        state->status == STATUS_DRAW_FIFTY_MOVES || 
        state->status == STATUS_DRAW_REPETITION || 
        state->status == STATUS_DRAW_INSUFFICIENT_MATERIAL) {
        return 0;
    }

    int total_score = 0;

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            ChessPiece piece = state->board[y][x];
            int piece_value = 0;

            switch (piece.type) {
                case PIECE_PAWN:   piece_value = 100;  break;
                case PIECE_KNIGHT: piece_value = 320;  break;
                case PIECE_BISHOP: piece_value = 330;  break;
                case PIECE_ROOK:   piece_value = 500;  break;
                case PIECE_QUEEN:  piece_value = 900;  break;
                case PIECE_KING:   piece_value = 20000; break;
                default:           piece_value = 0;    break;
            }

            /* Additional positional bonuses */
            if (piece_value > 0 && x >= 2 && x <= 5 && y >= 2 && y <= 5) {
                piece_value += 10;
            }

            if (piece.color == COLOR_WHITE) {
                total_score += piece_value;
            } else if (piece.color == COLOR_BLACK) {
                total_score -= piece_value;
            }
        }
    }

    return total_score;
}

static int minimax(const ChessGameContext *state, int depth, int alpha, int beta, bool is_maximizing) {
    if (depth == 0 || state->status != STATUS_ONGOING) {
        return ai_evaluate_board(state);
    }

    struct ListHead move_list;
    move_list.next = &move_list;
    move_list.prev = &move_list;

    size_t move_count = chess_generate_legal_moves(state, &move_list);
    if (move_count == 0) {
        chess_free_move_list(&move_list);
        return ai_evaluate_board(state);
    }

    struct ListHead *pos;
    if (is_maximizing) {
        int max_eval = -EVAL_INFINITY;
        
        for (pos = move_list.next; pos != &move_list; pos = pos->next) {
            ChessMove *move = (ChessMove *)((char *)pos - offsetof(ChessMove, list_node));
            
            ChessGameContext next_state = *state;
            chess_execute_move(&next_state, move);

            int eval = minimax(&next_state, depth - 1, alpha, beta, false);
            if (eval > max_eval) max_eval = eval;
            if (eval > alpha) alpha = eval;
            
            /* Alpha-Beta 剪枝触发 */
            if (beta <= alpha) break;
        }
        
        chess_free_move_list(&move_list);
        return max_eval;
    } else {
        int min_eval = EVAL_INFINITY;
        
        for (pos = move_list.next; pos != &move_list; pos = pos->next) {
            ChessMove *move = (ChessMove *)((char *)pos - offsetof(ChessMove, list_node));
            
            ChessGameContext next_state = *state;
            chess_execute_move(&next_state, move);

            int eval = minimax(&next_state, depth - 1, alpha, beta, true);
            if (eval < min_eval) min_eval = eval;
            if (eval < beta) beta = eval;
            
            /* Alpha-Beta cutoff */
            if (beta <= alpha) break;
        }
        
        chess_free_move_list(&move_list);
        return min_eval;
    }
}

AIContext *ai_create(const AIConfig config) {
    AIContext *ctx = (AIContext *)malloc(sizeof(AIContext));
    if (ctx) {
        ctx->config = config;
    }
    return ctx;
}

void ai_destroy(AIContext *ctx) {
    if (ctx) {
        free(ctx);
    }
}

void ai_find_best_move(AIContext *context, const ChessGameContext *state, ChessMove *best_move) {
    if (!context || !state || !best_move) return;

    int depth = context->config.search_depth;
    if (depth <= 0) depth = 3;

    PieceColor ai_color = state->active_turn;
    bool is_maximizing = (ai_color == COLOR_WHITE);

    struct ListHead move_list;
    move_list.next = &move_list;
    move_list.prev = &move_list;

    size_t move_count = chess_generate_legal_moves(state, &move_list);
    if (move_count == 0) {
        chess_free_move_list(&move_list);
        return; 
    }

    int alpha = -EVAL_INFINITY;
    int beta = EVAL_INFINITY;
    
    ChessMove chosen_move;
    bool move_found = false;

    struct ListHead *pos;
    if (is_maximizing) {
        int max_eval = -EVAL_INFINITY;
        for (pos = move_list.next; pos != &move_list; pos = pos->next) {
            ChessMove *move = (ChessMove *)((char *)pos - offsetof(ChessMove, list_node));
            
            ChessGameContext next_state = *state;
            chess_execute_move(&next_state, move);

            int eval = minimax(&next_state, depth - 1, alpha, beta, false);
            if (eval > max_eval || !move_found) {
                max_eval = eval;
                chosen_move = *move;
                move_found = true;
            }
            if (eval > alpha) alpha = eval;
        }
    } else {
        int min_eval = EVAL_INFINITY;
        for (pos = move_list.next; pos != &move_list; pos = pos->next) {
            ChessMove *move = (ChessMove *)((char *)pos - offsetof(ChessMove, list_node));
            
            ChessGameContext next_state = *state;
            chess_execute_move(&next_state, move);

            int eval = minimax(&next_state, depth - 1, alpha, beta, true);
            if (eval < min_eval || !move_found) {
                min_eval = eval;
                chosen_move = *move;
                move_found = true;
            }
            if (eval < beta) beta = eval;
        }
    }

    if (move_found) {
        *best_move = chosen_move;
    }

    chess_free_move_list(&move_list);
}