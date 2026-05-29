#include "chess_game.h"
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

char32_t get_piece_char(ChessPiece piece) {
switch (piece.type) {
            case PIECE_KING:   return 0x265A;
            case PIECE_QUEEN:  return 0x265B;
            case PIECE_ROOK:   return 0x265C;
            case PIECE_BISHOP: return 0x265D;
            case PIECE_KNIGHT: return 0x265E;
            case PIECE_PAWN:   return 0x265F;
            default: return ' ';
        }
    return ' ';
}

void chess_init_default_theme(ChessTheme *theme) {
    theme->light_square = 0xEDD9B5; // 仿木质浅色格 RGB
    theme->dark_square  = 0xB58863; // 仿木质深色格 RGB
    theme->white_piece  = 0xFFFFFF; // 白色棋子
    theme->black_piece  = 0x1A1A1A; // 黑色棋子
    theme->text_color   = 0x888888; // 边缘坐标标签颜色
    theme->highlight_bg = 0x7A9A60; // 选中/高亮时的绿色背景
    theme->checkmate_bg = 0xCC3333; // 将死时的红色背景
}

void chess_game_init(ChessGameState *state) {
    // 1. 先把棋盘全部清空
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            state->board[r][c] = (ChessPiece){PIECE_NONE, COLOR_NONE};
        }
    }

    // 2. 放置黑方重子 (第 0 行) 和 白方重子 (第 7 行)
    PieceType backline[] = {PIECE_ROOK, PIECE_KNIGHT, PIECE_BISHOP, PIECE_QUEEN, PIECE_KING, PIECE_BISHOP, PIECE_KNIGHT, PIECE_ROOK};
    for (int c = 0; c < 8; c++) {
        state->board[0][c] = (ChessPiece){backline[c], COLOR_BLACK};
        state->board[7][c] = (ChessPiece){backline[c], COLOR_WHITE};
    }

    // 3. 放置双方的兵 (第 1 行和第 6 行)
    for (int c = 0; c < 8; c++) {
        state->board[1][c] = (ChessPiece){PIECE_PAWN, COLOR_BLACK};
        state->board[6][c] = (ChessPiece){PIECE_PAWN, COLOR_WHITE};
    }

    // 4. 初始化游戏规则状态
    state->active_turn = COLOR_WHITE; // 白方先手
    state->castling_rights = 0x0F;     // 双方均拥有双侧王车易位权 (用4位掩码表示)
    state->en_passant_file = -1;       // 暂无可以吃过路兵的列
    state->halfmove_clock = 0;
    state->fullmove_number = 1;

    // 5. 载入默认主题
    chess_init_default_theme(&state->theme);
}

void chess_game_render(TUIContext *ctx, ChessGameState *state, int start_x, int start_y) {
    ChessTheme *theme = &state->theme;

    // 1. 绘制 8x8 棋盘主体
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            // 根据行列奇偶性决定深浅格颜色
            TUIColor bg = ((r + c) % 2 == 0) ? theme->light_square : theme->dark_square;

            ChessPiece piece = state->board[r][c];
            TUIColor fg = (piece.color == COLOR_WHITE) ? theme->white_piece : theme->black_piece;
            char32_t p_char = get_piece_char(piece);

            // 计算当前格子的终端坐标 (每个格子宽 2 字符，高 1 字符)
            int tui_x = start_x + c * 2;
            int tui_y = start_y + r;

            // 渲染棋子（左侧放置棋子字符，右侧用空格填充以凑足 2 个字符的宽度）
            tui_put_cell(ctx, tui_x,     tui_y, p_char, fg, bg);
            tui_put_cell(ctx, tui_x + 1, tui_y, 0xFE0E,    fg, bg);
            tui_put_cell(ctx, tui_x + 1, tui_y, ' ',    fg, bg);
        }
    }

    // 2. 绘制边缘坐标标签 (例如棋盘左侧的 8-1，底部的 A-H)
    // 左侧数字标签 (1-8，注意国际象棋第 0 行对应的是数字 8)
    for (int r = 0; r < 8; r++) {
        char label[2] = { '8' - r, '\0' };
        tui_put_string(ctx, start_x - 2, start_y + r, label, theme->text_color, 0x1E1E1E); // 0 代表透明或默认背景
    }

    // 底部字母标签 (A-H)
    for (int c = 0; c < 8; c++) {
        char label[3] = { 'A' + c, ' ', '\0' }; // 留个空格对齐 2 字符宽
        tui_put_string(ctx, start_x + c * 2, start_y + 8, label, theme->text_color, 0x1E1E1E);
    }
}

