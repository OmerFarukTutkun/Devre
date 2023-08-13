#include "search.h"
#include "ThreadData.h"
#include "TT.h"
#include "move.h"
#include "movegen.h"
#include "history.h"
#include "util.h"
#include <sstream>

void updatePv(Stack *ss) {
    auto pv = ss->pv;
    auto childPv = (ss + 1)->pv;

    pv[0] = ss->move;
    int i = 0;
    for (i = 0; i < MAX_PLY; i++) {
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
    bestMove = NO_MOVE;
}

void Search::setThread(int thread) {
    numThread = thread;
}

Search::~Search() {

}

void Search::stop() {
    stopped = true;
}

int Search::qsearch(int alpha, int beta, ThreadData &thread, Stack *ss) {
    int oldAlpha = alpha;
    Board *board = &thread.board;

    thread.nodes++;

    //TT Probing
    int ttDepth, ttScore, ttBound, ttStaticEval;
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

    //use TT score as eval TODO: test
    if (ttBound & (ttScore > standPat ? TT_LOWERBOUND : TT_UPPERBOUND))
        standPat = ttScore;

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
            bestMove = move;
            if (bestScore >= beta) {
                break;
            }
            if (score > alpha) {
                alpha = score;
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
    if (thread.ThreadID == 0 && timeManager->checkLimits()) {
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
        return 0;
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
    int ttDepth, ttScore, ttBound = TT_NONE, ttStaticEval;
    uint16_t ttMove = NO_MOVE;
    bool ttHit = TT::Instance()->ttProbe(board->key, ss->ply, ttDepth, ttScore, ttBound, ttStaticEval, ttMove);
    if (ttHit && !rootNode && !PVNode) {
        if (ttDepth >= depth) {
            if (ttBound == TT_EXACT
                || (ttBound == TT_UPPERBOUND && alpha >= ttScore)
                || (ttBound == TT_LOWERBOUND && beta <= ttScore))
                return ttScore;
        }
    }

    int eval = ttHit ? ttStaticEval : board->eval();

    //use TT score as eval TODO: test
    if (ttBound & (ttScore > eval ? TT_LOWERBOUND : TT_UPPERBOUND))
        eval = ttScore;

    ss->staticEval = eval;
    bool improving = !inCheck && ss->staticEval > (ss - 2)->staticEval;

    //IIR
    if (!ttHit && depth >= 4 && !PVNode)
        depth -= 1;

    //Reverse Futility Pruning
    if (!PVNode && !inCheck && depth <= 5 && eval > beta + depth * 125 && !rootNode) {
        return eval;
    }

    //Razoring TODO: test
    if (eval < alpha - 456 - 252 * depth * depth) {
        int value = qsearch(alpha - 1, alpha, thread, ss);
        if (value < alpha)
            return value;
    }

    (ss + 1)->killers[0] = NO_MOVE;
    (ss + 1)->killers[1] = NO_MOVE;

    int score;

    //Null Move pruning
    if (!PVNode && (ss - 1)->move != NULL_MOVE && !inCheck && depth >= 2 && eval > beta &&
        board->hasNonPawnPieces()) {
        int R = 4 + depth / 4 + std::min(3, (eval - beta) / 200);
        ss->move = NULL_MOVE;
        ss->continuationHistory = &thread.contHist[PAWN][A1];
        board->makeNullMove();
        score -= alphaBeta(-beta, -beta + 1, depth - R, thread, ss + 1);
        board->unmakeNullMove();
        if (score >= beta)
            return score < MIN_MATE_SCORE ? score : beta;
    }
    auto moveList = MoveList(ttMove);
    legalmoves<ALL_MOVES>(*board, moveList);

    //checkmate or stalemate
    if (0 == moveList.numMove) {
        return inCheck ? -(MAX_MATE_SCORE - ss->ply) : 0;
    }
    int lmr;
    int bestScore = -VALUE_INFINITE;
    uint16_t bestMove = NO_MOVE, move = NO_MOVE;

    ss->played = 0;

    //loop moves
    while ((move = moveList.pickMove(thread, ss)) != NO_MOVE) {

        ss->move = move;
        ss->playedMoves[ss->played++] = move;

        lmr = 1;
        if (ss->played > 2 && depth > 2 && isQuiet(move)) {
            lmr = 1.75 + log(depth) * log(ss->played - 1) / 2.25;
            lmr -= PVNode; //reduce less for PV nodes
            lmr += !improving;
        }

        if (isQuiet(move) && ss->played > 3 && !PVNode && !inCheck) {
            //lmp
            if (depth <= 5 && ss->played > 6 + (3 + 2 * improving) * depth)
                continue;

            // futility pruning
            if (depth <= 8 && eval + depth * 80 + 100 < alpha)
                continue;

            int history = getQuietHistory(thread, ss, move);
            //history pruning
            if (depth <= 4 && history < -4000 * depth)
                continue;

            //SEE pruning
            if (depth <= 5 && SEE(*board, move) < -30 * depth * depth)
                continue;

            int hisReduction = -history / 5000;
            lmr += std::max(-2, std::min(2, hisReduction));
        }
        lmr = std::max(1, std::min(depth - 1, lmr));

        ss->continuationHistory = &thread.contHist[board->pieceBoard[moveFrom(move)]][moveTo(move)];

        //make move
        board->makeMove(move);
        if (ss->played > 1) {
            score = -alphaBeta(-alpha - 1, -alpha, depth - lmr, thread, ss + 1);
            if (score > alpha && score < beta)// if the score is inside (alpha, beta) do research with an open window
            {
                score = -alphaBeta(-beta, -alpha, depth - 1, thread, ss + 1);
            }
        } else {
            score = -alphaBeta(-beta, -alpha, depth - 1, thread, ss + 1);
        }
        //unmake move
        board->unmakeMove(move);


        if (score > bestScore) {
            bestMove = move;
            bestScore = score;

            if (bestScore >= beta) {
                updateHistories(thread, ss, depth, moveList, bestMove);
                break;
            }
            if (bestScore > alpha) {
                if (PVNode)
                    updatePv(ss);
                alpha = bestScore;
            }
        }
    }
    TT_BOUND bound = bestScore >= beta ? TT_LOWERBOUND : (alpha > oldAlpha ? TT_EXACT : TT_UPPERBOUND);
    TT::Instance()->ttSave(board->key, ss->ply, bestScore, eval, bound, depth, bestMove);
    return bestScore;
}

void Search::start(Board *board, TimeManager *tm, int ThreadID) {
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

    int score;
    for (int i = 1; i <= timeManager->depthLimit; i++) {

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

                windowSize *= 2;
            }
        } else {
            score = alphaBeta(-VALUE_INFINITE, VALUE_INFINITE, i, threads.at(ThreadID), ss + 6);
        }

        if (stopped)
            break;
        if (ThreadID == 0) {
            auto elapsed = 1 + currentTime() - this->timeManager->startTime;

            this->bestMove = (ss + 6)->pv[0];
            auto nodes = this->totalNodes();
            auto nps = (1000 * nodes) / elapsed;
            std::cout << " info depth " << i
                      << " score cp " << score / 2
                      << " pv " << getPV(ss + 6, threads.at(ThreadID).board)
                      << " nps " << nps
                      << " nodes " << nodes
                      << " time " << elapsed
                      << std::endl;
            
            if (elapsed * 2 > timeManager->optimalTime)
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
        std::cout << "bestmove " << moveToUci(bestMove, *board) << std::endl;
        runningThreads.clear();
        threads.clear();
    }
}

uint64_t Search::totalNodes() {
    uint64_t sum = 0ull;
    for (int i = 0; i < numThread; i++) {
        sum += threads[i].nodes;
    }
    return sum;
}
