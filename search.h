#ifndef _SEARCH_H_
#define _SEARCH_H_

#include "movegen.h"
#include "board.h"
#include "legal.h"
#include "hash.h"
#include <windows.h>
#include "nnue.h"

uint64_t nodes = 0;
uint64_t qnodes =0;
#define INF 15000
#define MATE 14000

enum {NORMAL_GAME=0, FIX_DEPTH , FIX_NODES, FIX_TIME 
};

typedef struct search_info {
    int depth;
    uint64_t nodes;
    int quit;
    clock_t start_time;
    clock_t stop_time;
    int stopped;
    int search_type;
} search_info;

int non_pawn_pieces();
int pick_move(int* scores,int size, int* score_of_move);
int move_scoring(Position* pos, int* scores,uint16_t *moves, int size);
int is_repetition(Position* pos);
int only_captures(uint16_t moves[], int size);
int InputAvaliable();
int UciCheck(search_info* info);
int see(Position* pos,uint16_t move);
void divideHistoryTable(Position* pos, int x); // divide history table by 2^x
int16_t qsearch(int alpha, int beta, Position* pos,Stack* stack)
{
    qnodes++;
    int index = (pos->key<<HASH_SHIFT)>>HASH_SHIFT;
    int oldalpha,stand_pat,best_score , score ,flag,val,number_of_moves,score_of_move;
    uint16_t move ,bestmove = 0;

    // calculate network before probing TT so that incremental update would be possible for child nodes.
    stand_pat = evaluate_network(pos ,stack, pos->last_move); 
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
    if(stand_pat == 0)
        stand_pat = 1;
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
        move= moves[pick_move(moves_score, number_of_moves, &score_of_move)];
	    int value = see(pos, move);
        if(value < 0 || (value == 0 && stand_pat + 300 < alpha))
            continue;
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
int AlphaBeta(int alpha, int beta, Position* pos,Stack* stack, int depth,search_info* info, int NullMoveAllowed)
{
    int lmr, oldalpha= alpha ,score_of_move;
    int PVNode = (alpha != beta -1) ,ttdepth=0;
    uint8_t flag = 0;
    uint16_t move,best_move=0;
    int best_score = -INF,score=-INF, mating_value = MATE - pos->ply, inCheck,improving,eval;
    int index = (pos->key<<HASH_SHIFT)>>HASH_SHIFT;

    if( (nodes & 2047) == 0 )
    {
        UciCheck(info);
    }
    if((nodes & 65535) == 0)
    {
        divideHistoryTable(pos, 4);
    }
    if( ( ( nodes & 255 ) == 0  && ((clock() + 50) > info->stop_time ) && pos->search_depth > 1) 
            || info->stopped 
            || ( info->search_type == FIX_NODES && (nodes +qnodes) > info->nodes))  
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
    if(depth > 0 && pos->ply && !PVNode && hash_table[index].key== pos->key )
    {
        flag = hash_table[index].flag & TT_NODE_TYPE;
        eval = score = hash_table[index].score;
        if(hash_table[index].hit < 255)
            hash_table[index].hit++;
        ttdepth = hash_table[index].depth;
        if( ttdepth >= depth)
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
    pos->inCheck = inCheck = is_square_attacked(pos,PieceList[8*(pos->side_to_move ) + KING][0], !pos->side_to_move);
    //check extension
    if(inCheck && pos->ply)
    {
        depth = max(1, 1+ depth);
    }

    if (depth <= 0)
    {
        return  qsearch(alpha, beta, pos, stack);
    }
    if( pos->ply > MAX_DEPTH)
    {
        return evaluate_network(pos , stack, pos->last_move);
    }
     if(flag != TT_EXACT )
    {
        eval = qsearch(alpha ,beta , pos, stack);
    }
    pos->evals[pos->ply] = eval;
    if(pos->ply >= 2)
        improving = !inCheck &&  eval > pos->evals[pos->ply - 2];

    if(!PVNode && !inCheck && depth <= 5 && pos->ply && beta > -1000 && alpha< 1000)
    {
        if( (pos->last_move & MOVE_TYPE) <2 &&  eval < alpha - depth*200) // fail-low
        {
            return eval;
        }
        if(eval > beta + depth*125 ) //fail-high
        {
            return eval;
        }
    }

    //Null Move Pruning 
    //Todo : tune the parameters.
    if(!PVNode && NullMoveAllowed && !inCheck && pos->ply 
    && depth>=2 && beta > -1000 && alpha< 1000 
    && ( eval > beta) && non_pawn_pieces(pos) )
    {
        //Use very similar formula to Stockfish
        int R= 4+ depth/4;
        R += min(2 ,(eval- beta)/200);

        make_move(pos, NULL_MOVE, stack);
        score = -AlphaBeta( -(beta), -(beta -1), pos, stack, depth -R,info, FALSE);
        unmake_move(pos,stack, NULL_MOVE);
        if(score >= beta && abs(score) < MATE -100)
        {
             return score;
        }
    }

    pos->killer[pos->ply + 1] = 0;
    uint16_t moves[MAX_MOVES_NUMBER];
    int number_of_moves = generate_moves(pos, moves);
    int moves_score[MAX_MOVES_NUMBER];
    move_scoring(pos,moves_score, &moves[0], number_of_moves);
    int number_of_illegal_moves=0;
    int played = 0;
    for (int i=0 ; i<number_of_moves ; i++)
    {
        move= moves[pick_move(moves_score, number_of_moves , &score_of_move)];
        //see pruning
        if( !PVNode && depth <=5 && move_type(move) < 2 && played > 10 && see(pos,move) < 0)
        {
            played++;
            continue;
        }
        // futility pruning
        if( !PVNode && depth <=8 && !inCheck && move_type(move) < 2 && played > 5 && eval + depth*40 + 200 < alpha)
        {
            played++;
            continue;
        }
        int see_reduction = (played >1 && depth > 2 && see(pos, move) < 0);
        make_move(pos, move, stack);
        if( !is_legal(pos) )
        {
            number_of_illegal_moves++;
            unmake_move(pos,  stack, move );
            continue;
        }
        lmr=1;
        // The LMR code was taken from Ethereal, then modified
        if( played >1 && depth > 2  && move_type(move) < 2 )
        {
            lmr =  0.75 + log(depth)*log(played)/2.25;
            lmr += !PVNode;
            lmr += inCheck && (pos->board[move_to(move)] & 7) == KING;
            lmr -= (score_of_move > 1845); // killer and counter move
            lmr -= min( 2, pos->history[pos->side_to_move][(move&FROM)>>4][(move & TO)>>10] / 500);//less reduction for the moves with good history score
            lmr = max(1, min(depth-1 , lmr)); 
        }
        if(lmr != depth -1)
            lmr += see_reduction;
        if(played >=1 )// search with null window centered at alpha to prove the move fails low.
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
        played++;
        if(info->stopped == TRUE)
        {
            return 0;
        }
        if (score > best_score)
        {
            best_move = move;
            best_score = score;
            if(pos->ply == 0)
                pos->bestmove = best_move;
            if(best_score > alpha)
            {
                alpha = best_score;
            }
        }
        if( best_score >= beta)
        {
            if(move_type(move) < 2)
            {
                if(pos->last_move != 0 && pos->last_move != NULL_MOVE)
                    pos->counter_moves[pos->side_to_move][(pos->last_move&FROM)>>4][(pos->last_move&TO)>>10] = move;//update counter move heuristic
                pos->killer[pos->ply] = move;

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
        if( inCheck)
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
    if(depth == 0)
    {
        return 1;
    }
    if( hash_table[(pos->key<<HASH_SHIFT)>>HASH_SHIFT].key == pos->key)
    {
        uint16_t move = hash_table[(pos->key<<HASH_SHIFT)>>HASH_SHIFT].move;
        if(move == 0)
            return 0;

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
    return 0;
}
void search(Position* pos, Stack* stack,search_info* info )
{
    nodes=0;
    qnodes=0;
    pos->ply = 0;
    int nps=0;
    int16_t score,last_score;
    uint64_t total_nodes = 0;
    clock_t start_t=clock();
    uint16_t best_move=0;
    uint64_t t_nodes[MAX_DEPTH];
    float branching_factor = 1.0;
    for(int i=1 ; i<=info->depth ; i++)
    {
        pos->search_depth = i;
        if(i >= 4 ) // aspiration window search
        {
            int window_size=20;
            score = AlphaBeta((last_score-window_size), last_score+window_size, pos,stack,i,info, TRUE);
            while(1)
            {
                if(score >= last_score+window_size) 
                {
                    score = AlphaBeta(last_score -2*window_size, last_score+2*window_size, pos,stack,i,info, TRUE);
                }
                else if(score <= last_score-window_size)
                {
                    score = AlphaBeta((last_score-2*window_size), last_score +2*window_size , pos,stack,i,info, TRUE);
                }
                else
                {
                    break;
                }
                window_size *=2;
            }
        }
        else// full open window search for depth <4
        {
            score = AlphaBeta(-INF, INF, pos, stack,i,info, TRUE);
        }
        last_score = score;
        if(info->stopped == TRUE )
            break;

        best_move = pos->bestmove;
        if(i >= 10)
            branching_factor = t_nodes[i-1]/((float)t_nodes[i-2]);
        total_nodes = nodes + qnodes;
        t_nodes[i] = nodes +qnodes;
        nps= (total_nodes * 1000) / (clock() + 1 - start_t);
        if( abs(last_score) < MATE - MAX_DEPTH)
        {
            printf("info depth %d  nps %d nodes %llu score cp %d time %ld pv ",i,nps,total_nodes, last_score,clock() + 1 - start_t);       
        }
        else
        {
            int mate= (MATE-abs(last_score) +1)* (2*(last_score > 0) -1 ) /2;
            printf("info depth %d  nps %d nodes %llu score mate %d time %ld pv ",i,nps,total_nodes, mate,clock() + 1 - start_t);
             
        }
        getPV(pos, stack,i);
        printf("\n");
        fflush(stdout);
        if(info->search_type == NORMAL_GAME && (clock() - info->start_time)*branching_factor > (info->stop_time - info->start_time))
        {
            break ;
        }
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
}
int pick_move(int* scores,int size ,int *score_of_move)
{
    int max_index=0;
    for(int i=1 ; i<size ; i++)
    {
        if(scores[i] > scores[max_index])
        {
            max_index = i;
        }
    }
    score_of_move[0] = scores[max_index];
    scores[max_index]= -INF;
    return max_index;
}
int move_scoring(Position* pos, int* scores,uint16_t *moves, int size) {

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
            if( pos->killer[pos->ply]== moves[i])
            {
                scores[i] = 1970;
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
                uint8_t from = move_from(moves[i]);
                uint8_t to   = move_to(moves[i]);
                uint8_t piece_type = pos->board[from];
                uint8_t captured_piece_type = pos->board[to];
                scores[i] += (piece_values[captured_piece_type]*10 - piece_values[piece_type]/2)/2;
                if(to == move_to(pos->last_move))
                {
                    scores[i] += 100;
                }
                if(see(pos,moves[i]) < 0)
                    scores[i] -= 1000;
            }
        }

    }
    return 1;
}
//Iterative see implementation
int see(Position* pos, uint16_t move)
{
    if(move_type(move) >=2 && move_type(move) != CAPTURE)
        return 1000;
    uint64_t attackers[15] = {0,};
    int gain[32] = {0,};
    int d = 0;
    int from = move_from(move);
    int to = move_to(move);
    int side = pos->side_to_move;
    int attacking_piece = pos->board[from];
    int square=from, unit_vector;
    if( (attacking_piece & 7 ) == KING)
        return see_values[pos->board[to]];

    gain[0] = see_values[pos->board[to]];
    square_attacked_by(pos, to, &attackers[0]);
    
  do
  {
    d++;
    gain[d]  = see_values[attacking_piece] - gain[d-1];
    if ( max(-gain[d-1], gain[d]) < 0){
      break;
    }
    if((attacking_piece & 7) != KNIGHT && (attacking_piece & 7) != KING)
    {
        unit_vector = vector_to_unit_vector[square - to +77]; 
        if(!unit_vector)
            assert(0);
        int piece = EMPTY;
        int i = 0;
        while(piece == EMPTY)
        {
            i++;
            piece =  pos->board[ square + i*unit_vector] ;
        }
        if(piece != OFF)
        {
            if(ortogonal_or_diagonal[unit_vector + 21 ] == 1) // ortogonal
            {
                switch(piece)
                {
                    case ROOK: case BLACK_ROOK: case QUEEN: case BLACK_QUEEN: add_to_attackers(&attackers[0], piece, square + i*unit_vector ); break;
                    default: break;
                }
            }
            else if(ortogonal_or_diagonal[unit_vector + 21 ] == -1 )// diagonal
            {
                switch(piece)
                {
                    case BISHOP: case BLACK_BISHOP: case QUEEN: case BLACK_QUEEN: add_to_attackers(&attackers[0], piece, square + i*unit_vector ); break;
                    default: break;
                }
            }
        }
    }
    side = !side;
    attackers[attacking_piece] = attackers[attacking_piece] ^ ( ONE <<  mailbox[square] ); //delete the played move from attack table
    attacking_piece = get_smallest_attacker(attackers, &square,  side);
  } while (attacking_piece);

  while (--d){
    gain[d-1] = - max(-gain[d-1], gain[d]);
  }

  return gain[0];
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
            moves[k++] = moves[i];
        }
    }
    return k;
}

int non_pawn_pieces()
{
    if(!(PieceList[BLACK_KNIGHT][0] || PieceList[BLACK_ROOK][0] || PieceList[BLACK_BISHOP][0] || PieceList[BLACK_QUEEN][0] ))
        return 0;
    if(PieceList[KNIGHT][0] || PieceList[ROOK][0] || PieceList[BISHOP][0] || PieceList[QUEEN][0] )
        return 1;
    return 0;
}
void divideHistoryTable(Position* pos, int x)
{
    for(int i=0 ; i<2 ; i++)
    {
        for(int j=0 ; j<64 ; j++)
        {
            for(int k=0 ; k<64 ; k++)
            {
                pos->history[i][j][k] >>= x;
            }
        }
    }
}

int InputAvaliable()
{
//the code taken from Vice 
  static int init = 0, pipe;
	static HANDLE inh;
	DWORD dw;

	if (!init) {
		init = 1;
		inh = GetStdHandle(STD_INPUT_HANDLE);
		pipe = !GetConsoleMode(inh, &dw);
		if (!pipe) {
			SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT|ENABLE_WINDOW_INPUT));
			FlushConsoleInputBuffer(inh);
		}
	}
	if (pipe) {
		if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL))
			return 1;
		return dw;
	} else {
		GetNumberOfConsoleInputEvents(inh, &dw);
		return dw <= 1 ? 0 : dw;
	}
}
int UciCheck(search_info* info)
{
    if(InputAvaliable())
    {
         char input[256] = "";
		 fgets(input, 256, stdin);
		if (strlen(input) > 0) {
			if (!strncmp(input, "quit", 4))    {
              info->stopped = TRUE;
			  info->quit = TRUE;
              return 1;
			}
            else if (!strncmp(input, "stop", 4))    {
			  info->stopped = TRUE;
              return 1;
			}
            else if (!strncmp(input, "isready", 7))    {
			  printf("readyok\n");
			  fflush(stdout);
              return 1;
			}
		}
    }
    return 0;
}
#endif