static bool is_path_clear(const ChessPiece board[8][8], int fx, int fy, int tx, int ty);
static bool validate_piece_geometry(const ChessGameState *state, ChessMove move);
static bool find_king(const ChessGameState *state, PieceColor color, int *king_x, int *king_y);
static bool is_square_attacked(const ChessGameState *state, int target_x, int target_y, PieceColor attacker_color);

// ==========================================
// 核心公开接口：执行一步走法
// ==========================================
bool chess_execute_move(ChessGameState *state, ChessMove move) {
    // 1. 基础物理边界与元校验
    if (move.from_x < 0 || move.from_x > 7 || move.from_y < 0 || move.from_y > 7) return false;
    if (move.to_x < 0 || move.to_x > 7 || move.to_y < 0 || move.to_y > 7) return false;
    if (move.from_x == move.to_x && move.from_y == move.to_y) return false;

    ChessPiece moving_piece = state->board[move.from_y][move.from_x];
    if (moving_piece.type == PIECE_NONE || moving_piece.color != state->active_turn) return false;

    ChessPiece target_piece = state->board[move.to_y][move.to_x];
    if (target_piece.type != PIECE_NONE && target_piece.color == state->active_turn) return false;

    // 2. 棋子几何走法与特殊规则校验
    if (!validate_piece_geometry(state, move)) return false;

    // 3. 王车易位特殊路径校验（王不能“穿过”被攻击的格子）
    if (moving_piece.type == PIECE_KING && abs(move.to_x - move.from_x) == 2) {
        int step_x = (move.to_x > move.from_x) ? 1 : -1;
        PieceColor enemy = (state->active_turn == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;
        // 检查王当前位置、经过的位置、以及目的地是否处于被攻击状态
        if (is_square_attacked(state, move.from_x, move.from_y, enemy) ||
            is_square_attacked(state, move.from_x + step_x, move.from_y, enemy) ||
            is_square_attacked(state, move.to_x, move.to_y, enemy)) {
            return false;
        }
    }

    // 4. 【核心防御】国王安全模拟（King Safety Simulation）
    // 假设性执行这一步，看看走完后自己的王会不会被将军
    ChessGameState sandbox = *state; // 完美的浅拷贝（给 AI 用也是同理）
    
    // 在沙盒中执行移动
    bool is_en_passant = (moving_piece.type == PIECE_PAWN && move.from_x != move.to_x && target_piece.type == PIECE_NONE);
    if (is_en_passant) {
        sandbox.board[move.from_y][move.to_x] = (ChessPiece){PIECE_NONE, COLOR_NONE};
    }
    sandbox.board[move.to_y][move.to_x] = moving_piece;
    sandbox.board[move.from_y][move.from_x] = (ChessPiece){PIECE_NONE, COLOR_NONE};

    // 检查在沙盒里，自己的王是否暴露在敌方火力下
    int kx, ky;
    if (find_king(&sandbox, state->active_turn, &kx, &ky)) {
        PieceColor enemy = (state->active_turn == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;
        if (is_square_attacked(&sandbox, kx, ky, enemy)) {
            return false; // 非法走法！不能把国王送入火坑，拒绝执行
        }
    }

    // ==========================================
    // 5. 校验全部通过，正式在主状态上生效修改
    // ==========================================
    bool is_capture_or_pawn = (target_piece.type != PIECE_NONE) || (moving_piece.type == PIECE_PAWN);

    // 处理真实的吃过路兵物理消除
    if (is_en_passant && move.to_x == state->en_passant_file) {
        state->board[move.from_y][move.to_x] = (ChessPiece){PIECE_NONE, COLOR_NONE};
    }

    // 处理王车易位时“车”的物理移动
    if (moving_piece.type == PIECE_KING && abs(move.to_x - move.from_x) == 2) {
        int r_from_x = (move.to_x > move.from_x) ? 7 : 0;
        int r_to_x   = (move.to_x > move.from_x) ? 5 : 3;
        state->board[move.from_y][r_to_x] = state->board[move.from_y][r_from_x];
        state->board[move.from_y][r_from_x] = (ChessPiece){PIECE_NONE, COLOR_NONE};
    }

    // 物理落子
    state->board[move.to_y][move.to_x] = moving_piece;
    state->board[move.from_y][move.from_x] = (ChessPiece){PIECE_NONE, COLOR_NONE};

    // 兵的升变处理
    if (moving_piece.type == PIECE_PAWN && (move.to_y == 0 || move.to_y == 7)) {
        state->board[move.to_y][move.to_x].type = (move.promotion != PIECE_NONE) ? move.promotion : PIECE_QUEEN;
    }

    // 更新王车易位掩码 (角落的车或王动过，剥夺权力)
    if (move.from_y == 7 && move.from_x == 4) state->castling_rights &= ~0x03; // 白王动
    if (move.from_y == 0 && move.from_x == 4) state->castling_rights &= ~0x0C; // 黑王动
    if ((move.from_x == 0 && move.from_y == 7) || (move.to_x == 0 && move.to_y == 7)) state->castling_rights &= ~0x02; // 左下车
    if ((move.from_x == 7 && move.from_y == 7) || (move.to_x == 7 && move.to_y == 7)) state->castling_rights &= ~0x01; // 右下车
    if ((move.from_x == 0 && move.from_y == 0) || (move.to_x == 0 && move.to_y == 0)) state->castling_rights &= ~0x08; // 左上车
    if ((move.from_x == 7 && move.from_y == 0) || (move.to_x == 7 && move.to_y == 0)) state->castling_rights &= ~0x04; // 右上车

    // 更新过路兵标记
    if (moving_piece.type == PIECE_PAWN && abs(move.to_y - move.from_y) == 2) {
        state->en_passant_file = move.from_x;
    } else {
        state->en_passant_file = -1;
    }

    // 时钟更新与换边
    state->halfmove_clock = is_capture_or_pawn ? 0 : (state->halfmove_clock + 1);
    if (state->active_turn == COLOR_BLACK) {
        state->fullmove_number++;
        state->active_turn = COLOR_WHITE;
    } else {
        state->active_turn = COLOR_BLACK;
    }

    return true;
}

// ==========================================
// 内部辅助函数实现
// ==========================================

// 检查直线/对角线上是否有棋子挡路（不包含起点和终点）
static bool is_path_clear(const ChessPiece board[8][8], int fx, int fy, int tx, int ty) {
    int dx = (tx > fx) ? 1 : ((tx < fx) ? -1 : 0);
    int dy = (ty > fy) ? 1 : ((ty < fy) ? -1 : 0);
    int x = fx + dx;
    int y = fy + dy;
    while (x != tx || y != ty) {
        if (board[y][x].type != PIECE_NONE) return false;
        x += dx; y += dy;
    }
    return true;
}

// 核心几何校验：只管规则，不管国王死活
static bool validate_piece_geometry(const ChessGameState *state, ChessMove move) {
    ChessPiece p = state->board[move.from_y][move.from_x];
    int dx = abs(move.to_x - move.from_x);
    int dy = abs(move.to_y - move.from_y);
    ChessPiece target = state->board[move.to_y][move.to_x];

    switch (p.type) {
        case PIECE_KNIGHT:
            return (dx * dy == 2); // 1x2 或 2x1 完美的L型

        case PIECE_ROOK:
            if (dx != 0 && dy != 0) return false; // 必须是直线
            return is_path_clear(state->board, move.from_x, move.from_y, move.to_x, move.to_y);

        case PIECE_BISHOP:
            if (dx != dy) return false; // 必须是对角线
            return is_path_clear(state->board, move.from_x, move.from_y, move.to_x, move.to_y);

        case PIECE_QUEEN:
            if (dx != dy && dx != 0 && dy != 0) return false; // 直线或对角线
            return is_path_clear(state->board, move.from_x, move.from_y, move.to_x, move.to_y);

        case PIECE_KING:
            // 正常移动：任意方向移动一格
            if (dx <= 1 && dy <= 1) return true;
            
            // 王车易位逻辑
            if (dy == 0 && dx == 2) {
                if (p.color == COLOR_WHITE && move.from_y == 7 && move.from_x == 4) {
                    if (move.to_x == 6 && (state->castling_rights & 0x01)) // 王翼易位
                        return is_path_clear(state->board, 4, 7, 7, 7);
                    if (move.to_x == 2 && (state->castling_rights & 0x02)) // 后翼易位
                        return is_path_clear(state->board, 4, 7, 0, 7);
                }
                if (p.color == COLOR_BLACK && move.from_y == 0 && move.from_x == 4) {
                    if (move.to_x == 6 && (state->castling_rights & 0x04))
                        return is_path_clear(state->board, 4, 0, 7, 0);
                    if (move.to_x == 2 && (state->castling_rights & 0x08))
                        return is_path_clear(state->board, 4, 0, 0, 0);
                }
            }
            return false;

        case PIECE_PAWN: {
            int dir = (p.color == COLOR_WHITE) ? -1 : 1; // 白棋向上(-y)，黑棋向下(+y)
            int start_row = (p.color == COLOR_WHITE) ? 6 : 1;

            // 1. 直步前进
            if (move.from_x == move.to_x) {
                if (move.to_y == move.from_y + dir && target.type == PIECE_NONE) return true;
                if (move.from_y == start_row && move.to_y == move.from_y + 2 * dir) {
                    return (state->board[move.from_y + dir][move.from_x].type == PIECE_NONE && target.type == PIECE_NONE);
                }
            } 
            // 2. 斜步吃子
            else if (dx == 1 && move.to_y == move.from_y + dir) {
                if (target.type != PIECE_NONE) return true; // 常规吃子
                if (move.to_x == state->en_passant_file && move.to_y == (p.color == COLOR_WHITE ? 2 : 5)) return true; // 吃过路兵
            }
            return false;
        }
        default: return false;
    }
}

// 寻找国王坐标
static bool find_king(const ChessGameState *state, PieceColor color, int *king_x, int *king_y) {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (state->board[y][x].type == PIECE_KING && state->board[y][x].color == color) {
                *king_x = x; *king_y = y;
                return true;
            }
        }
    }
    return false;
}

