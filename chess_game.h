#ifndef TCG_CHESS_GAME_H
#define TCG_CHESS_GAME_H

#include "tui/tui.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    PIECE_NONE = 0,
    PIECE_PAWN,   // 兵
    PIECE_KNIGHT, // 马
    PIECE_BISHOP, // 象
    PIECE_ROOK,   // 车
    PIECE_QUEEN,  // 后
    PIECE_KING    // 王
} PieceType;

typedef enum {
    COLOR_NONE = 0,
    COLOR_WHITE,
    COLOR_BLACK
} PieceColor;

typedef struct {
    PieceType type;
    PieceColor color;
} ChessPiece;

typedef struct {
    TUIColor light_square; // 浅色格子颜色
    TUIColor dark_square;  // 深色格子颜色
    TUIColor white_piece;  // 白方棋子颜色
    TUIColor black_piece;  // 黑方棋子颜色
    TUIColor text_color;   // 坐标标签颜色
    TUIColor highlight_bg; // 选中格子背景色
    TUIColor checkmate_bg;  // 将死格子背景色
} ChessTheme;

typedef struct {
    ChessPiece board[8][8]; // 8x8 棋盘
    PieceColor active_turn; // 当前轮到谁走 (COLOR_WHITE 或 COLOR_BLACK)
    
    ChessTheme theme;       // 当前棋盘主题配色方案

    // 国际象棋特殊规则状态（后续实现高级功能必用）
    uint8_t castling_rights; // 王车易位权利 (用位掩码表示白K、白Q、黑K、黑Q)
    int en_passant_file;     // 可被“吃过路兵”的列坐标 (-1 表示没有)
    int halfmove_clock;      // 50步和棋规则计数器
    int fullmove_number;     // 当前回合数
} ChessGameState;

typedef struct {
    int from_x, from_y; // 起始格子坐标
    int to_x, to_y;     // 目的格子坐标
    PieceType promotion;// 兵升变时的目标棋子类型（默认 PIECE_NONE）
} ChessMove;

void chess_game_init(ChessGameState *state);
void chess_game_render(TUIContext *ctx, ChessGameState *state, int start_x, int start_y);
bool chess_execute_move(ChessGameState *state, ChessMove move);
bool chess_parse_single_pgn(const ChessGameState *state, const char *pgn_str, int pgn_len, ChessMove *out_move);
ChessMove chess_generate_ai_move(const ChessGameState *state);

#endif /* TCG_CHESS_GAME_H */