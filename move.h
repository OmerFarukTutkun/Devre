#ifndef _MOVE_H_
#define _MOVE_H_
#include "movegen.h"

#define Mil 1000000

typedef struct MoveList{
    uint16_t moves[256];
    int move_scores[256];
    int num_of_moves;
    int score_of_move;
}MoveList;

void print_move(uint16_t move);
uint16_t string_to_move(Position* pos ,char* str);
void make_move(Position* pos, uint16_t move);
void unmake_move(Position* pos,  uint16_t move);
uint16_t pick_move(MoveList* move_list);
void score_moves(Position* pos, MoveList* move_list, uint16_t hash_move, int flag);
int calculate_SEE(Position* pos, uint16_t move);
#endif
