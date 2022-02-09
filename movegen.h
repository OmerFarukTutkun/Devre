#ifndef _MOVEGEN_H_
#define _MOVEGEN_H_

#include "PieceList.h"
#include "board.h"
#include "attack.h"
#include "hash.h"
#define QUIET_MOVE 0 
#define DOUBLE_PAWN_PUSH 1 
#define SHORT_CASTLE 2
#define LONG_CASTLE 3
#define CAPTURE 4
#define ENPASSANT   5
#define KNIGHT_PROMOTION 8
#define BISHOP_PROMOTION 9
#define ROOK_PROMOTION 10
#define QUEEN_PROMOTION 11
#define KNIGHT_PROMOTION_CAPTURE 12
#define BISHOP_PROMOTION_CAPTURE 13
#define ROOK_PROMOTION_CAPTURE 14
#define QUEEN_PROMOTION_CAPTURE 15 
#define NULL_MOVE 7

#define MOVE_TYPE  15 // binary form : 0000000000001111 
#define FROM 1008// binary form :      0000001111110000 
#define TO   64512 // binary form:     1111110000000000
#define CAPTURE 4 // binary form :     0000000000000100
#define PROMOTION 8
#define MAX_MOVES_NUMBER 228
#define TRUE 1
#define FALSE 0
#define move_to(move )   mailbox64[((move & TO)>>10)]
#define move_from(move ) mailbox64[((move & FROM)>>4)]
#define move_type(move)  ( (move) & MOVE_TYPE)
    /* least 4 bit for move-type ,  6 bit = from  , 6 bit = to
                            MOVE TYPE
    code 	promotion 	capture 	special 1 	special 0 	kind of move
    0       	0 	    0         	0           	0       	quiet moves
    1       	0 	    0         	0           	1       	double pawn push
    2       	0 	    0         	1           	0       	king castle
    3       	0 	    0         	1           	1       	queen castle
    4       	0 	    1         	0           	0       	captures
    5       	0 	    1         	0           	1       	ENPASSANT-capture
    7           0       1           1               1           null-move
    8       	1 	    0         	0           	0       	knight-promotion
    9       	1 	    0         	0           	1       	bishop-promotion
    10       	1 	    0         	1           	0       	rook-promotion
    11       	1 	    0         	1           	1       	queen-promotion
    12       	1 	    1         	0           	0       	knight-promo capture
    13       	1 	    1         	0           	1       	bishop-promo capture
    14       	1 	    1         	1           	0       	rook-promo capture
    15       	1 	    1         	1           	1       	queen-promo capture 
    
    taken from https://www.chessprogramming.org */

