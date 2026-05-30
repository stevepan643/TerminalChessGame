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
 * File: chess.c
 * 
 * Description:
 *      Implements the core logic for the chess game, 
 *      including move validation, move execution, 
 *      and rendering the game state onto the TUI context.
 * 
 * This file is part of TCG.
 */

#include "chess.h"

#include <stdlib.h>    /* For abs */
#include <string.h>    /* For strncpy & strcmp */

/* Castling rights bitmasks matching 0x0F initialization */
#define CASTLE_WK 0x01
#define CASTLE_WQ 0x02
#define CASTLE_BK 0x04
#define CASTLE_BQ 0x08

/* Checks if the path between two squares is clear */
static bool is_path_clear(const ChessGameContext *state, int from_x, int from_y, int to_x, int to_y) {
    int dx = to_x - from_x;
    int dy = to_y - from_y;
    int step_x = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);
    int step_y = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);
    
    int x = from_x + step_x;
    int y = from_y + step_y;
    
    while (x != to_x || y != to_y) {
        if (state->board[y][x].type != PIECE_NONE) {
            return false;
        }
        x += step_x;
        y += step_y;
    }
    return true;
}

/* Checks if a square is attacked by any piece of the specified color */
static bool is_square_attacked(const ChessGameContext *state, int target_x, int target_y, PieceColor attacker_color) {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            ChessPiece p = state->board[y][x];
            if (p.color != attacker_color || p.type == PIECE_NONE) continue;
            
            int dx = abs(target_x - x);
            int dy = abs(target_y - y);
            
            switch (p.type) {
                case PIECE_PAWN: {
                    int attack_dy = (attacker_color == COLOR_WHITE) ? -1 : 1;
                    if (dy == 1 && (target_y - y == attack_dy) && dx == 1) return true;
                    break;
                }
                case PIECE_KNIGHT:
                    if ((dx == 1 && dy == 2) || (dx == 2 && dy == 1)) return true;
                    break;
                case PIECE_BISHOP:
                    if (dx == dy && is_path_clear(state, x, y, target_x, target_y)) return true;
                    break;
                case PIECE_ROOK:
                    if ((dx == 0 || dy == 0) && is_path_clear(state, x, y, target_x, target_y)) return true;
                    break;
                case PIECE_QUEEN:
                    if ((dx == dy || dx == 0 || dy == 0) && is_path_clear(state, x, y, target_x, target_y)) return true;
                    break;
                case PIECE_KING:
                    if (dx <= 1 && dy <= 1) return true;
                    break;
                default: break;
            }
        }
    }
    return false;
}

/* Checks if the king of the specified color is currently in check */
static bool is_king_in_check(const ChessGameContext *state, PieceColor color) {
    int king_x = -1, king_y = -1;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (state->board[y][x].type == PIECE_KING && state->board[y][x].color == color) {
                king_x = x; king_y = y;
                break;
            }
        }
        if (king_x != -1) break;
    }
    if (king_x == -1) return false;
    PieceColor attacker_color = (color == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;
    return is_square_attacked(state, king_x, king_y, attacker_color);
}

