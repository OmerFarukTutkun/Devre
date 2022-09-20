#include "search.h"
int16_t qsearch(int alpha, int beta, Position* pos,SearchInfo* info)
{
    int oldalpha,stand_pat,best_score,score;
    uint16_t move ,bestmove = 0,ttMove = 0;

    // calculate network before probing TT so that incremental update would be possible for child nodes.
    stand_pat =evaluate_nnue(pos);
    if(is_draw(pos))
        return 0;
    TTentry* entry = tt_probe(pos->key);
    if(entry)
    {
        char flag   = entry->flag ;
        int16_t val =  entry->score;
        ttMove = entry->move;
        if(val > MATE -MAX_DEPTH)
                val -= pos->ply;
        else if(val < -MATE + MAX_DEPTH)
                val +=pos->ply;
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

    MoveList move_list;
    move_list.num_of_moves = generate_moves(pos, move_list.moves, QSEARCH);
    score_moves(pos, &move_list,ttMove ,QSEARCH); 
    for (int i=0 ; i < move_list.num_of_moves ; i++)
    {
        move= pick_move(&move_list, i);
	    int see = SEE(pos, move);
        if(see < 0 || (see == 0 && stand_pat + 300 < alpha))
            continue;
        make_move(pos, move);
        if( is_legal(pos) == 0)
        {
            unmake_move(pos, move );
            continue;
        }
        score = -qsearch(-beta, -alpha, pos , info);
        unmake_move(pos, move );
        if(score > best_score)
        {
            best_score = score;
            bestmove = move;
            if( best_score >= beta)
            {
                tt_save(pos, best_score, TT_BETA, 0, bestmove);
                return best_score;
            }
            if(score > alpha)
            {
                alpha= score;
            }
        }
    }
    if( alpha > oldalpha )
    {
        tt_save(pos, best_score, TT_EXACT, 0, bestmove);
    }
    else
    {
        tt_save(pos, best_score, TT_ALPHA, 0, bestmove);
    }
    return best_score;
}
int AlphaBeta(int alpha, int beta, Position* pos, int depth,SearchInfo* info)
{
    int lmr, oldalpha =alpha;
    int PVNode = (alpha != beta -1);
    uint16_t move,best_move= 0 ,ttMove = 0;
    int tt_flag = 0;
    int best_score = -INF,score=-INF, mating_value = MATE - pos->ply, inCheck, eval;
    int rootNode = !pos->ply;
    if( (pos->nodes & 2047) == 0 )
    {
        UciCheck(info);
    }
    if( ( ( pos->nodes & 255 ) == 0  && ((timeInMilliseconds() + 50) > info->stop_time ) && info->search_depth > 1) 
            || info->stopped 
            || ( info->search_type == FIX_NODES && pos->nodes > info->fixed_nodes))  
    {
        info->stopped =TRUE;
        return 0;
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

    if(is_draw(pos))  
    {
        return 0;
    }
    TTentry* entry = tt_probe(pos->key);
    if(entry && depth > 0 && !rootNode && !PVNode)
    {
        tt_flag= entry->flag;
        eval = score = entry->score;
        ttMove = entry->move;
        if( entry->depth >= depth)
        {
            if(score > MATE -MAX_DEPTH)
                score -= pos->ply;
            else if(score < -MATE + MAX_DEPTH)
                score +=pos->ply;
            if( tt_flag == TT_EXACT
            || (tt_flag == TT_ALPHA && alpha >= score)
            || ( tt_flag == TT_BETA && beta <= score))
                return score;
        }
    }
    inCheck = is_inCheck(pos);
    //check extension
    if(inCheck && pos->ply)
    {
        depth = MAX(1, 1+ depth);
    }
    if (depth <= 0)
    {
        return  qsearch(alpha, beta, pos, info);
    }
    if( pos->ply > MAX_DEPTH)
    {
        return evaluate_nnue(pos);
    }
     if(tt_flag != TT_EXACT )
    {
        eval = qsearch(alpha ,beta , pos, info);
    }
    pos->evals[pos->ply] = eval;
    int improving = !inCheck && pos->ply >=2 &&  pos->evals[pos->ply]  >  pos->evals[pos->ply -2]; 

    //IIR
    if(entry == NULL && depth >= 4)
    	depth--;
    
    if(!PVNode && !inCheck && depth <= 5 && !rootNode && beta > -1000 && alpha< 1000)
    {
        if( eval < alpha -depth*200) // fail-low
        {
            return eval;
        }
        if(eval > beta +depth*125 ) //fail-high
        {
            return eval;
        }
    }

    //Null Move Pruning 
    if(!PVNode && pos->move_history[pos->ply] != NULL_MOVE && !inCheck && !rootNode
    && depth>=2 && beta > -1000 && alpha< 1000 
    && ( eval > beta) && has_non_pawn_pieces(pos) )
    {
        int R= 4 + depth/4;
        R += MIN(3 ,(eval- beta)/200);

        make_move(pos, NULL_MOVE);
        score = -AlphaBeta( -(beta), -(beta -1), pos, depth -R, info);
        unmake_move(pos, NULL_MOVE);
        if(score >= beta && abs(score) < MATE -100)
        {
             return score;
        }
    }

    pos->killers[pos->ply + 1][0] = 0;
    pos->killers[pos->ply + 1][1] = 0;
    MoveList move_list;
    move_list.num_of_moves = generate_moves(pos, move_list.moves, 1);
    score_moves(pos, &move_list, ttMove , 1); 

    int number_of_illegal_moves=0;
    int played = 0;
    uint16_t played_moves[256];
    for (int i=0 ; i < move_list.num_of_moves ; i++)
    {

        played_moves[played] = move = pick_move(&move_list,i);
        if(best_score > -2000 && !inCheck && !PVNode && move_type(move) < 2 && played > 2)
        {
            int skip= 0;
            //lmp
            if( depth <=5 && played > 10 + depth*5)
                skip = 1;
 
            //see pruning
            else if( depth <=5 && SEE(pos,move) < -50*depth)
                skip = 1;

            // futility pruning
            else if(  depth <=8  && eval + depth*40 + 200 < alpha)
                skip = 1;

            // history pruning
            else if( depth <=5 && get_history(pos, move) < -10000)
                skip = 1;

            if(skip)
            {
                played++;
                continue;
            }
        }
        int see_reduction = (played >1 && depth > 2 && SEE(pos, move) < 0);
        make_move(pos, move);
        if( !is_legal(pos) )
        {
            number_of_illegal_moves++;
            unmake_move(pos, move );
            continue;
        }
        lmr=1;
        if( played >1 && depth > 2  && move_type(move) < 2 )
        {
            lmr =  1.75 + log(depth)*log(played)/2.25; 
            lmr -= PVNode; //reduce less for PV nodes
            lmr += !improving;
        }
        lmr += see_reduction;
        if(move_type(move) == CAPTURE)
        {
            lmr -= get_capture_history(pos, move)/5000;
        }
        lmr =  MAX(1, MIN(depth-1 , lmr)); 

        if(played >=1 )// search with null window centered at alpha to prove the move fails low.
        {
            score= -AlphaBeta( -(alpha+1), -alpha, pos, depth -lmr, info);
            if(score > alpha && score < beta)// if the score is inside (alpha, beta) do research with an open window
            {
                score = -AlphaBeta( -beta, -alpha, pos, depth -1, info);
            }
        }
        // search only the first move with open window
        else {
            score = -AlphaBeta( -beta, -alpha, pos, depth -1, info );
        }
        unmake_move(pos, move );
        played++;

        if(info->stopped == TRUE)
        {
            return 0;
        }
        if (score > best_score)
        {
            best_move = move;
            best_score = score;
            if(rootNode)
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
                update_histories(pos, depth , played_moves, played );
            }
            if(move_type(move) <= CAPTURE)
            {
                update_capture_histories(pos, depth , played_moves, played );
            }
            tt_save(pos,best_score, TT_BETA, depth, move);
            return best_score;
        }
    }
    if(move_list.num_of_moves == number_of_illegal_moves)
    {
        if( inCheck)
        {
            return - (MATE - pos->ply);//ckeckmate

        }
        else
        {
            return 0; //stalemate
        }
    }
    if(alpha > oldalpha) // It raises alpha
    {
        tt_save(pos,best_score, TT_EXACT, depth,best_move);
    }
    else
    {
        tt_save(pos, best_score,TT_ALPHA, depth,best_move);
    }
    return best_score;
}
int getPV(Position* pos, int depth) //get PV from hash table. It is generally shorter than actual PV.
{
    if(depth == 0)
    {
        return 1;
    }
    TTentry* entry = tt_probe(pos->key);
    if( entry)
    {
        uint16_t move = entry->move;
        if(move == 0)
            return 0;
        MoveList move_list;
        move_list.num_of_moves = generate_moves(pos, move_list.moves, 1);
        for(int i=0 ; i <= move_list.num_of_moves ; i++)
        {
           if(i ==  move_list.num_of_moves)
                return 0;
            if(move_list.moves[i] == move)
                break;
        }
        make_move(pos, move);
        if( is_legal(pos ) == 0)
        {
            unmake_move(pos, move );
            return 0;
        }
        print_move(move);
        getPV(pos,depth -1);
        unmake_move(pos,move);
        return 1;
    }
    return 0;
}
void search(Position* pos, SearchInfo* info )
{
    pos->ply = 0;
    pos->nodes = 0ull;
    info->stopped = info->quit = FALSE;
    long long int start_t= timeInMilliseconds();
    uint16_t best_move = 0;
    float branching_factor = 1.0;
    int score = 0;
    for(int i=1 ; i <= info->fixed_depth ; i++)
    {
        info->search_depth = i;
        if(i >= 4 ) // aspiration window search
        {
            int window_size=20;
            score = AlphaBeta((info->score_history[i -1 ]  -window_size), info->score_history[i -1 ] + window_size, pos , i, info);
            while(1)
            {
                if(info->stopped)
		            break;
                if(score >= info->score_history[i -1 ] + window_size) 
                {
                    score = AlphaBeta(info->score_history[i -1 ] -2*window_size, info->score_history[i -1 ] + 2*window_size, pos,i,info);
                }
                else if(score <= info->score_history[i -1 ]-window_size)
                {
                    score = AlphaBeta((info->score_history[i -1 ] - 2*window_size), info->score_history[i -1 ] + 2*window_size , pos, i,info);
                }
                else
                {
                    break;
                }
                window_size *=2;
            }
        }
        else
        {
            score = AlphaBeta(-INF, INF, pos, i, info);
        }
        if(info->stopped)
            break;
        info->score_history[i] = score;
        info->bestmove_history[i] = best_move =  pos->bestmove;
        info->node_history[i] = pos->nodes;
        long long int elapsed = (timeInMilliseconds() + 1 - start_t);
        int nps= (info->node_history[i] * 1000) / elapsed;

        if(i >= 10)
            branching_factor =info->node_history[i - 1]/((float)info->node_history[i- 2]);

        if( abs(score) < MATE - MAX_DEPTH)
        {
            printf("info depth %d nps %d nodes %llu score cp %d time %lld pv ",i ,nps,  info->node_history[i], 2*info->score_history[i]/3, elapsed);      
        }
        else
        {
            int mate= (MATE-abs(score) +1)* (2*(score > 0) -1 ) /2;
            printf("info depth %d nps %d nodes %llu score mate %d time %lld pv ",i ,nps,  info->node_history[i], mate, elapsed);
             
        }   
        getPV(pos, i);
        printf("\n");
        fflush(stdout);
        if(info->search_type == NORMAL_GAME && (timeInMilliseconds() - info->start_time)*branching_factor > (info->stop_time - info->start_time))
        {
            break ;
        }
    }

    printf("bestmove ");
    print_move(best_move);
    printf("\n");
    fflush(stdout);
    update_age();
}
int InputAvaliable()
{
    #ifndef WIN32
        struct timeval tv;
        fd_set readfds;

        FD_ZERO (&readfds);
        FD_SET (fileno(stdin), &readfds);
        tv.tv_sec=0; tv.tv_usec=0;
        select(16, &readfds, 0, 0, &tv);

        return (FD_ISSET(fileno(stdin), &readfds));
    #else
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
    #endif
}
int UciCheck(SearchInfo* info)
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