int generate_moves(Position* pos, uint16_t* moves);
uint16_t create_move(uint8_t from, uint8_t to , uint8_t move_type)
{
   // we cannot store 12x10 board's index in 6bit, transform 8 bit to 6 bit with mailbox array. For example 21 -> 0 or 94 -> 59 
    return move_type + (mailbox[from] <<4) + (mailbox [to ]<<10);
}
void print_move(uint16_t move)
{
    
    uint8_t from = move_from(move);
    uint8_t to = move_to(move);
    printf("%s%s" ,mailbox_to_fen[from] , mailbox_to_fen[to]);
    if(move_type(move) >= KNIGHT_PROMOTION)
    {
        switch(move & MOVE_TYPE)
        {
            case KNIGHT_PROMOTION: case KNIGHT_PROMOTION_CAPTURE: printf("n"); break;
            case BISHOP_PROMOTION: case BISHOP_PROMOTION_CAPTURE: printf("b"); break;
            case ROOK_PROMOTION:   case ROOK_PROMOTION_CAPTURE: printf("r"); break;
            case QUEEN_PROMOTION:  case QUEEN_PROMOTION_CAPTURE: printf("q"); break;
        }
    }
    printf(" ");
}
uint16_t string_to_move(Position* pos ,char* str)
{
    uint16_t moves[MAX_MOVES_NUMBER];
    int number_of_moves = generate_moves(pos,moves);
    uint8_t from = ('h' - str[0]) + 8*(str[1] - '1');
    uint8_t to =   ('h' - str[2]) + 8*(str[3] - '1');
    int i;
    for ( i=0; i< number_of_moves ; i++ )
    {
        if ( from == (moves[i] & FROM )>>4  && to ==( moves[i] & TO )>>10)
        {
            if((moves[i] & MOVE_TYPE) > 7)//promotion
            {
                switch(str[4])
                {
                    case 'n': case 'N': return moves[i];
                    case 'b': case 'B': return moves[i+1];
                    case 'r': case 'R': return moves[i+2];
                    case 'q': case 'Q':return moves[i+3];
                    default: break;
                }
                return moves[i];
            }
            else
            {
                return moves[i];
            }
        }
    }
    return 0;
}
int generate_moves(Position* pos, uint16_t* moves)
{
    uint8_t from;
    uint8_t to;
  int number_of_moves= 0;  
    if(pos ->side_to_move)// black to move
    {
        switch (pos->castlings & BLACK_CASTLE ) // castlings
        {
            case 12: 
            if(
                pos->board[a8] == BLACK_ROOK && pos->board[e8] ==BLACK_KING && pos->board [d8] == EMPTY && pos->board [c8] == EMPTY 
                && pos->board [b8] == EMPTY && !is_square_attacked(pos , e8 , 0) && !is_square_attacked(pos , d8 , 0))
                    moves[number_of_moves++] = create_move(e8, c8 , LONG_CASTLE );
            case 4: 
            if(pos->board[h8] == BLACK_ROOK && pos->board[e8] ==BLACK_KING &&
                pos->board [f8] == EMPTY && pos->board [g8] == EMPTY &&! is_square_attacked(pos , e8 , 0) && !is_square_attacked(pos , f8 ,0))
                    moves[number_of_moves++] = create_move(e8, g8 , SHORT_CASTLE );
            break;
            case 8:
            if( pos->board[a8] == BLACK_ROOK &&  pos->board[e8] ==BLACK_KING &&pos->board [d8] == EMPTY && pos->board [c8] == EMPTY 
            && pos->board [b8] == EMPTY && !is_square_attacked(pos , e8 ,0) && !is_square_attacked(pos , d8 , 0))
                    moves[number_of_moves++] = create_move(e8, c8 , LONG_CASTLE );
            break;
        }


        if(pos->en_passant > 0)// ENPASSANTcapture
        {
            if(pos->board[ pos->en_passant +11 ]== BLACK_PAWN)
            {
                moves[number_of_moves++] = create_move(pos->en_passant +11, pos->en_passant , ENPASSANT);
            }
            if(pos->board[ pos->en_passant + 9 ]== BLACK_PAWN)
            {
                moves[number_of_moves++] = create_move(pos->en_passant +9, pos->en_passant , ENPASSANT);
            }
        }


        for(int j=BLACK_KNIGHT ; j<=BLACK_QUEEN ; j++)
        {
            for(int i=0 ; i < MAX_PIECE_NUMBER ; i++) //black-knights,bishops,rooks,queens
            {
                from = PieceList[j][i];
                if(from) // if there is a piece
                {
                    for(int k = 0 ; k < 8 ; k++)
                    {
                        to=from;
                        while(TRUE)
                        {
                         if(offset[j+1 -BLACK_KNIGHT][k] == 0)
                                break;
                            to += offset[j+1 -BLACK_KNIGHT][k] ;
                            if(pos->board[to] == EMPTY) 
                            {
                                moves[number_of_moves++] = create_move(from, to , QUIET_MOVE ); 
                            }
                            else 
                            {
                                switch (pos->board[to])
                                {
                                    case PAWN: case KNIGHT: case BISHOP: case ROOK: case QUEEN: 
                                    moves[number_of_moves++] = create_move(from, to , CAPTURE ); break;
                                }
                                break;
                            }
                            if(j == BLACK_KNIGHT )
                                break;
                        }
                    }
                }
                else
                    break;
            }
        }
        for(int j= 0; j < 8; j++) // black-king
        { 
             to = offset[5][j] + PieceList[BLACK_KING][0];
            switch (pos->board[to])
            {
                case PAWN: case ROOK: case KNIGHT:
                case QUEEN: case BISHOP: moves[number_of_moves++] = create_move(PieceList[BLACK_KING][0], to , CAPTURE ); break; //captures
                case EMPTY: moves[number_of_moves++] = create_move(PieceList[BLACK_KING][0], to , QUIET_MOVE); break; // quiet
            }
        }


        uint64_t temp = black_pawns;
        while(temp)
        {
            from = bitScanForward(temp);
            temp =  (temp>>(from+1)) << (from +1);
            from =mailbox64[from];
                uint8_t rank = from/10;
                 to = from -10; // 1 square ahead
                switch(rank)
                {
                    case 8: if(pos->board[to] == EMPTY && pos->board[to -10] == EMPTY )
                    {
                        moves[number_of_moves++] = create_move(from, to -10 , 1); //double pawn push
                    }
                    case 7: case 6: case 5: case 4:
                    if(pos->board[to] == EMPTY)
                        moves[number_of_moves++] = create_move(from, to , QUIET_MOVE);
                    to++; // look right
                    if(pos->board[to] >= PAWN && pos->board[to] <KING)
                        moves[number_of_moves++] = create_move(from, to , CAPTURE);
                      to -= 2; // look left
                    if(pos->board[to] >= PAWN && pos->board[to] <KING)
                        moves[number_of_moves++] = create_move(from, to , CAPTURE);       
                    break; 
                    case 3:
                    if(pos->board[to] == EMPTY)
                    {
                        for(int j= 8 ; j < 12 ; j++)
                            moves[number_of_moves++] = create_move(from, to , j);
                    }
                    to++; // look right
                    if(pos->board[to] >= PAWN && pos->board[to] <KING)
                    {
                        for(int j= 12 ; j < 16 ; j++)
                                moves[number_of_moves++] = create_move(from, to , j);
                    } 
                    to -= 2;
                    if(pos->board[to] >= PAWN && pos->board[to] <KING)
                    {
                        for(int j= 12 ; j < 16 ; j++)
                                moves[number_of_moves++] = create_move(from, to , j);
                    }  
                    break;                      
                }
             
        }
    }

    else//white to move
    {
        switch (pos->castlings & WHITE_CASTLE ) // castlings
        {
            case 3: 
            if( pos -> board[a1] == ROOK &&  pos->board [d1] == EMPTY && pos->board[e1] == KING &&pos->board [c1] == EMPTY &&
                 pos->board [b1] == EMPTY && ! is_square_attacked(pos , e1 , 1) && ! is_square_attacked(pos , d1 ,1))
                 moves[number_of_moves++] = create_move(e1, c1 , LONG_CASTLE );
            case 1:
            if( pos -> board[h1] == ROOK && pos->board[e1] == KING &&pos->board [f1] == EMPTY && pos->board [g1] == EMPTY && 
            !is_square_attacked(pos , e1 , 1) && !is_square_attacked(pos , f1 , 1))
                    moves[number_of_moves++] = create_move(e1, g1 , SHORT_CASTLE);
            break;
            case 2:
            if(
                pos -> board[a1] == ROOK &&  pos->board [d1] == EMPTY && pos->board[e1] == KING &&pos->board [c1] == EMPTY &&
                 pos->board [b1] == EMPTY && ! is_square_attacked(pos , e1 , 1) && ! is_square_attacked(pos , d1 ,1))
                 moves[number_of_moves++] = create_move(e1, c1 , LONG_CASTLE );
            break;
        }
        if(pos->en_passant > 0)// ENPASSANTcapture
        {
            if(pos->board[ pos->en_passant - 11 ] == PAWN)
            { 
            moves[number_of_moves++] = create_move(pos->en_passant -11, pos->en_passant , ENPASSANT);
            }

            if(pos->board[ pos->en_passant -9 ] == PAWN)
            {
                moves[number_of_moves++] = create_move(pos->en_passant -9, pos->en_passant , ENPASSANT);
            }
        }
        for(int j=KNIGHT ; j<=QUEEN ; j++)
        {
            for(int i=0 ; i < MAX_PIECE_NUMBER ; i++) //white-knights,bishops,rooks,queens
            {
                from = PieceList[j][i];
                if(from) // if there is a piece
                {
                    for(int k = 0 ; k < 8 ; k++)
                    {
                        to=from;
                        while(TRUE)
                        {
                            if(offset[j+1 -KNIGHT][k]  == 0)
                                break;
                            to += offset[j+1 -KNIGHT][k] ;
                            if(pos->board[to] == EMPTY) 
                            {
                                moves[number_of_moves++] = create_move(from, to , QUIET_MOVE ); 
                            }
                            else 
                            {
                                switch (pos->board[to])
                                {
                                    case BLACK_PAWN: case BLACK_KNIGHT: case BLACK_BISHOP: case BLACK_ROOK: case BLACK_QUEEN: 
                                    moves[number_of_moves++] = create_move(from, to , CAPTURE ); break;
                                }
                                break;
                            }
                            if(j == KNIGHT )
                                break;
                        }
                    }
                }
                else
                    break;
            }
        }
        for(int j= 0; j < 8; j++) // white-king
        { 
             to = offset[5][j] + PieceList[KING][0];
            switch (pos->board[to])
            {
               case BLACK_PAWN: case BLACK_ROOK : case BLACK_KNIGHT : case BLACK_QUEEN: case BLACK_BISHOP:
               moves[number_of_moves++] = create_move(PieceList[KING][0], to , CAPTURE ); break; //captures
                case EMPTY: moves[number_of_moves++] = create_move(PieceList[KING][0], to , QUIET_MOVE ); break; // quiet
            }
        }
        uint64_t temp = white_pawns;
        while(temp)
        {
            from = bitScanForward(temp);
            temp =  (temp>>(from + 1) )<<( from +1 );
            from =mailbox64[from];
            uint8_t rank = from/10;
                to = from + 10; // 1 square ahead
            switch(rank)
            {
                case 3: if(pos->board[to] == EMPTY && pos->board[to + 10] == EMPTY )
                {
                    moves[number_of_moves++] = create_move(from, to +10 , DOUBLE_PAWN_PUSH); //double pawn push
                }
                case 7: case 6: case 5: case 4:
                if(pos->board[to] == EMPTY)
                    moves[number_of_moves++] = create_move(from, to , QUIET_MOVE);
                to++; // look right
                if(pos->board[to] >= BLACK_PAWN && pos->board[to] < BLACK_KING )
                    moves[number_of_moves++] = create_move(from, to , CAPTURE);                    
                to -= 2; // look left
                if(pos->board[to] >= BLACK_PAWN && pos->board[to] < BLACK_KING)
                    moves[number_of_moves++] = create_move(from, to , CAPTURE);
                break; 
                case 8:
                    if(pos->board[to] == EMPTY)
                    {
                        for(int j= 8 ; j < 12 ; j++)
                            moves[number_of_moves++] = create_move(from, to , j);
                    }
                to++; // look right
                if(pos->board[to] >= BLACK_PAWN && pos->board[to] <BLACK_KING)
                {
                    for(int j= 12 ; j < 16 ; j++)
                            moves[number_of_moves++] = create_move(from, to , j);
                }
                to -= 2;
                if(pos->board[to] >= BLACK_PAWN  && pos->board[to] <BLACK_KING )
                {
                    for(int j= 12 ; j < 16 ; j++)
                            moves[number_of_moves++] = create_move(from, to , j);
                }
                break;
            }
         }
    }
    return number_of_moves;
}
void make_move(Position* pos, uint16_t move , Stack* stack)
{
    if(move == NULL_MOVE)//do null move
    {
        push( stack, pos, 0);
        pos->ply++;
        pos->side_to_move = 1- pos->side_to_move;
        pos->key ^= SideToPlayKey;
        pos->en_passant = 0;
        pos->half_move = 0;
        pos->last_move = NULL_MOVE;
        return ;
    }
    uint8_t from = move_from(move);
    uint8_t to   =move_to(move);
    uint8_t move_type = move_type(move);
    uint8_t piece_type = pos->board[from]; // moving piece
    uint8_t sq ; // for ENPASSANT
   
    uint8_t captured_piece = pos->board[to]; // Captured piece type 
    if(move_type == ENPASSANT)
    {
       captured_piece = PAWN + (!pos->side_to_move)*8;
    }
    push(stack, pos, captured_piece);
    pos->en_passant = 0;
    pos->half_move++; 
    pos->full_move += pos->side_to_move;
    pos->ply++;
    pos->last_move = move;

    if(move_type & CAPTURE)
        pos->piece_count--;

    if(pos->castlings)
    {
        if(from == h1 || to == h1)
            pos->castlings &= 14 ; //  0b00001110
        else if( from == a1 || to == a1)
            pos->castlings &= 13; // 0b00001101
        else if(from == h8 || to == h8)
            pos->castlings &= 11; // 0b00001011
        else if( from == a8 || to == a8)
            pos->castlings &= 7; //  0b00000111
        else if( pos-> board[from] == KING)
            pos->castlings &= BLACK_CASTLE ; // binary form : 00001100 it will remove both short/long castlings
        else if(pos->board[from] == BLACK_KING)
            pos->castlings &= WHITE_CASTLE ; // binary form : 0000011 it will remove both short/long castlings
    }
    if(stack->array[stack->top].en_passant) //remove the last ENPASSANThash key 
    {
        pos->key ^= PieceKeys[0][ stack->array[stack->top].en_passant ] ;
    }
    if(pos->en_passant)// xor if there is en passant
    {
        pos->key  ^= PieceKeys[0][ pos->en_passant ] ;
    }
    if(stack->array[stack->top].castlings != pos->castlings) //xor if there is a change in castling rights
    {
        pos->key ^= CastlingKeys[ stack->array[stack->top].castlings] ^ CastlingKeys[pos->castlings];
    }
    pos->key ^= SideToPlayKey;
     switch(move_type)
     {
        case QUIET_MOVE:
            pos->board[to] = piece_type;
            pos->board[from] = EMPTY ; 
            change_piece( piece_type , from ,to);
            pos->key ^= PieceKeys[piece_type][from] ^ PieceKeys[piece_type][to];
            break;
        case CAPTURE:
            pos->board[to] = piece_type;
            pos->board[from] = EMPTY ; 
            delete_piece(captured_piece , to);
            change_piece(piece_type ,from , to );
            pos->key ^= PieceKeys[piece_type][from] ^ PieceKeys[piece_type][to] ^ PieceKeys[captured_piece][to];
            break;
        case DOUBLE_PAWN_PUSH:
            pos->board[to] = piece_type;
            pos->board[from] = EMPTY ;
            change_piece(piece_type , from ,to);
            pos->key ^= PieceKeys[piece_type][from] ^ PieceKeys[piece_type][to];
            if(pos-> side_to_move) //check en passant
            {
               if(pos->board[to +1] == PAWN || pos->board[to -1] == PAWN)
                    pos->en_passant = to +10;
            }
            else
            {
                 if(pos->board[to +1] == BLACK_PAWN || pos->board[to -1] == BLACK_PAWN)
                    pos->en_passant = to - 10;
            }
                break;
        case SHORT_CASTLE:
            pos->board[from ] = EMPTY ;
            pos->board[from -3] = EMPTY;
            if(pos->side_to_move) // if black to move
            {
                pos->key ^= PieceKeys[BLACK_KING][e8] ^ PieceKeys[BLACK_KING][g8] ^ PieceKeys[BLACK_ROOK][h8]^ PieceKeys[BLACK_ROOK][f8];
                pos->board[f8] = BLACK_ROOK;
                pos->board[g8] = BLACK_KING;
                change_piece(BLACK_KING, e8 , g8);
                change_piece(BLACK_ROOK , h8 ,f8);
            }
            else
            {
                pos->key ^= PieceKeys[KING][e1] ^ PieceKeys[KING][g1] ^ PieceKeys[ROOK][h1]^ PieceKeys[ROOK][f1] ;
                pos->board[f1] = ROOK ;
                pos->board[g1] = KING;
                change_piece(KING , e1 , g1);
                change_piece(ROOK , h1 ,f1);  
            }
            break;
        case LONG_CASTLE:
            pos->board[from ] = EMPTY ;
            pos->board[from +4] = EMPTY;
            if(pos->side_to_move) // if black to move
            {
                pos->key ^= PieceKeys[BLACK_KING][e8] ^ PieceKeys[BLACK_KING][c8] ^ PieceKeys[BLACK_ROOK][a8]^ PieceKeys[BLACK_ROOK][d8] ;
                pos->board[d8] = BLACK_ROOK;
                pos->board [c8] = BLACK_KING;
                change_piece(BLACK_KING , e8 , c8);
                change_piece(BLACK_ROOK , a8 ,d8);
            }
            else
            {
                pos->key ^= PieceKeys[KING][e1] ^ PieceKeys[KING][c1] ^ PieceKeys[ROOK][a1]^ PieceKeys[ROOK][d1] ;
                pos->board[d1] = ROOK;
                pos->board [c1] = KING;
                change_piece(KING , e1 , c1);
                change_piece(ROOK , a1 ,d1);
            }          
            break;
        case ENPASSANT:
            pos->board[to] = piece_type;
            pos->board[from] = EMPTY ;
            pos->key ^= PieceKeys[piece_type][from] ^ PieceKeys[piece_type][to] ^ PieceKeys[captured_piece][(from/10)*10 + to%10];
            sq = to  -10 + 20*pos->side_to_move;  
            pos->board[ sq ] =EMPTY;
            change_piece( piece_type , from ,to);
            delete_piece(captured_piece , sq );
            break;
        case KNIGHT_PROMOTION_CAPTURE: case BISHOP_PROMOTION_CAPTURE: case ROOK_PROMOTION_CAPTURE: case QUEEN_PROMOTION_CAPTURE:
            delete_piece(captured_piece , to);
            pos->key ^= PieceKeys[captured_piece][to];
        case KNIGHT_PROMOTION: case BISHOP_PROMOTION: case ROOK_PROMOTION: case QUEEN_PROMOTION:
            pos->board[from] = EMPTY ;
            if(pos-> side_to_move) 
            {
                pos->key  ^= PieceKeys[BLACK_PAWN][from] ^ PieceKeys[BLACK_KNIGHT + ( move_type & 3)][to];
                pos->board[to] = BLACK_KNIGHT +( move_type & 3);
                delete_piece(BLACK_PAWN  , from);
                add_piece(BLACK_KNIGHT +( move_type & 3), to);
            }
            else
            {
                pos->board[to] = KNIGHT+ ( move_type & 3);
                delete_piece(PAWN , from);
                add_piece(KNIGHT +( move_type & 3), to);
                pos->key  ^= PieceKeys[PAWN][from] ^ PieceKeys[KNIGHT + ( move_type & 3)][to];
            }
        break;
     }  
     pos->side_to_move = 1-pos->side_to_move;
     if((move_type & CAPTURE) || (piece_type & 7)== PAWN )
    {
        pos->half_move = 0;
    }
     pos_history.position_keys[pos_history.n++] = pos->key;
}

