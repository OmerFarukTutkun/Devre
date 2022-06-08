#ifndef _TT_H_
#define _TT_H_

#include "board.h"
#include "search.h"

#define TT_ALPHA 1u
#define TT_BETA 2u
#define TT_EXACT 3u
#define TT_NODE_TYPE 3u
#define TT_OLD 4u
#define TT_NEW 8u

extern uint64_t PieceKeys[12][64];
extern uint64_t SideToPlayKey;
extern uint64_t CastlingKeys[16];
extern uint64_t EnPassantKeys[64];

typedef struct TTentry{
    uint64_t key;
    int16_t score;
    uint8_t depth;
    char flag;
    uint16_t move;
    uint8_t hit;
}TTentry;

uint64_t rand_u64();
void init_keys();
void initZobristKey(Position* pos);
void tt_save(Position* pos , int score ,char flag,uint8_t depth,uint16_t move);
TTentry* tt_probe(uint64_t key);
void tt_init(int megabyte);
void tt_clear();
void tt_free();
void tt_clear_old_entries();
#endif