/* Validates a move based on generic chess rules */
static bool validate_move_generic(const ChessGameContext *state, const ChessMove *move, bool check_king_safety) {
    // 1. Boundary check
    if (move->from_x < 0 || move->from_x >= 8 || move->from_y < 0 || move->from_y >= 8 ||
        move->to_x < 0 || move->to_x >= 8 || move->to_y < 0 || move->to_y >= 8) return false;
    
    // 2. Cannot stay in place
    if (move->from_x == move->to_x && move->from_y == move->to_y) return false;
    
    ChessPiece p = state->board[move->from_y][move->from_x];
    // 3. Piece existence and active turn validation
    if (p.type == PIECE_NONE || p.color != state->active_turn) return false;
    
    // 4. Friendly fire prevention
    ChessPiece target = state->board[move->to_y][move->to_x];
    if (target.type != PIECE_NONE && target.color == state->active_turn) return false;
    
    int dx = abs(move->to_x - move->from_x);
    int dy = abs(move->to_y - move->from_y);
    bool pseudo_legal = false;
    
    switch (p.type) {
        case PIECE_PAWN: {
            int direction = (p.color == COLOR_WHITE) ? -1 : 1;
            int start_row = (p.color == COLOR_WHITE) ? 6 : 1;
            
            // Forward moves
            if (move->to_x == move->from_x) {
                if (move->to_y - move->from_y == direction) {
                    if (target.type == PIECE_NONE) pseudo_legal = true;
                } else if (move->to_y - move->from_y == 2 * direction && move->from_y == start_row) {
                    if (target.type == PIECE_NONE && state->board[move->from_y + direction][move->from_x].type == PIECE_NONE) {
                        pseudo_legal = true;
                    }
                }
            } 
            // Standard capture & En Passant
            else if (dx == 1 && move->to_y - move->from_y == direction) {
                if (target.type != PIECE_NONE && target.color != p.color) {
                    pseudo_legal = true;
                } else if (state->en_passant_file == move->to_x && move->to_y == (p.color == COLOR_WHITE ? 2 : 5)) {
                    pseudo_legal = true;
                }
            }
            break;
        }
        case PIECE_KNIGHT:
            if ((dx == 1 && dy == 2) || (dx == 2 && dy == 1)) pseudo_legal = true;
            break;
        case PIECE_BISHOP:
            if (dx == dy && is_path_clear(state, move->from_x, move->from_y, move->to_x, move->to_y)) pseudo_legal = true;
            break;
        case PIECE_ROOK:
            if ((dx == 0 || dy == 0) && is_path_clear(state, move->from_x, move->from_y, move->to_x, move->to_y)) pseudo_legal = true;
            break;
        case PIECE_QUEEN:
            if ((dx == dy || dx == 0 || dy == 0) && is_path_clear(state, move->from_x, move->from_y, move->to_x, move->to_y)) pseudo_legal = true;
            break;
        case PIECE_KING:
            if (dx <= 1 && dy <= 1) {
                pseudo_legal = true;
            } else if (dx == 2 && dy == 0) {
                // Castling check
                PieceColor enemy = (p.color == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;
                if (p.color == COLOR_WHITE && move->from_y == 7 && move->from_x == 4) {
                    if (move->to_x == 6 && (state->castling_rights & CASTLE_WK)) {
                        if (state->board[7][5].type == PIECE_NONE && state->board[7][6].type == PIECE_NONE) {
                            if (!is_square_attacked(state, 4, 7, enemy) &&
                                !is_square_attacked(state, 5, 7, enemy) &&
                                !is_square_attacked(state, 6, 7, enemy)) {
                                pseudo_legal = true;
                            }
                        }
                    } else if (move->to_x == 2 && (state->castling_rights & CASTLE_WQ)) {
                        if (state->board[7][1].type == PIECE_NONE && state->board[7][2].type == PIECE_NONE && state->board[7][3].type == PIECE_NONE) {
                            if (!is_square_attacked(state, 4, 7, enemy) &&
                                !is_square_attacked(state, 3, 7, enemy) &&
                                !is_square_attacked(state, 2, 7, enemy)) {
                                pseudo_legal = true;
                            }
                        }
                    }
                } else if (p.color == COLOR_BLACK && move->from_y == 0 && move->from_x == 4) {
                    if (move->to_x == 6 && (state->castling_rights & CASTLE_BK)) {
                        if (state->board[0][5].type == PIECE_NONE && state->board[0][6].type == PIECE_NONE) {
                            if (!is_square_attacked(state, 4, 0, enemy) &&
                                !is_square_attacked(state, 5, 0, enemy) &&
                                !is_square_attacked(state, 6, 0, enemy)) {
                                pseudo_legal = true;
                            }
                        }
                    } else if (move->to_x == 2 && (state->castling_rights & CASTLE_BQ)) {
                        if (state->board[0][1].type == PIECE_NONE && state->board[0][2].type == PIECE_NONE && state->board[0][3].type == PIECE_NONE) {
                            if (!is_square_attacked(state, 4, 0, enemy) &&
                                !is_square_attacked(state, 3, 0, enemy) &&
                                !is_square_attacked(state, 2, 0, enemy)) {
                                pseudo_legal = true;
                            }
                        }
                    }
                }
            }
            break;
        default: break;
    }
    
    if (!pseudo_legal) return false;
    if (!check_king_safety) return true;
    
    // Simulate move to ensure King safety
    ChessGameContext temp_state = *state;
    temp_state.board[move->to_y][move->to_x] = p;
    temp_state.board[move->from_y][move->from_x] = (ChessPiece){PIECE_NONE, COLOR_NONE};
    
    if (p.type == PIECE_PAWN && dx == 1 && target.type == PIECE_NONE) {
        // Remove en-passant captured pawn in simulation
        temp_state.board[move->from_y][move->to_x] = (ChessPiece){PIECE_NONE, COLOR_NONE};
    }
    
    return !is_king_in_check(&temp_state, state->active_turn);
}

char32_t get_piece_char(ChessPiece piece) {
    switch (piece.type) {
        case PIECE_KING:   return 0x265A;   /* ♚ */
        case PIECE_QUEEN:  return 0x265B;   /* ♛ */
        case PIECE_ROOK:   return 0x265C;   /* ♜ */
        case PIECE_BISHOP: return 0x265D;   /* ♝ */
        case PIECE_KNIGHT: return 0x265E;   /* ♞ */
        case PIECE_PAWN:   return 0x265F;   /* ♟ */
        default: return ' ';
    }
    return ' ';
}

void chess_init_default_theme(ChessTheme *theme) {
    theme->light_square = 0xEDD9B5; 
    theme->dark_square  = 0xB58863; 
    theme->white_piece  = 0xFFFFFF; 
    theme->black_piece  = 0x1A1A1A; 
    theme->text_color   = 0x888888; 
    theme->highlight_bg = 0x7A9A60; 
    theme->checkmate_bg = 0xCC3333; 
}

void chess_game_init(ChessGameContext *state) {
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            state->board[r][c] = (ChessPiece){PIECE_NONE, COLOR_NONE};
        }
    }

    PieceType backline[] = {PIECE_ROOK, PIECE_KNIGHT, PIECE_BISHOP, PIECE_QUEEN, PIECE_KING, PIECE_BISHOP, PIECE_KNIGHT, PIECE_ROOK};
    for (int c = 0; c < 8; c++) {
        state->board[0][c] = (ChessPiece){backline[c], COLOR_BLACK};
        state->board[7][c] = (ChessPiece){backline[c], COLOR_WHITE};
    }

    for (int c = 0; c < 8; c++) {
        state->board[1][c] = (ChessPiece){PIECE_PAWN, COLOR_BLACK};
        state->board[6][c] = (ChessPiece){PIECE_PAWN, COLOR_WHITE};
    }

    state->active_turn = COLOR_WHITE;   
    state->castling_rights = 0x0F;      
    state->en_passant_file = -1;        
    state->halfmove_clock = 0;
    state->fullmove_number = 1;
    state->status = STATUS_ONGOING;     

    chess_init_default_theme(&state->theme);
}

