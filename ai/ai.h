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
 * File: ai.h
 * 
 * Description:
 *      Defines the AI logic for the chess game.
 * 
 * This file is part of TCG.
 */

#ifndef TCG_AI_H
#define TCG_AI_H

#include "chess/chess.h"

typedef struct {
    int difficulty_level;
    int search_depth;
    int time_limit_ms;
} AIConfig;

typedef struct AIContext AIContext;

AIContext *ai_create(const AIConfig config);
void ai_destroy(AIContext *ctx);

void ai_find_best_move(AIContext *context, const ChessGameContext *state, ChessMove *best_move);

#endif /* TCG_AI_H */