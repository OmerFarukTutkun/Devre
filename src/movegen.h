#ifndef _MOVEGEN_H_
#define _MOVEGEN_H_

#include "attack.h"

enum MoveTypes{
    QUIET                    = 0,
    DOUBLE_PAWN_PUSH         = 1,
    KING_CASTLE              = 2,
    QUEEN_CASTLE             = 3,
    CAPTURE                  = 4,
    EN_PASSANT               = 5,
    KNIGHT_PROMOTION         = 8,
    BISHOP_PROMOTION         = 9,
    ROOK_PROMOTION           = 10,
    QUEEN_PROMOTION          = 11,
    KNIGHT_PROMOTION_CAPTURE = 12,
    BISHOP_PROMOTION_CAPTURE = 13,
    ROOK_PROMOTION_CAPTURE   = 14,
    QUEEN_PROMOTION_CAPTURE  = 15,
};

enum MoveGenerationTypes{
    QSEARCH = 0, // Captures + Queen promotion
    ALL_MOVES = 1,
};

// 16-bit move
// 001100 011100  0000    -> e2e4 quiet move
//  from    to    type

#define NULL_MOVE 7
#define move_to(move )    (((move) >> 4) & 63)
#define move_from(move)   (((move) >> 10) & 63)
#define move_type(move)   ((move) & 15)

#define piece(move)   ((move >> 16) & 15)
#define create_move(from, to, type) ( (from)<<10 | (to)<<4 | (type))

#define is_promotion(move) ( ((move) >> 3) & ONE )
#define is_capture(move)   ( ((move) >> 2) & ONE )
#define is_tactical(move)  ( ((move) >> 2) & 3ull)
#define is_quiet(move)     ( !is_tactical(move)  )

int generate_moves(Position* pos, uint16_t* moves, int flag);
#endif