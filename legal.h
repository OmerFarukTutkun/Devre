#ifndef _LEGAL_H_
#define _LEGAL_H_
#include "attack.h"
//Todo: write is_legal function more efficiently by using we are in check or not.
// If we are in check, most moves are illegal. If we are not in check , most moves are legal.  
#define is_legal(pos) (!(is_square_attacked(pos ,PieceList[8*(!pos->side_to_move ) + KING][0] , pos->side_to_move)))
#endif