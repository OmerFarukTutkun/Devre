#ifndef _MOVEGEN_H_
#define _MOVEGEN_H_

#include "PieceList.h"
#include "board.h"
#include "attack.h"
#include "hash.h"
#define Q_MOVES 0 //
#define D_PAWN_PUSH 1 
#define K_CASTLE 2
#define Q_CASTLE 3
#define CAPTURE 4
#define EP    5
#define N_PROM 8
#define B_PROM 9
#define R_PROM 10
#define Q_PROM 11
#define N_PROM_CAPTURE 12
#define B_PROM_CAPTURE 13
#define R_PROM_CAPTURE 14
#define Q_PROM_CAPTURE 15 

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

    /* least 4 bit for move-type ,  6 bit = from  , 6 bit = to
                            MOVE TYPE
    code 	promotion 	capture 	special 1 	special 0 	kind of move
    0       	0 	    0         	0           	0       	quiet moves
    1       	0 	    0         	0           	1       	double pawn push
    2       	0 	    0         	1           	0       	king castle
    3       	0 	    0         	1           	1       	queen castle
    4       	0 	    1         	0           	0       	captures
    5       	0 	    1         	0           	1       	ep-capture
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
    if((move & MOVE_TYPE) > 7 )
    {
        switch(move & MOVE_TYPE)
        {
            case 8: case 12: printf("n"); break;
            case 9: case 13: printf("b"); break;
            case 10: case 14: printf("r"); break;
            case 11: case 15: printf("q"); break;
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
}
int generate_moves(Position* pos, uint16_t* moves)
{
    uint8_t from;
    uint8_t to;
    uint8_t piece;
  int number_of_moves= 0;  
    if(pos ->side_to_move)// black to move
    {
        switch (pos->castlings & B_CASTLING ) // castlings
        {
            case 12: 
            if(
                pos->board[a8] == B_ROOK && pos->board[e8] ==B_KING && pos->board [d8] == EMPTY && pos->board [c8] == EMPTY 
                && pos->board [b8] == EMPTY && !is_square_attacked(pos , e8 , 0) && !is_square_attacked(pos , d8 , 0))
                    moves[number_of_moves++] = create_move(e8, c8 , 3 );
            case 4: 
            if(pos->board[h8] == B_ROOK && pos->board[e8] ==B_KING &&
                pos->board [f8] == EMPTY && pos->board [g8] == EMPTY &&! is_square_attacked(pos , e8 , 0) && !is_square_attacked(pos , f8 ,0))
                    moves[number_of_moves++] = create_move(e8, g8 , 2 );
            break;
            case 8:
            if( pos->board[a8] == B_ROOK &&  pos->board[e8] ==B_KING &&pos->board [d8] == EMPTY && pos->board [c8] == EMPTY 
            && pos->board [b8] == EMPTY && !is_square_attacked(pos , e8 ,0) && !is_square_attacked(pos , d8 , 0))
                    moves[number_of_moves++] = create_move(e8, c8 , 3 );
            break;
        }


        if(pos->en_passant > 0)// ep capture
        {
            if(pos->board[ pos->en_passant +11 ]== B_PAWN)
            {
                moves[number_of_moves++] = create_move(pos->en_passant +11, pos->en_passant , 5);
            }
            if(pos->board[ pos->en_passant + 9 ]== B_PAWN)
            {
                moves[number_of_moves++] = create_move(pos->en_passant +9, pos->en_passant , 5);
            }
        }


        for(int j=B_KNIGHT ; j<=B_QUEEN ; j++)
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
                         if(offset[j+1 -B_KNIGHT][k] == 0)
                                break;
                            to += offset[j+1 -B_KNIGHT][k] ;
                            if(pos->board[to] == EMPTY) 
                            {
                                moves[number_of_moves++] = create_move(from, to , 0 ); 
                            }
                            else 
                            {
                                switch (pos->board[to])
                                {
                                    case PAWN: case KNIGHT: case BISHOP: case ROOK: case QUEEN: 
                                    moves[number_of_moves++] = create_move(from, to , 4 ); break;
                                }
                                break;
                            }
                            if(j == B_KNIGHT )
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
             to = offset[5][j] + PieceList[B_KING][0];
            switch (pos->board[to])
            {
                case PAWN: case ROOK: case KNIGHT:
                case QUEEN: case BISHOP: moves[number_of_moves++] = create_move(PieceList[B_KING][0], to , 4 ); break; //captures
                case EMPTY: moves[number_of_moves++] = create_move(PieceList[B_KING][0], to , 0 ); break; // quiet
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
                        moves[number_of_moves++] = create_move(from, to , 0);
                    to++; // look right
                    if(pos->board[to] >= PAWN && pos->board[to] <KING)
                        moves[number_of_moves++] = create_move(from, to , 4);
                      to -= 2; // look left
                    if(pos->board[to] >= PAWN && pos->board[to] <KING)
                        moves[number_of_moves++] = create_move(from, to , 4);       
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
        switch (pos->castlings & W_CASTLING ) // castlings
        {
            case 3: 
            if( pos -> board[a1] == ROOK &&  pos->board [d1] == EMPTY && pos->board[e1] == KING &&pos->board [c1] == EMPTY &&
                 pos->board [b1] == EMPTY && ! is_square_attacked(pos , e1 , 1) && ! is_square_attacked(pos , d1 ,1))
                 moves[number_of_moves++] = create_move(e1, c1 , 3 );
            case 1:
            if( pos -> board[h1] == ROOK && pos->board[e1] == KING &&pos->board [f1] == EMPTY && pos->board [g1] == EMPTY && 
            !is_square_attacked(pos , e1 , 1) && !is_square_attacked(pos , f1 , 1))
                    moves[number_of_moves++] = create_move(e1, g1 , 2 );
            break;
            case 2:
            if(
                pos -> board[a1] == ROOK &&  pos->board [d1] == EMPTY && pos->board[e1] == KING &&pos->board [c1] == EMPTY &&
                 pos->board [b1] == EMPTY && ! is_square_attacked(pos , e1 , 1) && ! is_square_attacked(pos , d1 ,1))
                 moves[number_of_moves++] = create_move(e1, c1 , 3 );
            break;
        }
        if(pos->en_passant > 0)// ep capture
        {
            if(pos->board[ pos->en_passant - 11 ] == PAWN)
            { 
            moves[number_of_moves++] = create_move(pos->en_passant -11, pos->en_passant , 5);
            }

            if(pos->board[ pos->en_passant -9 ] == PAWN)
            {
                moves[number_of_moves++] = create_move(pos->en_passant -9, pos->en_passant , 5);
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
                                moves[number_of_moves++] = create_move(from, to , 0 ); 
                            }
                            else 
                            {
                                switch (pos->board[to])
                                {
                                    case B_PAWN: case B_KNIGHT: case B_BISHOP: case B_ROOK: case B_QUEEN: 
                                    moves[number_of_moves++] = create_move(from, to , 4 ); break;
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
               case B_PAWN: case B_ROOK : case B_KNIGHT : case B_QUEEN: case B_BISHOP:
               moves[number_of_moves++] = create_move(PieceList[KING][0], to , 4 ); break; //captures
                case EMPTY: moves[number_of_moves++] = create_move(PieceList[KING][0], to , 0 ); break; // quiet
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
                    moves[number_of_moves++] = create_move(from, to +10 , 1); //double pawn push
                }
                case 7: case 6: case 5: case 4:
                if(pos->board[to] == EMPTY)
                    moves[number_of_moves++] = create_move(from, to , 0);
                to++; // look right
                if(pos->board[to] >= B_PAWN && pos->board[to] < B_KING )
                    moves[number_of_moves++] = create_move(from, to , 4);                    
                to -= 2; // look left
                if(pos->board[to] >= B_PAWN && pos->board[to] < B_KING)
                    moves[number_of_moves++] = create_move(from, to , 4);
                break; 
                case 8:
                    if(pos->board[to] == EMPTY)
                    {
                        for(int j= 8 ; j < 12 ; j++)
                            moves[number_of_moves++] = create_move(from, to , j);
                    }
                to++; // look right
                if(pos->board[to] >= B_PAWN && pos->board[to] <B_KING)
                {
                    for(int j= 12 ; j < 16 ; j++)
                            moves[number_of_moves++] = create_move(from, to , j);
                }
                to -= 2;
                if(pos->board[to] >= B_PAWN  && pos->board[to] <B_KING )
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
    uint8_t move_type = (move & MOVE_TYPE);
    uint8_t piece_type = pos->board[from]; // moving piece
    uint8_t sq ; // for ep
   
     uint8_t captured_piece = pos->board[to]; // Captured piece type 
     if(move_type == 5)
     {
        captured_piece = PAWN + (!pos->side_to_move)*8;
     }
     push(stack, pos, captured_piece);
      pos->en_passant = 0;
      pos->half_move++; 
      pos->full_move += pos->side_to_move;
      pos->ply++;
      pos->last_move = move;


    if(from == h1 || to == h1)
        pos->castlings &= 14 ; //  0b00001110
    else if( from == a1 || to == a1)
        pos->castlings &= 13; // 0b00001101
    else if(from == h8 || to == h8)
        pos->castlings &= 11; // 0b00001011
    else if( from == a8 || to == a8)
        pos->castlings &= 7; //  0b00000111
    else if( pos-> board[from] == KING)
        pos->castlings &= B_CASTLING ; // binary form : 00001100 it will remove both short/long castlings
    else if(pos->board[from] == B_KING)
        pos->castlings &= W_CASTLING ; // binary form : 0000011 it will remove both short/long castlings

     switch(move_type)
     {
         case 0:
            pos->board[to] = pos->board[from];
            pos->board[from] = EMPTY ; 
            change_piece( piece_type , from ,to);
            break;
         case 4:
            pos->board[to] = pos->board[from];
            pos->board[from] = EMPTY ; 
            delete_piece(captured_piece , to);
            change_piece(piece_type ,from , to );
            break;
         case 1:
            pos->board[to] = piece_type;
            pos->board[from] = EMPTY ;
            change_piece(piece_type , from ,to);
            if(pos-> side_to_move) //check en passant
            {
               if(pos->board[to +1] == PAWN || pos->board[to -1] == PAWN)
                    pos->en_passant = to +10;
            }
            else
            {
                 if(pos->board[to +1] == B_PAWN || pos->board[to -1] == B_PAWN)
                    pos->en_passant = to - 10;
            }
                break;
         case 2:
            pos->board[from ] = EMPTY ;
            pos->board[from -3] = EMPTY;
            if(pos->side_to_move) // if black to move
            {
                pos->board[f8] = B_ROOK;
                pos->board[g8] = B_KING;
                change_piece(B_KING, e8 , g8);
                change_piece(B_ROOK , h8 ,f8);
            }
            else
            {
                pos->board[f1] = ROOK ;
                pos->board[g1] = KING;
                change_piece(KING , e1 , g1);
                change_piece(ROOK , h1 ,f1);  
            }
            break;
        case 3:
            pos->board[from ] = EMPTY ;
            pos->board[from +4] = EMPTY;
            if(pos->side_to_move) // if black to move
            {
                pos->board[d8] = B_ROOK;
                pos->board [c8] = B_KING;
                change_piece(B_KING , e8 , c8);
                change_piece(B_ROOK , a8 ,d8);
            }
            else
            {
                pos->board[d1] = ROOK;
                pos->board [c1] = KING;
                change_piece(KING , e1 , c1);
                change_piece(ROOK , a1 ,d1);
            }          
            break;
        case 5:
            pos->board[to] = pos->board[from];
            pos->board[from] = EMPTY ;
            if(pos-> side_to_move)
            {
                sq = to  + 10;  
                pos->board[ sq ] =EMPTY;
                change_piece( B_PAWN , from ,to);
                delete_piece( PAWN , sq );
            }
            else 
            {
                sq = to - 10;  
                pos->board[ sq ] =EMPTY; 
                change_piece(PAWN , from ,to);
                delete_piece( B_PAWN, sq );
            }
            break;
        case 12: case 13: case 14: case 15:
        delete_piece(captured_piece , to);
         case 8: case 9: case 10: case 11:
           pos->board[from] = EMPTY ;
           if(pos-> side_to_move) 
           {
               pos->board[to] = B_KNIGHT +( move_type & 3);
               delete_piece(B_PAWN  , from);
               add_piece(B_KNIGHT +( move_type & 3), to);

           }
           else
           {
               pos->board[to] = KNIGHT+ ( move_type & 3);
               delete_piece(PAWN , from);
               add_piece(KNIGHT +( move_type & 3), to);
           }
        break;
     }  
     pos->side_to_move = 1-pos->side_to_move;
     updateZobristKey(pos, move , stack);
     if((move_type & CAPTURE) || (piece_type == PAWN) || (piece_type == B_PAWN))
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
        pos->ply--;

    if(move == NULL_MOVE)
    {
        pop(stack);
        return ;
    }
    uint8_t captured_piece = stack->array[stack->top].captured_piece;  
    pos_history.n--;  
    uint8_t piece_type = pos->board[to] ;
    uint8_t sq;

    if(pos->side_to_move == BLACK)
        pos->full_move--;
    switch(move_type)
     {
         case 0: case 1:
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
         case 2:
            pos->board[from -1] =  EMPTY;
            pos->board[from -2] =  EMPTY;
            if(pos->side_to_move) // if black to move
            {
                pos->board[e8] = B_KING;
                pos->board[h8] = B_ROOK;
                change_piece( B_KING , g8 , e8);
                change_piece( B_ROOK , f8 ,h8);
            }
            else
            {
                pos->board[e1] = KING;
                pos->board[h1] = ROOK;
                change_piece( KING , g1 , e1);
                change_piece(ROOK , f1 , h1); 
            }
            break;
        case 3: 
            pos->board[from +1] = EMPTY;
            pos->board[from +2] = EMPTY;
            if(pos->side_to_move) // if black to move
            {
                pos->board[e8] = B_KING;
                pos->board[a8] = B_ROOK;
                change_piece( B_KING , c8 , e8);
                change_piece(B_ROOK , d8 , a8);
            }
            else
            {
                pos->board[e1] = KING;
                pos->board[a1] = ROOK;
                change_piece( KING  , c1 , e1);
                change_piece(ROOK  , d1 , a1);
            }          
            break;
        case 5:
            pos->board[from] = pos->board[to];
            pos->board[to] = EMPTY ;
            if(pos-> side_to_move)
            {
                 sq = to + 10;  
                pos->board[ sq ] = PAWN; 
                change_piece(B_PAWN , to , from);
                add_piece( PAWN , sq );
            }
            else 
            {
                sq = to - 10;  
                pos->board[ sq ] = B_PAWN;
                change_piece(PAWN , to ,from );
                add_piece( B_PAWN, sq );
            }
            break;
        case 12: case 13: case 14: case 15:
        pos->board[to] =  stack->array[stack->top].captured_piece;
        add_piece(  captured_piece , to);
        case 8: case 9: case 10: case 11:
        if(!(move_type & CAPTURE))
            pos->board[to] = EMPTY;
        if(pos-> side_to_move) 
        {
            pos->board[from] = B_PAWN;
            delete_piece( B_KNIGHT+ (move_type & 3) , to);
            add_piece( B_PAWN , from );
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