void chess_game_render(TUIContext *ctx, ChessGameContext *state, int start_x, int start_y) {
    ChessTheme *theme = &state->theme;

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            TUIColor bg = ((r + c) % 2 == 0) ? theme->light_square : theme->dark_square;

            ChessPiece piece = state->board[r][c];
            TUIColor fg = (piece.color == COLOR_WHITE) ? theme->white_piece : theme->black_piece;
            char32_t p_char = get_piece_char(piece);

            int tui_x = start_x + c * 2;
            int tui_y = start_y + r;

            tui_put_cell(ctx, tui_x,     tui_y, p_char, fg, bg, 0);
            tui_put_cell(ctx, tui_x + 1, tui_y, ' ',    fg, bg, 0);
        }
    }

    for (int r = 0; r < 8; r++) {
        char label[2] = { '8' - r, '\0' };
        tui_put_string(ctx, start_x - 2, start_y + r, label, theme->text_color, 0x1E1E1E); 
    }

    for (int c = 0; c < 8; c++) {
        char label[3] = { 'A' + c, ' ', '\0' }; 
        tui_put_string(ctx, start_x + c * 2, start_y + 8, label, theme->text_color, 0x1E1E1E); 
    }
}

ChessMove create_move(int from_x, int from_y, int to_x, int to_y, PieceType promotion) {
    ChessMove move = {from_x, from_y, to_x, to_y, promotion, {NULL, NULL}};
    return move;
}

