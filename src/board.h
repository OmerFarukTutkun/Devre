#ifndef _BOARD_H_
#define _BOARD_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>
#include "bitboards.h"
#define STARTING_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

#define MAX_DEPTH 100 

#define TRUE 1
#define FALSE 0

#define  WHITE_SHORT_CASTLE  1
#define  WHITE_LONG_CASTLE   2 
#define  BLACK_SHORT_CASTLE  4
#define  BLACK_LONG_CASTLE   8
#define  WHITE_CASTLE        3
#define  BLACK_CASTLE        12

typedef struct UnMake_Info
{
    //store the information we cannot get back
    int8_t   en_passant;
    uint8_t  castlings;
    uint8_t  captured_piece;
    uint8_t  half_move;
    uint64_t key;
    uint64_t threat;
}UnMake_Info;

typedef struct Position_History{
    uint64_t keys[2*MAX_DEPTH];
    int index;
}Position_History;

typedef struct Position{
    uint8_t   board[64] ;
    uint64_t  bitboards[12];
    uint64_t  occupied[2];
    uint8_t   castlings, side,  half_move, castling_rooks[4]; 
    int8_t    en_passant;
    uint16_t  full_move;
    uint64_t  key; 
    uint64_t  threat;
    uint64_t nodes;
    uint16_t  bestmove;
    uint8_t   ply;
    int16_t   history[2][2][2][64][64];   // history table for ordering quiet moves
    int16_t   conthist[2][6][64][6][64]; 
    int16_t   followuphist[2][6][64][6][64]; 
    int16_t   capturehist[2][6][64][6];
    uint16_t  killers[MAX_DEPTH][2]; // 2 killer moves per ply for move ordering
    uint16_t  counter_moves[2][64][64]; // a counter move for move ordering

    uint8_t   accumulator_cursor[2*MAX_DEPTH];
    uint8_t   piece_count;
    int16_t   evals[MAX_DEPTH];
    UnMake_Info unmakeStack[2*MAX_DEPTH];
    uint32_t move_history[2*MAX_DEPTH];
    Position_History pos_history;
 }Position;

//piece values for mvv-lva
static const int piece_values[12] = {100,300,325,500,1000,1500, 100,300,325,500,1000,1500};

long long timeInMilliseconds(void);
void push(Position* pos , uint8_t capture);
void add_piece(Position* pos, uint8_t piece_type, uint8_t sq) ;
void remove_piece(Position* pos,  uint8_t piece_type, uint8_t sq) ;
void move_piece(Position* pos, uint8_t piece_type, uint8_t from , uint8_t to );
void fen_to_board ( Position* pos , const char* fen);
void board_to_fen(Position* pos, char* fen);
void print_Board(Position* pos );
bool has_non_pawn_pieces(Position* pos);
bool is_draw(Position* pos);

#endif
