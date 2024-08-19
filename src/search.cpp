#include "search.h"
#include "threadData.h"
#include "tt.h"
#include "move.h"
#include "movegen.h"
#include "history.h"
#include "util.h"
#include <sstream>
#include "tuning.h"
#include "fathom/src/tbprobe.h"

int LMR_TABLE[MAX_PLY][256];
int seeThreshold(bool quiet, int depth)
{
    if(quiet) {
        return -97*depth;
    }
    else
        return -324*depth;
}
void Search::initSearchParameters() {
    for (int i = 0; i < MAX_PLY; i++) {
        for (int j = 0; j < 256; j++) {
            if (i >= 1 && j >= 2)
                LMR_TABLE[i][j] = 0.28 + log(i) * log(j - 1) / 2.57;
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
uint32_t probeTB(Board& pos) {
    if ( pos.halfMove != 0 || pos.castlings != 0 || (popcount64(pos.occupied[WHITE] | pos.occupied[BLACK]) > TB_LARGEST))
        return TB_RESULT_FAILED;

    auto white = pos.occupied[WHITE];
    auto black = pos.occupied[BLACK];

    auto knights= pos.bitboards[WHITE_KNIGHT] | pos.bitboards[BLACK_KNIGHT];
    auto bishops= pos.bitboards[WHITE_BISHOP] | pos.bitboards[BLACK_BISHOP];
    auto queens = pos.bitboards[WHITE_QUEEN]  | pos.bitboards[BLACK_QUEEN];
    auto kings  = pos.bitboards[WHITE_KING]   | pos.bitboards[BLACK_KING];
    auto rooks   = pos.bitboards[WHITE_ROOK]   | pos.bitboards[BLACK_ROOK];
    auto pawns  = pos.bitboards[WHITE_PAWN]   | pos.bitboards[BLACK_PAWN];

    return tb_probe_wdl(
            white, black,
            kings, queens, rooks,
            bishops, knights, pawns,
            pos.halfMove,
            pos.castlings,
            pos.enPassant == NO_SQ ? 0 : pos.enPassant,
            pos.sideToMove == WHITE);
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
    doubleExtension = 0;
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
    for (auto th:threads)
    {
        delete th;
    }
    threads.clear();
    for (int i = 0; i < numThread; i++)
    {
        auto th = new ThreadData(START_FEN, i);
        threads.emplace_back(th);
    }

}

Search::~Search() = default;

void Search::stop() {
    stopped = true;
}

int Search::qsearch(int alpha, int beta, ThreadData &thread, Stack *ss) {
    int oldAlpha = alpha;
    Board *board = &thread.board;
    int PVNode = (alpha != beta - 1);

    thread.nodes++;

    //TT Probing
    int ttDepth=0, ttScore=0, ttBound = TT_NONE, ttStaticEval=0;
    uint16_t ttMove = NO_MOVE;
    bool ttHit = TT::Instance()->ttProbe(board->key, ss->ply, ttDepth, ttScore, ttBound, ttStaticEval, ttMove);
    if (ttHit && !PVNode) {
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

    int rawEval = (ttHit && ttStaticEval != SCORE_NONE) ? ttStaticEval : board->eval();
    auto standPat = adjustEvalWithCorrHist(thread,rawEval);

    //ttValue can be used as a better position evaluation
    if (ttHit && (ttBound & (ttScore > standPat ? TT_LOWERBOUND : TT_UPPERBOUND)))
    {
        standPat = ttScore;
    }

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
        
        if(this->stopped)
            return 0;

        if (score > bestScore) {
            bestScore = score;
            
            if (score > alpha)
            {
                bestMove = move;
                alpha = score;
            }
            if (bestScore >= beta)
                break;

            
        }
    }

    TT_BOUND bound = bestScore >= beta ? TT_LOWERBOUND : TT_UPPERBOUND;
    TT::Instance()->ttSave(board->key, ss->ply, bestScore, rawEval, bound, 0, bestMove);
    return bestScore;
}

int Search::alphaBeta(int alpha, int beta, int depth, const bool cutNode, ThreadData &thread, Stack *ss) {
    int oldAlpha = alpha;
    int bestScore = -VALUE_INFINITE;
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
    int ttDepth=0, ttScore=0, ttBound=TT_NONE, ttStaticEval=0;
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

    // Probe tablebases
    uint32_t tbResult = (rootNode || ss->excludedMove) ? TB_RESULT_FAILED : probeTB(*board);

    if (TB_RESULT_FAILED != tbResult) {

        thread.tbHits++;
        int tbScore;


        TT_BOUND bound;

        if (tbResult == TB_LOSS) {
            tbScore = -(TB_SCORE - ss->ply);
            bound = TT_UPPERBOUND;
        }
        else if (tbResult == TB_WIN) {
            tbScore = TB_SCORE - ss->ply;
            bound = TT_LOWERBOUND;
        }
        else {
            tbScore = 0;
            bound = TT_EXACT;
        }

        if ((bound == TT_EXACT) || (bound == TT_LOWERBOUND ? tbScore >= beta : tbScore <= alpha)) {

            TT::Instance()->ttSave(board->key, ss->ply, tbScore, SCORE_NONE, bound, depth, NO_MOVE);
            return tbScore;
        }

        if (PVNode) {
            bestScore = tbScore;
            if (ttBound == TT_LOWERBOUND)
            {
                alpha = std::max(alpha, tbScore);
            }
        }
    }

    int rawEval = (ttHit && ttStaticEval != SCORE_NONE) ? ttStaticEval : board->eval();
    int eval = ss->staticEval = adjustEvalWithCorrHist(thread,rawEval);

    bool improving = !inCheck && ss->staticEval > (ss - 2)->staticEval;

    //ttValue can be used as a better position evaluation
    if (ttHit && (ttBound & (ttScore > eval ? TT_LOWERBOUND : TT_UPPERBOUND)))
        eval = ttScore;

    //IIR
    if (!ttHit && depth >= 3 && !PVNode)
        depth -= 1;

    //Reverse Futility Pruning
    if (!PVNode && !inCheck && ss->excludedMove == NO_MOVE && depth <= 8 && eval > beta + depth * 107 && !rootNode) {
        return eval;
    }

    //Razoring
    if (!PVNode && !inCheck && ss->excludedMove == NO_MOVE && depth <= 4 && eval + 408 * depth < alpha) {
        int score = qsearch(alpha, beta, thread,ss);
        if(score < alpha)
            return score;
    }
    (ss + 1)->excludedMove = NO_MOVE;
    (ss + 1)->killers[0] = NO_MOVE;
    (ss + 1)->killers[1] = NO_MOVE;
    ss->doubleExtension = (ss-1)->doubleExtension;

    int score;

    //Null Move pruning
    if (!PVNode && ss->excludedMove == NO_MOVE && (ss - 1)->move != NULL_MOVE && !inCheck && depth >= 2 && eval > beta &&
        board->hasNonPawnPieces()) {
        int R = 4 + depth / 4 + std::min(3, (eval - beta) / 177);

        ss->move = NULL_MOVE;
        ss->continuationHistory = &thread.contHist[PAWN][A1];
        board->makeNullMove();

        score = -alphaBeta(-beta, -beta + 1, depth - R, !cutNode, thread, ss + 1);

        board->unmakeNullMove();
        if (score >= beta)
            return score < MIN_MATE_SCORE ? score : beta;
    }

    //probcut
    auto  probCutBeta = beta + 200 - 50 * improving;
      if (!PVNode && depth > 3
        && std::abs(beta) < MIN_MATE_SCORE
        && ss->excludedMove == NO_MOVE
        && !(ttDepth >= depth - 3 && ttScore < probCutBeta))
    {
        auto moveList = MoveList(ttMove);
        legalmoves<TACTICAL_MOVES>(*board, moveList);
        uint16_t move =NO_MOVE;

        
        while ((move = moveList.pickMove(thread, ss)) != NO_MOVE) {
            if(move == ss->excludedMove)
                continue;

            ss->move = move;
            ss->playedMoves[ss->played++] = move;
            ss->continuationHistory = &thread.contHist[board->pieceBoard[moveFrom(move)]][moveTo(move)];

            board->makeMove(move);

            auto score = -qsearch(-probCutBeta, -probCutBeta + 1, thread,ss);

            if(score >= probCutBeta)
                score = -alphaBeta(-probCutBeta, -probCutBeta + 1,depth -4, !cutNode,thread,ss);

            board->unmakeMove(move);

            if(this->stopped)
                return 0;

            if(score >= probCutBeta)
            {
                TT::Instance()->ttSave(board->key, ss->ply, score, rawEval, TT_LOWERBOUND, depth -3 , move);
                return score;
            }
        }
    }

    auto moveList = MoveList(ttMove);
    legalmoves<ALL_MOVES>(*board, moveList);

    // checkmate or stalemate
    if (0 == moveList.numMove) {
        return inCheck ? -(MAX_MATE_SCORE - ss->ply) : 0;
    }
    int lmr;

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
            if (depth <= 6 && ss->played > 6 + (2 + 2 * improving) * depth)
                continue;

            // futility pruning
            if (depth <= 10 && eval + std::max(192, -(ss->played) * 10 + 192 + depth * 109) < alpha)
                continue;

            //contHist pruning
            int contHist = getContHistory(thread,ss, move);
            if(depth <= 3 && contHist < -3633 )
                continue;
        }
        if(ss->played > 3 && !PVNode && depth <= 5 && !SEE(*board, move, seeThreshold(isQuiet(move), depth))) {
            continue;
        }
        int history = 0;
        lmr = 0;
        if (ss->played > 2 && depth > 2) {
            lmr = LMR_TABLE[depth][ss->played];
            lmr -= PVNode; //reduce less for PV nodes
            lmr += !improving;
            
            if(isQuiet(move))
                history = getQuietHistory(thread,ss, move);
            else
                history = getCaptureHistory(thread,ss, move);

            lmr -= std::clamp(history/8474, -2,2);
            lmr += cutNode;
            lmr += ttMove && isTactical(ttMove);
        }
        
        lmr = std::max(0, std::min(depth - 1, lmr));
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
            const int singularBeta = ttScore - 4*depth;
            const int singularDepth = (depth - 1) / 2;

            ss->excludedMove = move;
            int singularScore = alphaBeta(singularBeta - 1, singularBeta , singularDepth, cutNode, thread, ss);
            ss->excludedMove = NO_MOVE;

            if(singularScore < singularBeta) {
                extension = 1;
                int margin = 300 * PVNode - 200 * !isTactical(ttMove);
                if( (singularScore + margin < singularBeta) && ss->doubleExtension <= 5)
                {
                    ss->doubleExtension = (ss-1)->doubleExtension + 1;
                    extension++;
                }
            }
            else if(singularScore >= beta )
            {
                return singularScore;
            }

            //negative extensions
            else if(ttScore >= beta)
                extension = -2;

            else if(cutNode)
                extension = -1;

            //reAssign some stack values that might have been changed
            ss->played = 0;
            ss->move = move;
            ss->playedMoves[ss->played++] = move;
            ss->continuationHistory = &thread.contHist[board->pieceBoard[moveFrom(move)]][moveTo(move)];
        }
        int newDepth = depth - 1 + extension;
        int d = newDepth -lmr;
        //make move
        board->makeMove(move);
        if(lmr >= 1)
        {
            score = -alphaBeta(-alpha - 1, -alpha, d, true, thread, ss + 1);
            if (score > alpha && d < newDepth) {

                const bool doDeeperSearch = score > (bestScore + 35 + 2 * newDepth);
                const bool doShallowerSearch = score < bestScore + newDepth;

                newDepth += doDeeperSearch - doShallowerSearch;

                if (newDepth > d)
                    score = -alphaBeta(-alpha - 1, -alpha, newDepth, !cutNode, thread, ss + 1);
            }
        }
        else if (!PVNode || ss->played > 1) {
            score = -alphaBeta(-alpha - 1, -alpha, newDepth, !cutNode, thread, ss + 1);
        }

        if(PVNode && (ss->played == 1 || score > alpha))
        {
            score = -alphaBeta(-beta, -alpha, newDepth, false, thread, ss + 1);
        }
        board->unmakeMove(move);

        if(this->stopped)
            return 0;
        
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

        TT::Instance()->ttSave(board->key, ss->ply, bestScore, rawEval, bound, depth, bestMove);
    }
    return bestScore;
}

