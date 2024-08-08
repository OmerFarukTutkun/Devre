#include "search.h"
#include "threadData.h"
#include "tt.h"
#include "move.h"
#include "movegen.h"
#include "history.h"
#include "util.h"
#include <sstream>
int LMR_TABLE[MAX_PLY][256];
int seeThreshold(bool quiet, int depth)
{
    if(quiet) {
        return -100*depth;
    }
    else
        return -300*depth;
}
void Search::initSearchParameters() {
    for (int i = 0; i < MAX_PLY; i++) {
        for (int j = 0; j < 256; j++) {
            if (i >= 1 && j >= 2)
                LMR_TABLE[i][j] = 1.25 + log(i) * log(j - 1) / 2.50;
            else
                LMR_TABLE[i][j] = 0;
        }
    }
}

void updatePv(Stack *ss) {
    auto pv = ss->pv;
    auto childPv = (ss + 1)->pv;

    pv[0] = ss->move;
    auto i = 0;
    for (; i < MAX_PLY; i++) {
        if (childPv[i] != NO_MOVE)
            pv[i + 1] = childPv[i];
        else
            break;
    }
    pv[i + 1] = NO_MOVE;
}

std::string getPV(Stack *stack, Board &board) {
    std::stringstream ss;
    auto pv = stack->pv;
    for (int i = 0; i < MAX_PLY; i++) {
        if (pv[i] == NO_MOVE)
            break;
        ss << moveToUci(pv[i], board) + " ";
    }
    return ss.str();
}
Stack::Stack() {
    excludedMove = NO_MOVE;
    staticEval = VALUE_INFINITE;
    move = NO_MOVE;
    threat = 0ull;
    killers[0] = NO_MOVE;
    killers[1] = NO_MOVE;
    pv[0] = NO_MOVE;
    played = 0;
}

Search::Search() {
    timeManager = nullptr;
    numThread = 1;
    threads.clear();
    stopped = false;
    m_bestMove = NO_MOVE;
    initSearchParameters();
}

void Search::setThread(int thread) {
    numThread = thread;
}

Search::~Search() = default;

void Search::stop() {
    stopped = true;
}

int Search::qsearch(int alpha, int beta, ThreadData &thread, Stack *ss) {
    int oldAlpha = alpha;
    Board *board = &thread.board;

    thread.nodes++;

    //TT Probing
    int ttDepth=0, ttScore=0, ttBound = TT_NONE, ttStaticEval=0;
    uint16_t ttMove = NO_MOVE;
    bool ttHit = TT::Instance()->ttProbe(board->key, ss->ply, ttDepth, ttScore, ttBound, ttStaticEval, ttMove);
    if (ttHit) {
        if (ttBound == TT_EXACT
            || (ttBound == TT_UPPERBOUND && alpha >= ttScore)
            || (ttBound == TT_LOWERBOUND && beta <= ttScore))
            return ttScore;
    }

    if (board->isDraw())
        return 0;

    if (ss->ply > MAX_PLY) {
        return board->eval();
    }

    auto standPat = ttHit ? ttStaticEval : board->eval();
    standPat = adjustEvalWithCorrHist(thread,standPat);

    if (standPat >= beta) {
        return standPat;
    }
    if (alpha < standPat) {
        alpha = standPat;
    }

    int bestScore = standPat, score;
    uint16_t move, bestMove = NO_MOVE;

    auto moveList = MoveList(ttMove);
    legalmoves<TACTICAL_MOVES>(*board, moveList);

    while ((move = moveList.pickMove(thread, ss, moveTypeScores[CAPTURE] - 5 * MIL))) {

        ss->move = move;
        board->makeMove(move);

        score = -qsearch(-beta, -alpha, thread, ss + 1);

        board->unmakeMove(move);

        if (score > bestScore) {
            bestScore = score;
            
        if (score > alpha) {
                bestMove = move;
                alpha = score;
            }
            if (bestScore >= beta) {
                break;
            }
            
        }
    }

    TT_BOUND bound = bestScore >= beta ? TT_LOWERBOUND : (alpha > oldAlpha ? TT_EXACT : TT_UPPERBOUND);
    TT::Instance()->ttSave(board->key, ss->ply, bestScore, standPat, bound, 0, bestMove);
    return bestScore;
}

