#include "perft.h"
#include "attack.h"
uint64_t perft(Position * pos , int depth)
{
    if(depth == 0)
    {
        return 1;
    }
    MoveList move_list;
    move_list.num_of_moves = generate_moves(pos , move_list.moves , ALL_MOVES);  
    uint64_t count = 0;
    for(int i=0 ; i<move_list.num_of_moves ; i++)
    {
        make_move(pos, move_list.moves [i] );
        if( is_legal(pos ))
        {
            count += perft(pos , depth -1 );
        }
        unmake_move(pos  , move_list.moves [i] );
    }
    return count;
}
void perft_test(Position* pos, int depth)
{
    long long int  start_t, end_t, elapsed_t; 
    start_t = timeInMilliseconds();

    uint64_t total = 0;
    uint64_t count = 0;

    MoveList move_list;
    move_list.num_of_moves = generate_moves(pos , move_list.moves , ALL_MOVES);
    for(int i = 0 ; i < move_list.num_of_moves ; i++)
    {
        make_move(pos, move_list.moves[i] );
        if(is_legal(pos) )
        {
            count = perft(pos, depth -1);
            print_move( move_list.moves[i]);
            printf(": %llu \n" , count);
            total = total + count;
       }
        unmake_move(pos ,  move_list.moves[i]);
    } 

    end_t = timeInMilliseconds();
    elapsed_t = (end_t - start_t) ;
    printf("Time: %lld ms\t" , elapsed_t);
    printf("Speed: %.2f Mnps\t" ,total/(elapsed_t*1000.0) );
    printf("Total position: %llu\n" , total);
}