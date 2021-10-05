#ifndef _TEST_H_
#define _TEST_H_
#include "legal.h"
#include <stdio.h>
#include "hash.h"
#include "search.h"
#include "uci.h"
#include "time.h"
uint64_t perft_counter = 0;
uint64_t perft(Position * pos  , Stack* stack , int depth)
{
    if(depth == 0)
    {
        perft_counter++;
        return 1;
    }
    uint16_t moves[MAX_MOVES_NUMBER];
    int moves_count = 0;
    moves_count= generate_moves(pos , moves);        
    for(int i=0 ; i<moves_count ; i++)
    {
        make_move(pos, moves[i] , stack);
        if( is_legal(pos ))
        {
            perft(pos  , stack , depth -1 );
        }
            unmake_move(pos,  stack  , moves[i] );
    }
    return perft_counter;
}
uint64_t perft_test(Position* pos,Stack* stack, int depth)
{
    clock_t start_t, end_t, total_t; 
    start_t = clock();

    uint64_t total = 0;
    uint64_t nodes = 0;

    uint16_t moves[MAX_MOVES_NUMBER];
    uint8_t from;
    uint8_t to;  
    int moves_count = 0;

    moves_count= generate_moves(pos , moves);

    for(int i = 0 ; i < moves_count ; i++)
    {
         from  = move_from(moves[i]) ;
         to = move_to(moves[i]);
        perft_counter = 0;
        make_move(pos, moves[i] , stack);

        if(is_legal(pos) )
        {
            nodes = perft(pos, stack , depth -1);
            print_move(moves[i]);
            printf(": %llu \n" , nodes);
            total = total + nodes;
       }
            unmake_move(pos , stack , moves[i] );
    } 
    end_t = clock();
    total_t = (end_t - start_t) ;
    printf("Time: %u\t" , total_t);
    printf("Speed: %.2f Mnps\t" ,total/(total_t*1000.0) );
    printf("Total position: %llu\n" , total);
    return total;
}
#endif