int Search::alphaBeta(int alpha, int beta, int depth, ThreadData &thread, Stack *ss) {
    int oldAlpha = alpha;
    int PVNode = (alpha != beta - 1);
    int rootNode = (0 == ss->ply);

    ss->pv[0] = NO_MOVE;


    Board *board = &thread.board;

    if (stopped) {
        return 0;
    }
    if (thread.ThreadID == 0 && timeManager->checkLimits(totalNodes())) {
        stopped = true;
        return 0;
    }

    //Mate Distance Pruning
    int mateValue = MAX_MATE_SCORE - ss->ply;
    if (mateValue < beta) {
        beta = mateValue;
        if (alpha >= beta)
            return beta;
    }
    if (mateValue < -alpha) {
        alpha = -mateValue;
        if (alpha >= beta)
            return alpha;
    }

    //draw check
    if (!rootNode && board->isDraw()) {
        return  4 - (thread.nodes & 7);
    }
    if (ss->ply > MAX_PLY) {
        return board->eval();
    }
    //calculate opponent threats
    ss->threat = board->threat();
    //find we are in check or not by using opponent threat
    bool inCheck = board->inCheck(ss->threat);

    //check Extension
    if (!rootNode && inCheck) {
        depth = std::max(1, 1 + depth);
    }

    if (depth <= 0) {
        return qsearch(alpha, beta, thread, ss);
    }
    thread.nodes++;

    //TT Probing
    int ttDepth, ttScore, ttBound, ttStaticEval;
    uint16_t ttMove = NO_MOVE;
    bool ttHit = false;
    if( ss->excludedMove == NO_MOVE )
        ttHit = TT::Instance()->ttProbe(board->key, ss->ply, ttDepth, ttScore, ttBound, ttStaticEval, ttMove);

    if (ttHit  && !rootNode && !PVNode) {
        if (ttDepth >= depth) {
            if (ttBound == TT_EXACT
                || (ttBound == TT_UPPERBOUND && alpha >= ttScore)
                || (ttBound == TT_LOWERBOUND && beta <= ttScore))
                return ttScore;
        }
    }

    int eval = ttHit ? ttStaticEval : board->eval();
    ss->staticEval = adjustEvalWithCorrHist(thread,eval);

    bool improving = !inCheck && ss->staticEval > (ss - 2)->staticEval;

    //ttValue can be used as a better position evaluation
    if (ttHit && (ttBound & (ttScore > eval ? TT_LOWERBOUND : TT_UPPERBOUND)))
        eval = ttScore;

    //IIR
    if (!ttHit && depth >= 3 && !PVNode)
        depth -= 1;

    //Reverse Futility Pruning
    if (!PVNode && !inCheck && ss->excludedMove == NO_MOVE && depth <= 8 && eval > beta + depth * 115 && !rootNode) {
        return eval;
    }

    //Razoring
    if (!PVNode && !inCheck && ss->excludedMove == NO_MOVE && depth <= 4 && eval + 360 * depth < alpha) {
        int score = qsearch(alpha, beta, thread,ss);
        if(score < alpha)
            return score;
    }
    (ss + 1)->excludedMove = NO_MOVE;
    (ss + 1)->killers[0] = NO_MOVE;
    (ss + 1)->killers[1] = NO_MOVE;

    int score;

    //Null Move pruning
    if (!PVNode && ss->excludedMove == NO_MOVE && (ss - 1)->move != NULL_MOVE && !inCheck && depth >= 2 && eval > beta &&
        board->hasNonPawnPieces()) {
        int R = 4 + depth / 6 + std::min(3, (eval - beta) / 200);
        ss->move = NULL_MOVE;
        ss->continuationHistory = &thread.contHist[PAWN][A1];
        board->makeNullMove();
        score = -alphaBeta(-beta, -beta + 1, depth - R, thread, ss + 1);
        board->unmakeNullMove();
        if (score >= beta)
            return score < MIN_MATE_SCORE ? score : beta;
    }
    auto moveList = MoveList(ttMove);
    legalmoves<ALL_MOVES>(*board, moveList);

    // checkmate or stalemate
    if (0 == moveList.numMove) {
        return inCheck ? -(MAX_MATE_SCORE - ss->ply) : 0;
    }
    int lmr;
    int bestScore = -VALUE_INFINITE;
    uint16_t  bestMove = NO_MOVE, move =NO_MOVE;

    ss->played = 0;

    //loop moves
    while ((move = moveList.pickMove(thread, ss)) != NO_MOVE) {

        if(move == ss->excludedMove)
            continue;

        ss->move = move;
        ss->playedMoves[ss->played++] = move;

        if (isQuiet(move) && ss->played > 3 && !PVNode) {
            // late move pruning
            if (depth <= 6 && ss->played > 6 + (3 + 2 * improving) * depth)
                continue;

            // futility pruning
            if (depth <= 10 && eval + std::max(200, -(ss->played) * 10 + 200 + depth * 100) < alpha)
                continue;

            //contHist pruning
            int contHist = getContHistory(thread,ss, move);
            if(depth <= 4 && contHist < -3057 )
                continue;
        }
        if(ss->played > 3 && !PVNode && depth <= 5 && !SEE(*board, move, seeThreshold(isQuiet(move), depth))) {
            continue;
        }
        lmr = 1;
        if (ss->played > 2 && depth > 2 && isQuiet(move)) {
            lmr = LMR_TABLE[depth][ss->played];
            lmr -= PVNode; //reduce less for PV nodes
            lmr += !improving;
        }
        else if ( ss->played > 2 && depth > 2 && isTactical(move))
        {
            lmr += (ss->played > 10);
        }
        lmr = std::max(1, std::min(depth - 1, lmr));
        ss->continuationHistory = &thread.contHist[board->pieceBoard[moveFrom(move)]][moveTo(move)];

        int extension = 0;
        if( ss->ply < thread.searchDepth
            && !rootNode
            && depth >= 8
            && move == ttMove
            && ss->excludedMove == NO_MOVE
            && (ttBound & TT_LOWERBOUND)
            && ttDepth >= depth -3
        )
        {
            const int singularBeta = ttScore - 3*depth;
            const int singularDepth = (depth - 1) / 2;

            ss->excludedMove = move;
            int singularScore = alphaBeta(singularBeta - 1, singularBeta , singularDepth, thread, ss);
            ss->excludedMove = NO_MOVE;

            if(singularScore < singularBeta) {
                extension = 1;
                int margin = 300 * PVNode - 200 * !isTactical(ttMove);
                extension = 1 + (singularScore + margin < singularBeta);
            }
            else if(singularScore >= beta )
            {
                return singularScore;
            }

            //reAssign some stack values that might have been changed
            ss->played = 0;
            ss->move = move;
            ss->playedMoves[ss->played++] = move;
        }

        //make move
        board->makeMove(move);
        if (ss->played > 1) {
            score = -alphaBeta(-alpha - 1, -alpha, depth - lmr, thread, ss + 1);
            if (score > alpha && score < beta)// if the score is inside (alpha, beta) do research with an open window
            {
                score = -alphaBeta(-beta, -alpha, depth - 1, thread, ss + 1);
            }
        } else {
            score = -alphaBeta(-beta, -alpha, depth - 1 + extension, thread, ss + 1);
        }
        board->unmakeMove(move);

        if (score > bestScore) {

            bestScore = score;

            if (bestScore > alpha) {
                if (PVNode)
                    updatePv(ss);

                bestMove = move;
                alpha = bestScore;
            }

            if (bestScore >= beta) {
                updateHistories(thread, ss, depth, bestMove);
                break;
            }
        }
    }
    if(ss->excludedMove == NO_MOVE) {
        TT_BOUND bound = bestScore >= beta ? TT_LOWERBOUND : (alpha > oldAlpha ? TT_EXACT : TT_UPPERBOUND);

        if (    !inCheck
                && (!bestMove || !isTactical(bestMove))
                &&  !(bound == TT_LOWERBOUND && bestScore <= ss->staticEval)
                &&  !(bound == TT_UPPERBOUND && bestScore >= ss->staticEval)) {

            updateCorrHistScore(thread, depth, bestScore - ss->staticEval);
        }


        TT::Instance()->ttSave(board->key, ss->ply, bestScore, eval, bound, depth, bestMove);
    }
    return bestScore;
}