void unmake_move(Position* pos, Stack* stack,  uint16_t move)
{
    uint8_t from = move_from(move);
    uint8_t to   = move_to(move);
    uint8_t move_type =move & MOVE_TYPE;
        //  Unmake info
        pos->castlings = stack->array[stack->top].castlings;
        pos->en_passant = stack->array[stack->top].en_passant;
        pos->half_move = stack->array[stack->top].half_move ;
        pos-> side_to_move = !pos-> side_to_move;
        pos-> key = stack->array[stack->top].key;
        pos->last_move = stack->array[stack->top].last_move;
        pos->accumulator_cursor[pos->ply ] = 0;
        pos->ply--;
    if(move == NULL_MOVE)
    {
        pop(stack);
        return ;
    }    
    if(move_type & CAPTURE)
        pos->piece_count++;
    uint8_t captured_piece = stack->array[stack->top].captured_piece;  
    pos_history.n--;  
    uint8_t piece_type = pos->board[to] ;
    uint8_t sq;

    if(pos->side_to_move == BLACK)
        pos->full_move--;
    switch(move_type)
     {
         case QUIET_MOVE: case DOUBLE_PAWN_PUSH:
            pos->board[from] = pos->board[to];
            pos->board[to] = EMPTY ; 
            change_piece( piece_type , to , from);
            break;
         case 4:
            pos->board[from] = pos->board[to];
            pos->board[to] = captured_piece;
            change_piece(piece_type , to , from ); 
            add_piece ( captured_piece , to);
            break;
         case SHORT_CASTLE:
            pos->board[from -1] =  EMPTY;
            pos->board[from -2] =  EMPTY;
            if(pos->side_to_move) // if black to move
            {
                pos->board[e8] = BLACK_KING;
                pos->board[h8] = BLACK_ROOK;
                change_piece( BLACK_KING , g8 , e8);
                change_piece( BLACK_ROOK , f8 ,h8);
            }
            else
            {
                pos->board[e1] = KING;
                pos->board[h1] = ROOK;
                change_piece( KING , g1 , e1);
                change_piece(ROOK , f1 , h1); 
            }
            break;
        case LONG_CASTLE: 
            pos->board[from +1] = EMPTY;
            pos->board[from +2] = EMPTY;
            if(pos->side_to_move) // if black to move
            {
                pos->board[e8] = BLACK_KING;
                pos->board[a8] = BLACK_ROOK;
                change_piece( BLACK_KING , c8 , e8);
                change_piece(BLACK_ROOK , d8 , a8);
            }
            else
            {
                pos->board[e1] = KING;
                pos->board[a1] = ROOK;
                change_piece( KING  , c1 , e1);
                change_piece(ROOK  , d1 , a1);
            }          
            break;
        case ENPASSANT:
            pos->board[from] = pos->board[to];
            pos->board[to] = EMPTY ;
            if(pos-> side_to_move)
            {
                 sq = to + 10;  
                pos->board[ sq ] = PAWN; 
                change_piece(BLACK_PAWN , to , from);
                add_piece( PAWN , sq );
            }
            else 
            {
                sq = to - 10;  
                pos->board[ sq ] = BLACK_PAWN;
                change_piece(PAWN , to ,from );
                add_piece( BLACK_PAWN, sq );
            }
            break;
        case KNIGHT_PROMOTION_CAPTURE: case BISHOP_PROMOTION_CAPTURE: case ROOK_PROMOTION_CAPTURE: case QUEEN_PROMOTION_CAPTURE:
        pos->board[to] =  stack->array[stack->top].captured_piece;
        add_piece(  captured_piece , to);
        case KNIGHT_PROMOTION: case BISHOP_PROMOTION: case ROOK_PROMOTION: case QUEEN_PROMOTION:
        if(!(move_type & CAPTURE))
            pos->board[to] = EMPTY;
        if(pos-> side_to_move) 
        {
            pos->board[from] = BLACK_PAWN;
            delete_piece( BLACK_KNIGHT+ (move_type & 3) , to);
            add_piece( BLACK_PAWN , from );
        }
        else
        {
            pos->board[from] = PAWN;
            delete_piece( KNIGHT+ (move_type & 3) , to);
            add_piece( PAWN , from);
        }
        break;
     }
     pop(stack);
}

#endif