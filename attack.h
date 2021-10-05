#ifndef _ATTACK_H_
#define _ATTACK_H_
#include "board.h"
#include "PieceList.h"
#define TRUE 1
#define FALSE 0

int offset[6][8] = {
	{   0,   0,  0,  0, 0,  0,  0,  0 },
	{ -21, -19,-12, -8, 8, 12, 19, 21 }, // KNIGHT 
	{ -11,  -9,  9, 11, 0,  0,  0,  0 }, // BISHOP 
	{ -10,  -1,  1, 10, 0,  0,  0,  0 }, // ROOK 
	{ -11, -10, -9, -1, 1,  9, 10, 11 }, // QUEEN 
	{ -11, -10, -9, -1, 1,  9, 10, 11 }  // KING 
};
 
 //turn vector to unit vector
 int8_t vector_to_unit_vector[155] = 
{ 
-11 ,  0 ,  0 ,  0 ,  0 , -9 ,  0 ,-10 ,  0 ,  0 ,  0 ,-11 ,  0 ,  0 , -9 ,  0 ,  0 ,-10 ,  0 ,  0 ,  0 ,  0 ,-11 , -9 ,  0 ,  0 ,  0 ,-10 ,  0 ,  0 ,
  0 ,  0 , -9 ,-11 ,  0 ,  0 ,  0 ,-10 ,  0 ,  0 ,  0 , -9 ,  0 ,  0 ,-11 ,  0 ,  0 ,-10 ,  0 ,  0 , -9 ,  0 ,  0 ,  0 ,  0 ,-11 ,  0 ,-10 ,  0 , -9 ,
  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,-11 ,-10 , -9 ,  0 , -1 , -1 , -1 , -1 , -1 , -1 , -1 ,  0 ,  1 ,  1 ,  1 ,  1 ,  1 ,  1 ,  1 ,  0 ,  9 , 10 , 11 ,  0 ,
  0 ,  0 ,  0 ,  0 ,  0 ,  9 ,  0 , 10 ,  0 , 11 ,  0 ,  0 ,  0 ,  0 ,  9 ,  0 ,  0 , 10 ,  0 ,  0 , 11 ,  0 ,  0 ,  9 ,  0 ,  0 ,  0 , 10 ,  0 ,  0 ,
  0 , 11 ,  9 ,  0 ,  0 ,  0 ,  0 , 10 ,  0 ,  0 ,  0 ,  9 , 11 ,  0 ,  0 ,  0 ,  0 , 10 ,  0 ,  0 ,  9 ,  0 ,  0 , 11 ,  0 ,  0 ,  0 , 10 ,  0 ,  9 ,
  0 ,  0 ,  0 ,  0 , 11};

 //is_move_possible[KNIGHT-1][vector + 77] will tell us if vector is knight move or not
 // it is used to fasten is_square_attacked.
 uint8_t is_move_possible[4][155] = {
{
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,  
0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1,  
0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  
0, 0, 0, 0, 0 },
{
1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 
0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1,  
0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,  
0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0,  
0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1,  
0, 0, 0, 0, 1 },
{
0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,  
0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0,  
0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,  
0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,  
0, 0, 0, 0, 0 },
{
1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 
0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1,  
0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 
0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 
0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 
0, 0, 0, 0, 1 }};

int is_square_attacked(Position* pos, uint8_t square,int side ) 
{
    uint8_t enemy;
    int8_t unit_vector;
    uint8_t enemy_king=B_KING,enemy_knight=B_KNIGHT;
    int i = 0;
    if(! side)
    {
        if( pos -> board[square -9] == PAWN || pos -> board[square - 11] == PAWN)// if enemy pawn can capture our piece return TRUE
            return TRUE;
        enemy_king = KING;
        enemy_knight = KNIGHT;
    }
    else
    {
        if( pos -> board[square +9] == B_PAWN || pos -> board[square + 11] == B_PAWN)
            return TRUE;
    }
    switch(square - PieceList[enemy_king][0] ) // if enemy king can capture our piece return TRUE
    {
        case 1: case 9: case 10: case 11: case -1: case -9 : case -11: case -10:
            return TRUE;
        default: break;
    }
    for(int j=enemy_knight ; j<=enemy_knight+3 ;j++)// enemy knights, bishops, rooks, queens
    {
        for( i = 0 ; i < MAX_PIECE_NUMBER ;i++) 
        {
            enemy= PieceList[j][i];
            if(enemy) // is there a piece
            {
                if(is_move_possible[j-enemy_knight][enemy -square +77])
                {
                    if(j == enemy_knight)
                        return TRUE;
                    unit_vector = vector_to_unit_vector[square - enemy +77]; // unit vector from enemy piece to our square
                    while(TRUE)
                    {
                        enemy += unit_vector;
                        if(enemy == square ) 
                            return TRUE;
                        if( pos -> board[enemy] != EMPTY)
                        {
                            break;
                        }
                    }
                }
            }
            else
                break;
        }
    }
    return FALSE;    
}
#endif