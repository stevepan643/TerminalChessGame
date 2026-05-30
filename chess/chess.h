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
 * File: chess.h
 * 
 * Description:
 *      Defines the core data structures 
 *      and functions for the chess game logic, 
 *      including the game state representation, 
 *      move execution, and rendering logic.
 * 
 * This file is part of TCG.
 */

#ifndef TCG_CHESS_H
#define TCG_CHESS_H

#include "tui/tui.h"    /* For TUIColor and TUIContext */
#include "util/list.h"       /* For intrusive linked list */

#include <stdint.h>     /* For uint8_t */
#include <stdbool.h>    /* For bool */
#include <stddef.h>     /* For size_t */

/* Enumerates the different types of chess pieces */
typedef enum {
    PIECE_NONE = 0,
    PIECE_PAWN,
    PIECE_KNIGHT,
    PIECE_BISHOP,
    PIECE_ROOK,
    PIECE_QUEEN,
    PIECE_KING
} PieceType;

/* Enumerates the different colors of chess pieces */
typedef enum {
    COLOR_NONE = 0,
    COLOR_WHITE,
    COLOR_BLACK
} PieceColor;

/* Represents a chess piece */
typedef struct {
    PieceType type;
    PieceColor color;
} ChessPiece;

/* Represents the theme for the chess board */
typedef struct {
    TUIColor light_square;  /* Light square color */
    TUIColor dark_square;   /* Dark square color */
    TUIColor white_piece;   /* White piece color */
    TUIColor black_piece;   /* Black piece color */
    TUIColor text_color;    /* Text color */
    TUIColor highlight_bg;  /* Highlight background color */
    TUIColor checkmate_bg;  /* Checkmate background color */
} ChessTheme;

/* Represents a chess move */
typedef struct {
    int from_x, from_y;     /* Starting square coordinates */
    int to_x, to_y;         /* Target square coordinates */
    PieceType promotion;    /* Promotion piece type (if applicable, otherwise PIECE_NONE) */

    struct ListHead list_node; /* For linking moves in a list (if needed) */
} ChessMove;

/* Represents information about a player */
typedef struct {
    char name[32];          /* Player's name */
    PieceColor color;       /* Player's color (COLOR_WHITE or COLOR_BLACK) */
} PlayerInfo;

/* Represents the status of the chess game */
typedef enum {
    STATUS_ONGOING = 0,
    
    STATUS_CHECKMATE_WHITE_WINS,    /* White wins by checkmate */
    STATUS_CHECKMATE_BLACK_WINS,    /* Black wins by checkmate */
    STATUS_RESIGN_WHITE_WINS,       /* White wins by resignation */
    STATUS_RESIGN_BLACK_WINS,       /* Black wins by resignation */

    STATUS_DRAW_STALEMATE,          /* Stalemate (no legal moves and not in check) */
    STATUS_DRAW_FIFTY_MOVES,        /* 50-move rule draw (halfmove_clock reaches 100) */
    STATUS_DRAW_REPETITION,         /* Threefold repetition draw */
    STATUS_DRAW_INSUFFICIENT_MATERIAL /* Insufficient material to deliver checkmate (e.g., king vs. king) */
} ChessGameStatus;

/* Represents the context of the chess game */
typedef struct {
    PlayerInfo white_player;    /* Information about the white player */
    PlayerInfo black_player;    /* Information about the black player */
    ChessPiece board[8][8];     /* 8x8 chess board */
    PieceColor active_turn;     /* Current player's turn (COLOR_WHITE or COLOR_BLACK) */
    
    ChessTheme theme;           /* Current chess board theme */

    uint8_t castling_rights;    /* Castling rights (bitmask) */
    int en_passant_file;        /* File of the en passant target (-1 if none) */
    int halfmove_clock;         /* Halfmove clock for the 50-move rule */
    int fullmove_number;        /* Full move number */

    ChessGameStatus status;     /* Current game status */

    struct ListHead move_history; /* History of moves (if needed) */
} ChessGameContext;

/* Initializes the default theme for the chess board */
void chess_init_default_theme(/* Not NULL */ChessTheme *theme);
/*
 * Initializes the chess game state to the standard starting position
 * If theme is not set, it will be initialized to the default theme
 */
void chess_game_init(/* Not NULL */ChessGameContext *state);
/* Renders the chess game state onto the TUI context at the specified position */
void chess_game_render(/* Not NULL */TUIContext *ctx, /* Not NULL */ChessGameContext *state, int start_x, int start_y);
/* Checks if a move is valid */
bool chess_move_is_valid(/* Not NULL */const ChessGameContext *state, /* Not NULL */ChessMove *move);
/* Executes a move */
bool chess_execute_move(/* Not NULL */ChessGameContext *state, /* Not NULL */ChessMove *move);

/* Creates a new chess move */
ChessMove create_move(int from_x, int from_y, int to_x, int to_y, PieceType promotion);
/* Parses a string representation of a move */
bool chess_parse_move(/* Not NULL */const ChessGameContext *state, /* Not NULL */const char *move_str, size_t move_str_len, /* Not NULL */ChessMove *out_move);
/* Converts a chess move to a string representation */
bool chess_move_to_string(/* Not NULL */const ChessMove *move, /* Not NULL */char *buffer, size_t buffer_size);

size_t chess_generate_legal_moves(const ChessGameContext *state, struct ListHead *move_list);
void chess_free_move_list(struct ListHead *move_list);

#endif /* TCG_CHESS_H */