SearchResult Search::start(Board *board, TimeManager *tm, int ThreadID) {
    SearchResult res{};
    std::vector<std::thread> runningThreads;
    if (ThreadID == 0) {
        stopped = false;
        for (int i = 0; i < numThread; i++)
            threads.emplace_back(*board, i);

        this->timeManager = tm;
        for (int i = 1; i < numThread; i++) {
            runningThreads.emplace_back(&Search::start, this, board, tm, i);
        }
    }

    auto *ss = new Stack[MAX_PLY + 10];
    for (int i = 0; i < MAX_PLY + 10; i++) {
        (ss + i)->ply = i - 6;
        (ss + i)->continuationHistory = &threads.at(ThreadID).contHist[0][0];
    }

    int score = 0;
    for (int i = 1; i <= timeManager->depthLimit; i++) {
        threads.at(ThreadID).searchDepth = i;
        // aspiration window search
        if (i > 4) {
            int windowSize = 20;
            int alpha = score - windowSize;
            int beta = score + windowSize;
            while (true) {
                score = alphaBeta(alpha, beta, i, threads.at(ThreadID), ss + 6);
                if (stopped || (score > alpha && score < beta))
                    break;
                if (score <= alpha)
                    alpha = std::max(-VALUE_INFINITE, alpha - windowSize);
                else if (score >= beta)
                    beta = std::min(+VALUE_INFINITE, beta + windowSize);

                windowSize += windowSize/3;
            }
        } else {
            score = alphaBeta(-VALUE_INFINITE, VALUE_INFINITE, i, threads.at(ThreadID), ss + 6);
        }

        if (stopped)
            break;
        if (ThreadID == 0) {
            auto elapsed = 1 + currentTime() - this->timeManager->startTime;

            this->m_bestMove = (ss + 6)->pv[0];
            auto nodes = this->totalNodes();
            auto nps = (1000 * nodes) / elapsed;

            std::cout << " info depth " << i;
            if(abs(score) < MIN_MATE_SCORE) {
                std::cout<< " score cp " << score / 2;
            }
            else{
                int mate= (MAX_MATE_SCORE - abs(score) +1) *(2*(score > 0) -1 ) /2;
                std::cout<< " score mate " << mate;
            }
                      std::cout << " pv " << getPV(ss + 6, threads.at(ThreadID).board)
                      << " nps " << nps
                      << " nodes " << nodes
                      << " time " << elapsed
                      << " hashfull " << TT::Instance()->getHashfull()
                      << std::endl;

            //if our score is much higher than staticEval spend less time
            double x = (score - (ss + 6)->staticEval + 200) / 500.0;
            x = std::min(1.0, std::max(0.0 ,x));

            if (elapsed * (2.5 + x) > timeManager->optimalTime)
                break;
        }
    }
    delete[] ss;
    if (ThreadID == 0) {
        //wait other threads
        this->stop();
        for (std::thread &th: runningThreads) {
            th.join();
        }
        std::cout << "bestmove " << moveToUci(this->m_bestMove, *board) << std::endl;
        runningThreads.clear();
        threads.clear();

        res.cp = score / 2;
        res.move = this->m_bestMove;
        res.nodes = totalNodes();
        TT::Instance()->updateAge();
    }
    return res;
}
uint64_t Search::totalNodes() {
    uint64_t sum = 0ull;
    for (int i = 0; i < numThread; i++) {
        sum += threads[i].nodes;
    }
    return sum;
}