bool chess_move_is_valid(const ChessGameContext *state, ChessMove *move) {
    if (!state || !move || state->status != STATUS_ONGOING) return false;
    return validate_move_generic(state, move, true);
}

bool chess_execute_move(ChessGameContext *state, ChessMove *move) {
    if (!chess_move_is_valid(state, move)) return false;

    ChessPiece p = state->board[move->from_y][move->from_x];
    ChessPiece target = state->board[move->to_y][move->to_x];
    
    bool is_pawn_or_capture = (p.type == PIECE_PAWN || target.type != PIECE_NONE);
    int dx = abs(move->to_x - move->from_x);

    // Handle En Passant Capture
    if (p.type == PIECE_PAWN && dx == 1 && target.type == PIECE_NONE) {
        state->board[move->from_y][move->to_x] = (ChessPiece){PIECE_NONE, COLOR_NONE};
        is_pawn_or_capture = true;
    }

    // Handle Castling Rook Movement
    if (p.type == PIECE_KING && dx == 2) {
        if (move->to_x == 6) { // Kingside
            state->board[move->to_y][5] = state->board[move->to_y][7];
            state->board[move->to_y][7] = (ChessPiece){PIECE_NONE, COLOR_NONE};
        } else if (move->to_x == 2) { // Queenside
            state->board[move->to_y][3] = state->board[move->to_y][0];
            state->board[move->to_y][0] = (ChessPiece){PIECE_NONE, COLOR_NONE};
        }
    }

    // Handle Promotion Trigger
    if (p.type == PIECE_PAWN && (move->to_y == 0 || move->to_y == 7)) {
        p.type = (move->promotion != PIECE_NONE) ? move->promotion : PIECE_QUEEN;
    }

    // Perform actual atomic square update
    state->board[move->to_y][move->to_x] = p;
    state->board[move->from_y][move->from_x] = (ChessPiece){PIECE_NONE, COLOR_NONE};

    /* Update Castling Rights Rule Triggers */
    if (p.type == PIECE_KING) {
        if (p.color == COLOR_WHITE) state->castling_rights &= ~(CASTLE_WK | CASTLE_WQ);
        else state->castling_rights &= ~(CASTLE_BK | CASTLE_BQ);
    }
    // Corner Rooks tracking
    if (move->from_y == 7 && move->from_x == 7) state->castling_rights &= ~CASTLE_WK;
    if (move->from_y == 7 && move->from_x == 0) state->castling_rights &= ~CASTLE_WQ;
    if (move->from_y == 0 && move->from_x == 7) state->castling_rights &= ~CASTLE_BK;
    if (move->from_y == 0 && move->from_x == 0) state->castling_rights &= ~CASTLE_BQ;
    // Captured rooks tracking
    if (move->to_y == 7 && move->to_x == 7) state->castling_rights &= ~CASTLE_WK;
    if (move->to_y == 7 && move->to_x == 0) state->castling_rights &= ~CASTLE_WQ;
    if (move->to_y == 0 && move->to_x == 7) state->castling_rights &= ~CASTLE_BK;
    if (move->to_y == 0 && move->to_x == 0) state->castling_rights &= ~CASTLE_BQ;

    // Update En Passant Availability
    if (p.type == PIECE_PAWN && abs(move->to_y - move->from_y) == 2) {
        state->en_passant_file = move->from_x;
    } else {
        state->en_passant_file = -1;
    }

    // Halfmove & Fullmove counts updates
    if (is_pawn_or_capture) state->halfmove_clock = 0;
    else state->halfmove_clock++;

    if (state->active_turn == COLOR_BLACK) {
        state->fullmove_number++;
    }

    // Toggle active player's turn
    state->active_turn = (state->active_turn == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;

    /* Turn Endgame Evaluation (Checkmate / Stalemate) */
    bool has_legal_moves = false;
    for (int fy = 0; fy < 8 && !has_legal_moves; fy++) {
        for (int fx = 0; fx < 8 && !has_legal_moves; fx++) {
            if (state->board[fy][fx].color == state->active_turn) {
                for (int ty = 0; ty < 8 && !has_legal_moves; ty++) {
                    for (int tx = 0; tx < 8 && !has_legal_moves; tx++) {
                        ChessMove test_move = create_move(fx, fy, tx, ty, PIECE_NONE);
                        if (validate_move_generic(state, &test_move, true)) {
                            has_legal_moves = true;
                        }
                    }
                }
            }
        }
    }

    if (!has_legal_moves) {
        if (is_king_in_check(state, state->active_turn)) {
            state->status = (state->active_turn == COLOR_WHITE) ? STATUS_CHECKMATE_BLACK_WINS : STATUS_CHECKMATE_WHITE_WINS;
        } else {
            state->status = STATUS_DRAW_STALEMATE;
        }
    } else if (state->halfmove_clock >= 100) {
        state->status = STATUS_DRAW_FIFTY_MOVES;
    }

    return true;
}

bool chess_parse_move(const ChessGameContext *state, const char *move_str, size_t move_str_len, ChessMove *out_move) {
    if (!move_str || !out_move || !state || move_str_len == 0) return false;

    char buf[16];
    if (move_str_len >= sizeof(buf)) return false;
    strncpy(buf, move_str, move_str_len);
    buf[move_str_len] = '\0';
    int len = (int)move_str_len;

    /* Trim trailing check/checkmate symbols */
    while (len > 0 && (buf[len - 1] == '+' || buf[len - 1] == '#')) {
        buf[--len] = '\0';
    }
    if (len == 0) return false;

    /* Parse Castling Moves */
    if (strcmp(buf, "O-O") == 0 || strcmp(buf, "o-o") == 0) {
        int king_y = (state->active_turn == COLOR_WHITE) ? 7 : 0;
        *out_move = create_move(4, king_y, 6, king_y, PIECE_NONE);
        return chess_move_is_valid(state, out_move);
    }
    if (strcmp(buf, "O-O-O") == 0 || strcmp(buf, "o-o-o") == 0) {
        int king_y = (state->active_turn == COLOR_WHITE) ? 7 : 0;
        *out_move = create_move(4, king_y, 2, king_y, PIECE_NONE);
        return chess_move_is_valid(state, out_move);
    }

    /* Parse Promotion Moves */
    PieceType promo = PIECE_NONE;
    if (len >= 2 && buf[len - 2] == '=') {
        char p_char = buf[len - 1];
        if (p_char == 'q' || p_char == 'Q') promo = PIECE_QUEEN;
        else if (p_char == 'r' || p_char == 'R') promo = PIECE_ROOK;
        else if (p_char == 'b' || p_char == 'B') promo = PIECE_BISHOP;
        else if (p_char == 'n' || p_char == 'N') promo = PIECE_KNIGHT;
        else return false;
        len -= 2;
        buf[len] = '\0';
    } else if (len >= 2 && (buf[len - 1] == 'Q' || buf[len - 1] == 'R' || buf[len - 1] == 'B' || buf[len - 1] == 'N' ||
                           buf[len - 1] == 'q' || buf[len - 1] == 'r' || buf[len - 1] == 'b' || buf[len - 1] == 'n')) {
        /* Some SAN formats omit the '=' sign for promotions */
        if (buf[len - 2] >= '1' && buf[len - 2] <= '8') {
            char p_char = buf[len - 1];
            if (p_char == 'q' || p_char == 'Q') promo = PIECE_QUEEN;
            else if (p_char == 'r' || p_char == 'R') promo = PIECE_ROOK;
            else if (p_char == 'b' || p_char == 'B') promo = PIECE_BISHOP;
            else if (p_char == 'n' || p_char == 'N') promo = PIECE_KNIGHT;
            len -= 1;
            buf[len] = '\0';
        }
    }

    /* Parse the piece type */
    PieceType expected_piece = PIECE_PAWN;
    int idx = 0;
    if (buf[0] == 'K') { expected_piece = PIECE_KING; idx++; }
    else if (buf[0] == 'Q') { expected_piece = PIECE_QUEEN; idx++; }
    else if (buf[0] == 'R') { expected_piece = PIECE_ROOK; idx++; }
    else if (buf[0] == 'B') { expected_piece = PIECE_BISHOP; idx++; }
    else if (buf[0] == 'N') { expected_piece = PIECE_KNIGHT; idx++; }

    /* Parse the target square */
    if (len - idx < 2) return false;
    int tx = buf[len - 2] - 'a';
    if (tx < 0 || tx >= 8) tx = buf[len - 2] - 'A';
    int ty = '8' - buf[len - 1];
    if (tx < 0 || tx >= 8 || ty < 0 || ty >= 8) return false;

    /*
     * Parse disambiguating modifiers
     * e.g. the 'b' in Nbd2 (specifies starting file), the '1' in R1e2 (specifies starting rank), the 'e' in exd5
     */
    int req_fx = -1;
    int req_fy = -1;
    for (int i = idx; i < len - 2; i++) {
        if (buf[i] == 'x' || buf[i] == 'X') continue;
        if (buf[i] >= 'a' && buf[i] <= 'h') {
            req_fx = buf[i] - 'a';
        } else if (buf[i] >= '1' && buf[i] <= '8') {
            req_fy = '8' - buf[i];
        } else {
            return false;
        }
    }

    /* Find the unique legal move */
    int match_count = 0;
    ChessMove found_move;

    for (int fy = 0; fy < 8; fy++) {
        if (req_fy != -1 && fy != req_fy) continue;
        for (int fx = 0; fx < 8; fx++) {
            if (req_fx != -1 && fx != req_fx) continue;

            if (state->board[fy][fx].type != expected_piece) continue;
            if (state->board[fy][fx].color != state->active_turn) continue;

            ChessMove candidate = create_move(fx, fy, tx, ty, promo);
            if (chess_move_is_valid(state, &candidate)) {
                match_count++;
                found_move = candidate;
            }
        }
    }

    if (match_count == 1) {
        *out_move = found_move;
        return true;
    }

    return false; 
}

bool chess_move_to_string(const ChessMove *move, char *buffer, size_t buffer_size) {
    if (!move || !buffer) return false;

    if (move->from_x == 4 && (move->from_y == 0 || move->from_y == 7) && move->from_y == move->to_y) {
        if (move->to_x == 6) { 
            if (buffer_size < 4) return false;
            buffer[0] = 'O'; buffer[1] = '-'; buffer[2] = 'O'; buffer[3] = '\0';
            return true;
        } else if (move->to_x == 2) { 
            if (buffer_size < 6) return false;
            buffer[0] = 'O'; buffer[1] = '-'; buffer[2] = 'O'; buffer[3] = '-'; buffer[4] = 'O'; buffer[5] = '\0';
            return true;
        }
    }

    size_t required = (move->promotion != PIECE_NONE) ? 6 : 5;
    if (buffer_size < required) return false;

    buffer[0] = 'a' + move->from_x;
    buffer[1] = '8' - move->from_y;
    buffer[2] = 'a' + move->to_x;
    buffer[3] = '8' - move->to_y;

    if (move->promotion != PIECE_NONE) {
        switch (move->promotion) {
            case PIECE_QUEEN:  buffer[4] = 'q'; break;
            case PIECE_ROOK:   buffer[4] = 'r'; break;
            case PIECE_BISHOP: buffer[4] = 'b'; break;
            case PIECE_KNIGHT: buffer[4] = 'n'; break;
            default: return false;
        }
        buffer[5] = '\0';
    } else {
        buffer[4] = '\0';
    }
    return true;
}

size_t chess_generate_legal_moves(const ChessGameContext *state, struct ListHead *move_list) {
    if (!state || !move_list) return 0;

    size_t move_count = 0;
    PieceColor current_turn = state->active_turn;

    /* Enumerate all legal moves */
    for (int from_y = 0; from_y < 8; from_y++) {
        for (int from_x = 0; from_x < 8; from_x++) {
            /* Get the piece at the current square */
            ChessPiece piece = state->board[from_y][from_x];

            /* Check if the piece belongs to the current player */
            if (piece.type == PIECE_NONE || piece.color != current_turn) {
                continue;
            }

            /* Enumerate all possible target squares */
            for (int to_y = 0; to_y < 8; to_y++) {
                for (int to_x = 0; to_x < 8; to_x++) {
                    if (from_x == to_x && from_y == to_y) {
                        continue;
                    }

                    /* Check for pawn promotion */
                    bool is_pawn_promotion = false;
                    if (piece.type == PIECE_PAWN) {
                        if ((current_turn == COLOR_WHITE && to_y == 7) || 
                            (current_turn == COLOR_BLACK && to_y == 0)) {
                            is_pawn_promotion = true;
                        }
                    }

                    if (is_pawn_promotion) {
                        /* Generate promotion moves */
                        PieceType promotion_types[] = {PIECE_QUEEN, PIECE_ROOK, PIECE_BISHOP, PIECE_KNIGHT};
                        
                        for (int i = 0; i < 4; i++) {
                            ChessMove move = create_move(from_x, from_y, to_x, to_y, promotion_types[i]);
                            
                            /* Validate the move */
                            if (chess_move_is_valid(state, &move)) {
                                ChessMove *new_move = (ChessMove *)malloc(sizeof(ChessMove));
                                if (new_move) {
                                    *new_move = move;
                                    list_add_tail(&new_move->list_node, move_list);
                                    move_count++;
                                }
                            }
                        }
                    } else {
                        /* Create a regular move */
                        ChessMove move = create_move(from_x, from_y, to_x, to_y, PIECE_NONE);
                        
                        if (chess_move_is_valid(state, &move)) {
                            ChessMove *new_move = (ChessMove *)malloc(sizeof(ChessMove));
                            if (new_move) {
                                *new_move = move;
                                list_add_tail(&new_move->list_node, move_list);
                                move_count++;
                            }
                        }
                    }
                }
            }
        }
    }

    return move_count;
}

void chess_free_move_list(struct ListHead *move_list) {
    if (!move_list) return;

    struct ListHead *pos, *q;
    list_for_each_safe(pos, q, move_list) {
        ChessMove *move = list_entry(pos, ChessMove, list_node);
        list_del(pos);
        free(move);
    }
}