SearchResult Search::start(Board *board, TimeManager *tm, int ThreadID) {
    SearchResult res{};
    std::vector<std::thread> runningThreads;
    if (ThreadID == 0) {
        stopped = false;

        for (int i = 0; i < numThread; i++)
        {
            threads.at(i)->nodes       = 0ull;
            threads.at(i)->tbHits       = 0ull;
            threads.at(i)->searchDepth = 0;
            threads.at(i)->board       = *board;
        }

        this->timeManager = tm;
        for (int i = 1; i < numThread; i++) {
            runningThreads.emplace_back(&Search::start, this, board, tm, i);
        }
    }

    auto *ss = new Stack[MAX_PLY + 10];
    for (int i = 0; i < MAX_PLY + 10; i++) {
        (ss + i)->ply = i - 6;
        (ss + i)->continuationHistory = &threads.at(ThreadID)->contHist[0][0];
    }

    int score = 0;
    for (int i = 1; i <= timeManager->depthLimit; i++) {
        threads.at(ThreadID)->searchDepth = i;
        // aspiration window search
        if (i > 4) {
            int windowSize = 20;
            int alpha = score - windowSize;
            int beta = score + windowSize;
            while (true) {
                score = alphaBeta(alpha, beta, i, false, *threads.at(ThreadID), ss + 6);
                if (stopped || (score > alpha && score < beta))
                    break;
                if (score <= alpha)
                    alpha = std::max(-VALUE_INFINITE, alpha - windowSize);
                else if (score >= beta)
                    beta = std::min(+VALUE_INFINITE, beta + windowSize);

                windowSize += windowSize/3;
            }
        } else {
            score = alphaBeta(-VALUE_INFINITE, VALUE_INFINITE, i, false, *threads.at(ThreadID), ss + 6);
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
                      std::cout << " nps " << nps
                      << " nodes " << nodes
                      << " time " << elapsed
                      << " hashfull " << TT::Instance()->getHashfull()
                      << " tbhits "   << totalTbHits()
                      << " pv " << getPV(ss + 6, threads.at(ThreadID)->board)
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
        sum += threads[i]->nodes;
    }
    return sum;
}

uint64_t Search::totalTbHits() {
    uint64_t sum = 0ull;
    for (int i = 0; i < numThread; i++) {
        sum += threads[i]->tbHits;
    }
    return sum;
}