// 判断某个格子是否在敌方攻击范围内（主要用于将军和王车易位判定）
static bool is_square_attacked(const ChessGameState *state, int target_x, int target_y, PieceColor attacker_color) {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (state->board[y][x].color == attacker_color) {
                ChessMove attack_move = { .from_x = x, .from_y = y, .to_x = target_x, .to_y = target_y, .promotion = PIECE_NONE };
                // 利用已有的几何校验函数，看这个敌方棋子能不能走到目标格
                if (validate_piece_geometry(state, attack_move)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool chess_parse_single_pgn(const ChessGameState *state, const char *pgn_str, int pgn_len, ChessMove *out_move) {
    // 安全边界检查：单步 PGN 长度不可能小于 2，也不可能大于 15（包含各种评注和升变）
    if (!pgn_str || pgn_len < 2 || pgn_len > 15) return false;

    PieceColor color = state->active_turn;
    int rank_line = (color == COLOR_WHITE) ? 7 : 0;

    // ==========================================
    // 步骤 1：利用长度直接安全判断王车易位
    // ==========================================
    if (pgn_len >= 5 && (strncmp(pgn_str, "O-O-O", 5) == 0 || strncmp(pgn_str, "0-0-0", 5) == 0)) {
        out_move->from_x = 4; out_move->from_y = rank_line;
        out_move->to_x = 2;   out_move->to_y = rank_line;
        out_move->promotion = PIECE_NONE;
        return true;
    }
    if (pgn_len >= 3 && (strncmp(pgn_str, "O-O", 3) == 0 || strncmp(pgn_str, "0-0", 3) == 0)) {
        // 注意：要防止 "O-O-O" 的前三个字符误触发 "O-O"
        if (pgn_len == 3 || pgn_str[3] == ' ' || pgn_str[3] == '+' || pgn_str[3] == '#') {
            out_move->from_x = 4; out_move->from_y = rank_line;
            out_move->to_x = 6;   out_move->to_y = rank_line;
            out_move->promotion = PIECE_NONE;
            return true;
        }
    }

    // ==========================================
    // 步骤 2：安全提取并清洗数据，存入本地栈缓冲区（带防御性上限）
    // ==========================================
    char clean[16];
    int len = 0;
    for (int i = 0; i < pgn_len; i++) {
        // 过滤无关的修饰符
        if (pgn_str[i] != '+' && pgn_str[i] != '#' && pgn_str[i] != '?' && pgn_str[i] != '!') {
            clean[len++] = pgn_str[i];
        }
    }
    clean[len] = '\0'; // 本地缓冲区加上 \0 方便后面统一处理
    if (len < 2) return false;

    // ==========================================
    // 步骤 3：解析兵的升变 (如 "e8=Q" 或 "e8Q")
    // ==========================================
    PieceType promo_type = PIECE_NONE;
    if (len >= 4 && clean[len - 2] == '=') {
        char p = clean[len - 1];
        if (p == 'Q') promo_type = PIECE_QUEEN;
        else if (p == 'R') promo_type = PIECE_ROOK;
        else if (p == 'B') promo_type = PIECE_BISHOP;
        else if (p == 'N') promo_type = PIECE_KNIGHT;
        len -= 2; clean[len] = '\0';
    } else if (len >= 3 && isupper(clean[len - 1]) && isdigit(clean[len - 2])) {
        char p = clean[len - 1];
        if (p == 'Q') promo_type = PIECE_QUEEN;
        else if (p == 'R') promo_type = PIECE_ROOK;
        else if (p == 'B') promo_type = PIECE_BISHOP;
        else if (p == 'N') promo_type = PIECE_KNIGHT;
        len -= 1; clean[len] = '\0';
    }

    // ==========================================
    // 步骤 4：提取目标格坐标（此时必然位于清洗后字符串的末尾两位）
    // ==========================================
    char to_file = clean[len - 2];
    char to_rank = clean[len - 1];
    if (to_file < 'a' || to_file > 'h' || to_rank < '1' || to_rank > '8') return false;

    int to_x = to_file - 'a';
    int to_y = '8' - to_rank;

    // ==========================================
    // 步骤 5：解析棋子类型及消除歧义标识 (Disambiguation)
    // ==========================================
    PieceType piece_type = PIECE_PAWN;
    int prefix_start = 0;
    
    if (isupper(clean[0])) {
        if (clean[0] == 'N')      piece_type = PIECE_KNIGHT;
        else if (clean[0] == 'B') piece_type = PIECE_BISHOP;
        else if (clean[0] == 'R') piece_type = PIECE_ROOK;
        else if (clean[0] == 'Q') piece_type = PIECE_QUEEN;
        else if (clean[0] == 'K') piece_type = PIECE_KING;
        prefix_start = 1;
    }

    int dis_x = -1;
    int dis_y = -1;
    for (int i = prefix_start; i < len - 2; i++) {
        if (clean[i] == 'x') continue; // 忽略吃子标记
        if (clean[i] >= 'a' && clean[i] <= 'h') {
            dis_x = clean[i] - 'a';
        } else if (clean[i] >= '1' && clean[i] <= '8') {
            dis_y = '8' - clean[i];
        }
    }

    // ==========================================
    // 步骤 6：沙盒模拟，搜索唯一合法的起始格子
    // ==========================================
    int match_count = 0;
    ChessMove final_move = {0};

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (state->board[y][x].type == piece_type && state->board[y][x].color == color) {
                if (dis_x != -1 && x != dis_x) continue;
                if (dis_y != -1 && y != dis_y) continue;

                ChessMove candidate = { .from_x = x, .from_y = y, .to_x = to_x, .to_y = to_y, .promotion = promo_type };

                // 试走校验
                ChessGameState sandbox = *state;
                if (chess_execute_move(&sandbox, candidate)) {
                    final_move = candidate;
                    match_count++;
                }
            }
        }
    }

    if (match_count == 1) {
        *out_move = final_move;
        return true;
    }

    return false; // 找不到落子点，或者走法存在多义性
}

ChessMove chess_generate_ai_move(const ChessGameState *state) {
    ChessMove legal_moves[256]; // 国际象棋单回合最大合法走法一般不超过 218 种
    int move_count = 0;
    PieceColor ai_color = state->active_turn;

    // 初始化随机数种子（如果 main 里没加的话）
    static bool seeded = false;
    if (!seeded) { srand(time(NULL)); seeded = true; }

    // 1. 遍历整张棋盘，寻找属于 AI 阵营的棋子
    for (int fy = 0; fy < 8; fy++) {
        for (int fx = 0; fx < 8; fx++) {
            if (state->board[fy][fx].color == ai_color) {
                
                // 2. 遍历所有可能的目标格子
                for (int ty = 0; ty < 8; ty++) {
                    for (int tx = 0; tx < 8; tx++) {
                        
                        // 组装一个候选走法
                        ChessMove candidate = { .from_x = fx, .from_y = fy, .to_x = tx, .to_y = ty, .promotion = PIECE_NONE };
                        
                        // 如果是兵到了底线，AI 默认直接升变为“后”
                        if (state->board[fy][fx].type == PIECE_PAWN && (ty == 0 || ty == 7)) {
                            candidate.promotion = PIECE_QUEEN;
                        }

                        // 3. 利用沙盒规则引擎，测试这一步是不是合法的
                        ChessGameState sandbox = *state;
                        if (chess_execute_move(&sandbox, candidate)) {
                            // 合法，记录下来
                            legal_moves[move_count++] = candidate;
                            if (move_count >= 256) break;
                        }
                    }
                }

            }
        }
    }

    // 如果没有合法走法，返回一个全 0 的空结构体，代表游戏结束
    if (move_count == 0) {
        return (ChessMove){0, 0, 0, 0, PIECE_NONE};
    }

    // 4. 从所有合法的走法中，随机挑选一步
    int choice = rand() % move_count;
    return legal_moves[choice];
}