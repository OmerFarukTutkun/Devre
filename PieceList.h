
#ifndef _PIECELIST_H_
#define _PIECELIST_H_

#include "board.h"

uint8_t PieceList[15][MAX_PIECE_NUMBER]; 
uint64_t black_pawns= 0; // use bitboard for just pawns
uint64_t white_pawns = 0;
const uint64_t ONE = 1; 

void add_piece(uint8_t piece_type, uint8_t square) //add a piece to PieceList
{
    if(piece_type == PAWN)
    {
        uint8_t sq = mailbox[square] ;
        white_pawns = white_pawns | (ONE<<sq);
    }
    else if(piece_type == BLACK_PAWN)
    {
        uint8_t sq = mailbox[square] ;
        black_pawns = black_pawns | (ONE<<sq);
    }
    else
    {
        for(int i = 0; i < MAX_PIECE_NUMBER ; i++)
        {
            if(PieceList[piece_type][i] == 0) // if empty
            {
                PieceList[piece_type][i] = square;
                return;
            }
        }
    }
}
void delete_piece( uint8_t piece_type, uint8_t square)//delete a piece from PieceList
{
    if(piece_type == PAWN)
    {
        white_pawns = white_pawns ^ (ONE<< mailbox[square]);
    }
    else if(piece_type == BLACK_PAWN)
    {
        black_pawns = black_pawns ^ (ONE << mailbox[square]);
    }
    else
    {
     for(int i = 0; i< MAX_PIECE_NUMBER ; i++)
    {
            if( PieceList[piece_type][i] == square)
            {
                for( int j=i ; j < MAX_PIECE_NUMBER -1 ; j++) // delete the piece and move other elements to the left
                {
                    if(PieceList[piece_type][j+1])
                        PieceList[piece_type][j] = PieceList[piece_type][j+1];
                    else
                    {
                        PieceList[piece_type][j] = 0;
                        return;
                    }
                        
                }
                PieceList[piece_type][MAX_PIECE_NUMBER - 1] = 0;
            }
        }
    }
}
void change_piece(uint8_t piece_type , uint8_t from , uint8_t to )// change the square of a piece in PieceList
{
    if(piece_type == PAWN)
    {
         from = mailbox[from];
         to = mailbox [ to] ;
        white_pawns = white_pawns ^ (ONE<<from);
        white_pawns = white_pawns  | (ONE<< to); 
    }
    else if(piece_type == BLACK_PAWN)
    {
         from = mailbox[from];
         to = mailbox [ to] ;
        black_pawns = black_pawns ^ (ONE<<from);
        black_pawns = black_pawns  | (ONE<< to);
    }
    else
    {
     for(int i = 0; i< MAX_PIECE_NUMBER ; i++)
        {
            if( PieceList[piece_type][i] == from)
            {
                PieceList[piece_type][i] = to;
                return ;
            }
        }
    }
}
void set_PieceList(Position* pos)// initialize PieceList 
{
    white_pawns=0;
    black_pawns=0;
    memset(PieceList , 0 ,15*MAX_PIECE_NUMBER*sizeof(uint8_t));
    for(int k = 21; k<99 ;k++)
    {
        if(pos->board[k] != EMPTY)
            add_piece(pos->board[k] , k);
    }
}
#endif
