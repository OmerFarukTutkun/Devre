#ifndef _SEARCH_H_
#define _SEARCH_H_
#include "movegen.h"
#include "network.h"
#include "board.h"
#include "legal.h"
#include "hash.h"
#include <unistd.h>
int nodes = 0;
int qnodes =0;
#define INF 15000
#define MATE 14000
typedef struct search_info {
    int depth;
    int quit;
    clock_t start_time;
    clock_t stop_time;
    int stopped;
} search_info;
int pick_move(int* scores,int size);
int move_scoring(Position* pos,int* scores,uint16_t *moves, int size);
int is_repetition(Position* pos);
int only_captures(uint16_t moves[], int size);
int max(int a,int b)
{
    return a>b ? a : b;
}
int16_t qsearch(int alpha, int beta, Position* pos,Stack* stack)
{
    qnodes++;
    int index = (pos->key<<HASH_SHIFT)>>HASH_SHIFT;
    int oldalpha,stand_pat,best_score , score ,flag,val,number_of_moves;
    uint16_t move ,bestmove = 0;

      if(hash_table[index].key== pos->key)
    {
        flag = hash_table[index].flag & TT_NODE_TYPE;
        val = hash_table[index].score;

        if(val > MATE -MAX_DEPTH)
                val -= pos->ply;
        else if(val < -MATE + MAX_DEPTH)
                val +=pos->ply;

        if(hash_table[index].hit < 255)
            hash_table[index].hit++;
        if( flag == TT_EXACT
        || (flag == TT_ALPHA && alpha >= val)
        || ( flag == TT_BETA && beta <= val))
            return val;
    }
    oldalpha = alpha;
    stand_pat = evaluate_network(pos);
    best_score = stand_pat;
    if(stand_pat >= beta)
    {
        return stand_pat;
    }
    if ( alpha <stand_pat )
        alpha = stand_pat;


    uint16_t moves[MAX_MOVES_NUMBER];    
    int moves_score[MAX_MOVES_NUMBER] ;

    number_of_moves = generate_moves(pos, moves);
    number_of_moves= only_captures(moves,number_of_moves );
    move_scoring(pos, moves_score, &moves[0], number_of_moves);

    for (int i=0 ; i<number_of_moves ; i++)
    {
        move= moves[pick_move(moves_score, number_of_moves)];
        make_move(pos, move, stack);
        if( is_legal(pos) == 0)
        {
            unmake_move(pos,  stack, move );
            continue;
        }
        score = -qsearch(-beta, -alpha, pos, stack);
        unmake_move(pos,  stack, move );
        if(score > best_score)
        {
            best_score = score;
            bestmove = move;
            if( best_score >= beta)
            {
                updateHashTable(pos, best_score, TT_BETA, 0, bestmove);
                return best_score;
            }
            if(score > alpha)
            {
                alpha=score;
            }
        }
    }
    if( alpha > oldalpha )
    {
        updateHashTable(pos, best_score, TT_EXACT, 0, bestmove);
    }
    else
    {
        updateHashTable(pos, best_score, TT_ALPHA, 0, bestmove);
    }
    return best_score;

}
int16_t AlphaBeta(int alpha, int beta, Position* pos,Stack* stack, int depth,search_info* info, int NullMoveAllowed)
{
    int lmr, oldalpha= alpha;
    int PVNode = (alpha != beta -1);
    uint16_t move,best_move=0;
    int best_score = -INF,score=-INF, mating_value = MATE - pos->ply, incheck;
    int index = (pos->key<<HASH_SHIFT)>>HASH_SHIFT;

    if(( ( nodes + qnodes) %1000 == 0 && (clock() + 30) > info->stop_time  && pos->search_depth > 1) || info->stopped)  //at least do 1 depth search
    {
        info->stopped =TRUE;
        return 0;
    }
    if(depth > 0)
    {
        nodes++;
    }

    //Mate Distance Pruning, the code taken from chessprogramming wiki

    if (mating_value < beta) {
        beta = mating_value;
        if (alpha >= mating_value) return mating_value;
    }

    mating_value = -MATE + pos->ply;

    if (mating_value > alpha) {
        alpha = mating_value;
        if (beta <= mating_value) return mating_value;
    }

    if((is_repetition(pos) || pos->half_move > 100) && pos->ply )  //do not immediately return 0 for root
    {
        return 0;
    }
    if(depth > 0 && hash_table[(pos->key<<HASH_SHIFT)>>HASH_SHIFT].key== pos->key )
    {
        char flag = hash_table[(pos->key<<HASH_SHIFT)>>HASH_SHIFT].flag & TT_NODE_TYPE;
        score = hash_table[(pos->key<<HASH_SHIFT)>>HASH_SHIFT].score;
        if(hash_table[index].hit < 255)
            hash_table[index].hit++;
        if(hash_table[(pos->key<<HASH_SHIFT)>>HASH_SHIFT].depth >= depth )
        {
            if(score > MATE -MAX_DEPTH)
                score -= pos->ply;
            else if(score < -MATE + MAX_DEPTH)
                score +=pos->ply;
            if( flag == TT_EXACT
            || (flag == TT_ALPHA && alpha >= score)
            || ( flag == TT_BETA && beta <= score))
                return score;
        }
    }
    incheck = is_square_attacked(pos,PieceList[8*(pos->side_to_move ) + KING][0], !pos->side_to_move);

    //check extension
    if(incheck && pos->ply)
    {
        if(depth)
            depth++;
        else
            depth=1;
    }

    if (depth <= 0)
    {
        return  qsearch(alpha, beta, pos, stack);
    }
    if( pos->ply > MAX_DEPTH)
    {
        return evaluate_network(pos);
    }
    //Prune positions whose score is well above beta or well below alpha. Return fail-soft.
    //Todo: Tune the parameters and conditions.
    if(!PVNode && !incheck && depth<=4 && pos->ply && beta > -1000 && alpha< 1000)
    {
        if(score == -INF)// get a score from quiescence search if we don't have a score from hash_table
            score= qsearch (alpha,beta,pos,stack);
        if( (pos->last_move & MOVE_TYPE) <2 &&  score < alpha -depth*200) // fail-low
        {
            return score;
        }
        if(score > beta + depth*200) //fail-high
        {
            return score;
        }
    }

    //Null Move Pruning 
    //Todo : tune the parameters.
    if(!PVNode && NullMoveAllowed && !incheck && pos->ply 
    && depth>=2 && beta > -1000 && alpha< 1000 
    && ( score == -INF || score > beta -200))
    {
        int R= 4+ depth/4;
        make_move(pos, NULL_MOVE, stack);
        score = -AlphaBeta( -(beta), -(beta -1), pos, stack, depth -R,info, FALSE);
        unmake_move(pos,stack, NULL_MOVE);
        if(score >= beta && abs(score) < MATE -100)
        {
            return score;
        }
    }

    pos->killers[pos->ply + 1][0] = 0;
    pos->killers[pos->ply + 1][1] = 0;

    uint16_t moves[MAX_MOVES_NUMBER];
    int number_of_moves = generate_moves(pos, moves);
    int moves_score[MAX_MOVES_NUMBER];
    move_scoring(pos, moves_score, &moves[0], number_of_moves);
    int number_of_illegal_moves=0;
    for (int i=0 ; i<number_of_moves ; i++)
    {
        move= moves[pick_move(moves_score, number_of_moves)];
        make_move(pos, move, stack);
        if( !is_legal(pos) )
        {
            number_of_illegal_moves++;
            unmake_move(pos,  stack, move );
            continue;
        }
        lmr=1;
        //Todo: try not reducing for PVNodes or reducing less for PVNodes
        //Todo: try late move pruning 
        //Todo: try different reduction formulas/tables for late move reduction
        if( i > 3 && depth > 3 && (move & MOVE_TYPE) < 2 && !incheck )// don't use late move reduction in check
        {
            lmr=(int)sqrt((sqrt(depth) * sqrt(i)));
        }
        if(best_score > -INF)// search with null window centered at alpha to prove the move fails low.
        {
            score= -AlphaBeta( -(alpha+1), -alpha, pos, stack, depth -lmr,info, NullMoveAllowed);
            if(score > alpha && score < beta)// if the score is inside (alpha, beta) do research with an open window
            {
                score = -AlphaBeta( -beta, -alpha, pos, stack, depth -1,info,NullMoveAllowed );
            }
        }
        // search only the first move with open window
        else {
            score = -AlphaBeta( -beta, -alpha, pos, stack, depth -1,info,NullMoveAllowed );
        }
        unmake_move(pos,  stack, move );
        if(info->stopped == TRUE)
        {
            return 0;
        }
        if (score >best_score)
        {
            best_move = move;
            best_score=score;
            if(pos->ply == 0)
                pos->bestmove = best_move;
            if(best_score > alpha)
            {
                alpha = score;
            }
        }
        if( best_score >= beta)
        {
            if((move & MOVE_TYPE) < 2)
            {
                pos->counter_moves[pos->side_to_move][(pos->last_move&FROM)>>4][(pos->last_move&TO)>>10] = move;//update counter move heuristic
                pos->killers[pos->ply][1] = pos->killers[pos->ply][0];// update killer move heuristic
                pos->killers[pos->ply][0] = move;

                if( depth > 2 )
                {
                    pos->history[pos->side_to_move][(move&FROM)>>4][(move&TO)>>10] += depth*depth;
                }
            }
            updateHashTable(pos,best_score, TT_BETA, depth,move);
            return best_score;
        }
    }
    if(number_of_moves == number_of_illegal_moves)
    {
        if( incheck)
        {
            return -(MATE - pos->ply);//ckeckmate

        }
        else
        {
            return 0;//stalemate
        }
    }
    if(alpha > oldalpha) // It raises alpha
    {
        updateHashTable(pos,best_score, TT_EXACT, depth,best_move);
    }
    else
    {
        updateHashTable(pos, best_score,TT_ALPHA, depth,best_move);
    }
    return best_score;
}
int getPV(Position* pos, Stack* stack, int depth) //get PV from hash table. It is generally shorter than actual PV.
{
    int i=0;
    if(depth == 0)
    {
        return 1;
    }
    if( hash_table[(pos->key<<HASH_SHIFT)>>HASH_SHIFT].key == pos->key)
    {
        uint16_t moves[MAX_MOVES_NUMBER];
        int number_of_moves = generate_moves(pos, moves);
        uint16_t move = hash_table[(pos->key<<HASH_SHIFT)>>HASH_SHIFT].move;
        if(move == 0)
            return 0;
        for(i=0 ; i<number_of_moves ; i++)
        {
            if(moves[i] == move)
            {
                make_move(pos, move, stack);
                if( is_legal(pos ) == 0)
                {
                    unmake_move(pos,  stack, move );
                    return 0;
                }
                print_move(move);
                getPV(pos, stack,depth -1);
                unmake_move(pos, stack,move);
                return 1;
            }
        }
    }
    return 0;
}
uint16_t search(Position* pos, Stack* stack,search_info* info )
{
    nodes=0;
    qnodes=0;
    pos->ply = 0;
    int nps=0;
    int i=1;
    int16_t score,last_score;
    uint64_t total_nodes = 0;
    clock_t start_t=clock();
    uint16_t best_move=0;
    for(i=1 ; i<=info->depth ; i++)
    {
        pos->search_depth = i;
        if(i >= 4 ) // aspiration window search
        {

            int window_size=40;
            score = AlphaBeta((last_score-window_size), last_score+window_size, pos,stack,i,info, TRUE);
            while(1)
            {
                if(score >= last_score+window_size) //half open window search
                {
                    score = AlphaBeta(last_score, last_score+2*window_size, pos,stack,i,info, TRUE);
                }
                else if(score <= last_score-window_size)
                {
                    score = AlphaBeta((last_score-2*window_size), last_score, pos,stack,i,info, TRUE);
                }
                else
                {
                    break;
                }
                window_size *=2;
            }
        }
        else // full open window search for depth <4
        {
            score = AlphaBeta(-INF, INF, pos, stack,i,info, TRUE);
        }
        last_score = score;
        if(info->stopped == TRUE )
            break;

        best_move = pos->bestmove;
        total_nodes = nodes + qnodes;
        nps= (total_nodes * 1000) / (clock() + 1 - start_t);
        if( abs(last_score) < MATE - MAX_DEPTH)
            printf("info depth %d  nps %d nodes %d score cp %d time %d pv ",i,nps,total_nodes, last_score,clock() + 1 - start_t);
        else
        {
            int mate= (MATE-abs(last_score) +1)* (2*(last_score > 0) -1 ) /2;
            printf("info depth %d  nps %d nodes %d score mate %d time %d pv ",i,nps,total_nodes, mate,clock() + 1 - start_t);
        }
        getPV(pos, stack,i);
        printf("\n");
        fflush(stdout);
    }
    printf("bestmove  ");
    if( best_move)
    {
        print_move(best_move);
    }
    else
    {
        getPV(pos, stack,1);
    }
    printf("\n");
    fflush(stdout);

    for(int i=0; i<HASH_SIZE ; i++)
    {
        if( hash_table[i].hit == 0  && (TT_OLD & hash_table[i].flag))//clear old hash entries if there is no hit
            hash_table[i].key=0;
        hash_table[i].hit = 0;
        hash_table[i].flag =(hash_table[i].flag &TT_NODE_TYPE)+ TT_OLD;
    }

    for(int i=0 ; i<2 ; i++)
    {
        for(int j=0 ; j<64 ; j++)
        {
            for(int k=0 ; k<64 ; k++)
            {
                pos->history[i][j][k] /= 64;
            }
        }
    }
}
int pick_move(int* scores,int size)
{
    int max_index=0;
    for(int i=1 ; i<size ; i++)
    {
        if(scores[i] > scores[max_index])
        {
            max_index = i;
        }
    }
    scores[max_index]= -100000;
    return max_index;
}
int move_scoring(Position* pos,int* scores,uint16_t *moves, int size) {

    uint16_t hash_move = 0;
    if( hash_table[(pos->key<<HASH_SHIFT)>>HASH_SHIFT].key == pos->key)
    {
        hash_move = hash_table[(pos->key<<HASH_SHIFT)>>HASH_SHIFT].move;
    }
    for(int i=0; i<size ; i++)
    {
        if(moves[i] == hash_move)
        {
            scores[i] = 100000;
        }
        else
        {
            switch((moves[i] & MOVE_TYPE))// give scores to moves by using their type which is already stored in 2byte move
            {
            case 4:  case 5:  case 8:  case 12:  case 9:
            case 13: case 10: case 14: case 11:  case 15:
                scores[i] = (moves[i] & MOVE_TYPE)*500;
                break;
            case 3: scores[i] =1900;
                break;
            case 2: scores[i] = 1950;
                break;
            case 0: case 1: scores[i] = 0;
                break;
            }

            //Todo: Test the order of killer[0] , killer[1], counter_move.
            if( pos->killers[pos->ply][0] == moves[i])
            {
                scores[i] = 1870;
            }
            else if( pos->killers[pos->ply][1] == moves[i])
            {
                scores[i] = 1860;
            }
            else if(pos->counter_moves[pos->side_to_move][(pos->last_move&FROM)>>4][(pos->last_move&TO)>>10] == moves[i])
            {
                scores[i] = 1850;
            }
            else if((moves[i] & MOVE_TYPE) <2)//history heuristic
            {
                int score= pos->history[pos->side_to_move][(moves[i]&FROM)>>4][(moves[i] & TO)>>10];
                if( score > 1000 )
                {
                    if(score > 80000)
                        scores[i] = 1845;
                    else
                        scores[i] = 1000 + score/100;
                }
                else
                    scores[i] = score;
            }
            // Todo: implement SEE and test against mvv-lva, we can also use SEE for pruning.
            if((moves[i] & MOVE_TYPE) == 4  )//mvv-lva
            {
                uint8_t from = (moves[i] & FROM)>>4;
                from = mailbox64[from];
                uint8_t to = (moves[i] & TO)>>10;
                to = mailbox64[to];
                uint8_t piece_type = pos->board[from];
                uint8_t captured_piece_type = pos->board[to];
                scores[i] += (piece_values[captured_piece_type]*10 - piece_values[piece_type])/2;
            }
        }

    }
    return 1;
}

int is_repetition(Position * pos)
{
    for(int i=pos_history.n-2 ; i>= 0; i--) 
    {

        if(pos->key == pos_history.position_keys[i])
        {
            return TRUE;
        }
        if( pos_history.n - i > pos->half_move)
        {
            return FALSE;
        }
    }
    return FALSE;
}
 int only_captures(uint16_t moves[],int size)
{
    int k=0;
    for(int i=0 ; i<size ; i++)
    {
        if(moves[i] & CAPTURE )
        {
            moves[k++]=moves[i];
        }
    }
    return k;
}
#endif