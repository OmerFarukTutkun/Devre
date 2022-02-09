#ifndef _BOARD_H_
#define _BOARD_H_
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <immintrin.h>
#include "time.h"

#define OFF 255
#define EMPTY 0
#define PAWN 1
#define KNIGHT 2
#define BISHOP 3
#define ROOK 4
#define QUEEN 5 
#define KING 6 // for black 4. bit is 1  
#define BLACK_PAWN 9 
#define BLACK_KNIGHT 10
#define BLACK_BISHOP 11
#define BLACK_ROOK 12
#define BLACK_QUEEN 13 
#define BLACK_KING 14

#define WHITE 0
#define BLACK 1

#define PIECE_TYPE  8 // 00000111
#define WHITE_SHORT_CASTLE 1 
#define BLACK_SHORT_CASTLE 4 
#define WHITE_CASTLE     3 
#define BLACK_CASTLE     12
#define WHITE_LONG_CASTLE 2
#define BLACK_LONG_CASTLE 8

#define STARTING_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

#define MAX_PIECE_NUMBER 10 //the  maximum number of same pieces in a legal chess position. It is 10 for knights,rooks...
//It will cause problems for illegal positions
#define MAX_DEPTH 100 

#define TRUE 1
#define FALSE 0

#define Mirror(sq)  ((7-sq/8)*8 +sq%8)
#define pop(stack) (stack->top--)
#define bitScanForward(bb) ( __builtin_ffsll(bb)-1)  //used in pawn bitboards

 typedef struct Position{
     uint8_t board[120] ;
     uint8_t castlings ;
     uint8_t side_to_move; // 1 for black 0 for White
     int8_t en_passant; 
     uint8_t half_move; 
     uint16_t full_move; 
     uint64_t key; 
     uint8_t ply;
     uint8_t inCheck;
     uint16_t last_move;
     uint16_t bestmove;
     uint8_t search_depth;
     uint64_t history[2][64][64];   // history table for ordering quiet moves
     uint16_t killer[MAX_DEPTH]; // a killer move for move ordering
     uint16_t counter_moves[2][64][64]; // a counter move for move ordering
     uint8_t  accumulator_cursor[2*MAX_DEPTH];
     uint8_t piece_count;
     int scores[MAX_DEPTH];

 }Position;
 
 typedef struct UnMake_Info
{
    //store the information we cannot get back
    int8_t en_passant;
    uint8_t castlings;
    uint8_t captured_piece;
    uint8_t half_move;
    uint64_t key;
    uint16_t last_move;
}UnMake_Info;

 typedef struct Stack{
      uint8_t top;
     UnMake_Info* array;
 }Stack;
