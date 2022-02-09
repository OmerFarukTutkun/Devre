#ifndef _HASH_H_
#define _HASH_H_

#include "board.h"
#define TT_ALPHA 1
#define TT_BETA 2
#define TT_EXACT 3
#define TT_NODE_TYPE 3
#define TT_OLD 4
#define TT_NEW 8
#define MOVE_TYPE  15 // binary form : 0000000000001111 
#define FROM 1008// binary form :      0000001111110000 
#define TO   64512 // binary form:     1111110000000000

uint64_t PieceKeys[15][120];// PieceKeys[0][sq] is used for en-passant
uint64_t SideToPlayKey;
uint64_t CastlingKeys[16];
int  HASH_SIZE = 1048576; // 1024 x 1024 entry -> 16MB hash size
int  HASH_SHIFT = 44;


typedef struct TTentry{ //16byte per entry. There is one byte room , i will add something 
    uint64_t key;
    int16_t score;
    uint8_t depth;
    char flag;
    uint16_t move;
    uint8_t hit;
}TTentry;

TTentry* hash_table;

uint64_t rand_u64()
{
    static uint64_t seed= 1442695040888963407; 
    const uint64_t a = 6364136223846793005; 
    seed *= a;
    return seed;
}
typedef struct{
    uint64_t position_keys[4*MAX_DEPTH];
    uint64_t n;
}position_history;
position_history pos_history;

void init_keys()
{
    for(int i=0 ; i<15 ; i++)
    {
        for(int j=0 ; j<120 ;j++)
        {
            PieceKeys[i][j] = rand_u64();
        }
    }
    SideToPlayKey = rand_u64();
    for(int i = 0; i<16 ;i++)
        CastlingKeys[i] = rand_u64();
}

void initZobristKey(Position* pos)
{
    pos->key = 0;
    for(int i=0 ; i< 120; i++ )
    {
            if(pos->board[i] != OFF && pos->board[i] != EMPTY)
            {
                pos->key ^= PieceKeys[ pos->board[i] ][i];
            }
    }
    if(pos ->side_to_move == 1)
       pos->key ^= SideToPlayKey;
    if(pos-> en_passant > 0)
       pos->key ^= PieceKeys[0][pos-> en_passant];

    pos->key ^= CastlingKeys[ pos->castlings];
}
void updateHashTable(Position* pos , int score ,char flag,uint8_t depth,uint16_t move)
{
    //Todo: write other replecement schemes and test them.
    int ind =(pos->key<<HASH_SHIFT)>>HASH_SHIFT;
    if( hash_table[ind].key == 0  
    || hash_table[ind].depth < depth
    || (hash_table[ind].depth == depth  && (flag==TT_EXACT || (hash_table[ind].flag & TT_NODE_TYPE) != TT_EXACT) ))
    {
        if( score >  13900)
            score += pos->ply;
        else if( score < -13900)
            score -= pos->ply;
        if(pos->key != hash_table[ind].key)
            hash_table[ind].hit= 0;
        hash_table[ind].key = pos->key;
        hash_table[ind].score = score;
        hash_table[ind].depth = depth;
        hash_table[ind].flag = flag + TT_NEW;
        hash_table[ind].move = move;
    }
}
#endif