int mailbox64[64] = {
    21, 22, 23, 24, 25, 26, 27, 28,
    31, 32, 33, 34, 35, 36, 37, 38,
    41, 42, 43, 44, 45, 46, 47, 48,
    51, 52, 53, 54, 55, 56, 57, 58,
    61, 62, 63, 64, 65, 66, 67, 68,
    71, 72, 73, 74, 75, 76, 77, 78,
    81, 82, 83, 84, 85, 86, 87, 88,
    91, 92, 93, 94, 95, 96, 97, 98};
 int mailbox[120] = {
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1,  0,  1,  2,  3,  4,  5,  6,  7, -1,
     -1,  8,  9, 10, 11, 12, 13, 14, 15, -1,
     -1, 16, 17, 18, 19, 20, 21, 22, 23, -1,
     -1, 24, 25, 26, 27, 28, 29, 30, 31, -1,
     -1, 32, 33, 34, 35, 36, 37, 38, 39, -1,
     -1, 40, 41, 42, 43, 44, 45, 46, 47, -1,
     -1, 48, 49, 50, 51, 52, 53, 54, 55, -1,
     -1, 56, 57, 58, 59, 60, 61, 62, 63, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

char mailbox_to_fen[120][3] = { 
"" , "" ,"" , "" ,"" , "" ,"" , "" ,"" , "" , 
"" , "" ,"" , "" ,"" , "" ,"" , "" ,"" , "" , 
"","h1", "g1", "f1" , "e1" , "d1" , "c1" , "b1" , "a1", "",
"","h2", "g2", "f2" , "e2" , "d2" , "c2" , "b2" , "a2", "",
"","h3", "g3", "f3" , "e3" , "d3" , "c3" , "b3" , "a3", "",
"","h4", "g4", "f4" , "e4" , "d4" , "c4" , "b4" , "a4", "",
"","h5", "g5", "f5" , "e5" , "d5" , "c5" , "b5" , "a5", "",
"","h6", "g6", "f6" , "e6" , "d6" , "c6" , "b6" , "a6", "",
"","h7", "g7", "f7" , "e7" , "d7" , "c7" , "b7" , "a7", "",
"","h8", "g8", "f8" , "e8" , "d8" , "c8" , "b8" , "a8", "",
"","" , "" ,"" , "" ,"" , "" ,"" , "" ,"" , 
"","" , "" ,"" , "" ,"" , "" ,"" , "" ,""  
};

enum {
	h1 = 21, g1, f1, e1, d1, c1, b1, a1,
	h2 = 31, g2, f2, e2, d2, c2, b2, a2,
	h3 = 41, g3, f3, e3, d3, c3, b3, a3,
	h4 = 51, g4, f4, e4, d4, c4, b4, a4,
	h5 = 61, g5, f5, e5, d5, c5, b5, a5,
	h6 = 71, g6, f6, e6, d6, c6, b6, a6,
	h7 = 81, g7, f7, e7, d7, c7, b7, a7,
	h8 = 91, g8, f8, e8, d8, c8, b8, a8
};

 Stack* createStack(void)
{
    Stack* stack = (Stack*)malloc(sizeof(struct Stack));
    stack->top = 0;  
    stack->array = (UnMake_Info*)malloc(4*MAX_DEPTH * sizeof(UnMake_Info));
    return stack;
}
void push( Stack* stack, Position* pos , uint8_t capture)
{
    stack->top = stack->top + 1;
    stack->array[stack->top].castlings =pos->castlings;
    stack->array[stack->top].en_passant = pos->en_passant;
    stack->array[stack->top].captured_piece = capture;
    stack->array[stack->top].half_move = pos->half_move;
    stack->array[stack->top].key = pos->key;
    stack->array[stack->top].last_move = pos->last_move;
}
void fen_to_board ( Position* pos , char fen[])
{
    pos->castlings = 0;
    pos->en_passant = 0 ; 
    pos->full_move = 1;
    pos ->half_move = 0;
    pos->bestmove= 0;
    pos->ply = 0;
    pos->last_move = 0;
    pos->killer[0]= 0;
    pos->piece_count =64;
    memset(pos->accumulator_cursor , 0, 2*MAX_DEPTH);
    int k=0;
    for( int i = 0 ; i <120 ; i++)
        pos->board[i] = EMPTY;
    for(int i = 11 ; i>=0 ; i--)
    {
        for( int j = 9 ; j >= 0 ; j-- )
        {
            if(i < 2 || i>9 || j<1 || j>8) // off board
                pos->board[10*i +j ] = OFF;
            else
            {
                switch(fen[k])
                {
                    case 'Q' : pos->board[10*i + j] = QUEEN; break;
                    case 'q' : pos->board[10*i + j] = BLACK_QUEEN; break;
                    case 'R' : pos->board[10*i + j] = ROOK; break;
                    case 'r' : pos->board[10*i + j] = BLACK_ROOK; break;
                    case 'K' : pos->board[10*i + j] = KING; break;
                    case 'k' : pos->board[10*i + j] = BLACK_KING; break;
                    case 'N' : pos->board[10*i + j] = KNIGHT; break;
                    case 'n' : pos->board[10*i + j] = BLACK_KNIGHT; break;
                    case 'B' : pos->board[10*i + j] = BISHOP; break;
                    case 'b' : pos->board[10*i + j] = BLACK_BISHOP; break;
                    case 'P' : pos->board[10*i + j] = PAWN; break;
                    case 'p' : pos->board[10*i + j] = BLACK_PAWN; break;
                    case '/' : j++; break; //ignore '/'
                    case ' ' : goto out_of_for; break;
                    case '1' : case '2' :case '3' :case '4' :case '5' :case '6' :case '7' : case '8': 
                    for(char e = '0' ; e < fen[k] ; e++) // empty squares
                    {
                        pos->board[10*i + j] = EMPTY;
                        pos->piece_count--;
                        j--; 
                    }     
                        j++;                                break;
                    default: printf("%s",fen); printf("Error: Fen isn't proper!!! "); 
                    assert(0); break;
                }
                k++;
            }
        }
    }
    out_of_for:
    k++;
    if(fen[k] == 'w') // 2- side to move 
        pos->side_to_move = 0;
    else if(fen[k] == 'b')
        pos->side_to_move = 1;
     k +=2;

    while(fen[k] != ' ') // 3-castlings
    {
        switch (fen[k])
        {
        case 'K' : pos->castlings |= WHITE_SHORT_CASTLE;    break;
        case 'Q' : pos->castlings |= WHITE_LONG_CASTLE;  break;
        case 'k' : pos->castlings |= BLACK_SHORT_CASTLE; break;
        case 'q' : pos->castlings |= BLACK_LONG_CASTLE; break;       
        default:          break;
        }
        k++;
    }
    k++;
    if(  fen[k] != '-') //4- en passant square
    {
        pos->en_passant += ('h' - fen[k] + 1);
        k++;
        pos->en_passant += 10*(fen[k] - '0' + 1);
    }
    k +=2;

    int i = 0;
    char number[5] = "";
    while(fen[k] != ' ' && fen[k] != '\n' && fen[k] != '\0')//5- half move
    {
           number[i] = fen[k];
           i++;
           k++;
    }
    number[i] = 0;
    pos->half_move =atoi(number);
    i = 0;
    k++;
    while(fen[k] != '\0' && fen[k] != ' ' && fen[k] != '\n')//6- full move
    {
           number[i] = fen[k];
           i++;
           k++;
    }
    number[i] = 0;
    pos->full_move =(uint16_t)atoi(number);
}
void board_to_fen(Position* pos, char* fen)
{
    char fen_notation[15]=" PNBRQK  pnbrqk";
    int counter=0;
    int empty=0;
    //1-pieces
     for(int i = 9 ; i>=2 ; i--)
    {
        for( int j = 8 ; j >= 1 ; j-- )
        {
            if(pos->board[10*i +j])
            {
                if(empty)
                    fen[counter++] = empty +'0';
                fen[counter++] = fen_notation[pos->board[10*i + j]];
                empty=0;
            }
            else
            {
                empty++;
            }
        }
        if(empty )
        {
            fen[counter++] = empty +'0';
            empty=0;
        }
        if(i> 2)
            fen[counter++] = '/';
    }
    fen[counter++] = ' ';

    //2-side to move
    if(pos->side_to_move)
        fen[counter++] = 'b';
    else
        fen[counter++] = 'w';

    fen[counter++] = ' ';

    //3-castlings
    uint8_t white_castlings = pos->castlings & (WHITE_CASTLE);
    uint8_t black_castlings = pos->castlings & (BLACK_CASTLE);
    switch (white_castlings)
    {
        case 3 : fen[counter++] = 'K';   
        case 2 : fen[counter++] = 'Q'; break;
        case 1 : fen[counter++] = 'K'; break; 
    }
    switch (black_castlings )
    {
        case 12 : fen[counter++] = 'k';   
        case 8 : fen[counter++] =  'q'; break;
        case 4 : fen[counter++] =  'k'; break; 
    }
    if(pos->castlings == 0)
        fen[counter++] = '-';

    //4-5-6 en-passsant, halfmove , fullmove
    fen[counter++] = ' ';
    sprintf(fen + counter, "%s %d %d", pos->en_passant ? mailbox_to_fen[pos->en_passant] : "-", pos->half_move, pos->full_move);
}

void print_Board(Position* pos )
{
    char fen[100];
    char fen_notation[15]=" PNBRQK  pnbrqk";
    printf("fen:  ");
    board_to_fen(pos,fen);
    printf("%s\n", fen);
    for(int i=9 ; i>1 ;i--)
    {
        printf("\n  |----|----|----|----|----|----|----|----|\n");
        for(int j = 8; j >0 ;j--)
         {
              printf("%5c" , fen_notation[pos->board[10*i +j]]);
         }
    }
    printf("\n  |----|----|----|----|----|----|----|----|\n");
    printf("\n%5c%5c%5c%5c%5c%5c%5c%5c\n", 'a','b','c', 'd', 'e','f', 'g' , 'h');
}
#endif
