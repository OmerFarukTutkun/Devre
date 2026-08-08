#include "search.h"
#include "threadData.h"
#include "tt.h"
#include "move.h"
#include "movegen.h"
#include "movepick.h"
#include "history.h"
#include "util.h"
#include <sstream>
#include <limits>
#include "tuning.h"
#include "fathom/src/tbprobe.h"

DEFINE_PARAM_B(nodeTmBase, 150, 0, 350);
DEFINE_PARAM_B(nodeTmMultp, -100, -300, 0);

// Best-move stability soft-time scaling (percent). When the best root move keeps
// changing between iterations we spend more time; when it is stable we stop earlier.
DEFINE_PARAM_B(bmStabBase, 132, 80, 200);
DEFINE_PARAM_B(bmStabScale, 8, 0, 25);
DEFINE_PARAM_B(bmStabMin, 68, 40, 120);

int LMR_TABLE[MAX_PLY][256];

int seeThreshold(bool quiet, int depth) {
    if (quiet)
    {
        return -80 * depth;
    }
    else
        return -326 * depth;
}

void Search::initSearchParameters() {
    for (int i = 0; i < MAX_PLY; i++)
    {
        for (int j = 0; j < 256; j++)
        {
            if (i >= 1 && j >= 2)
                LMR_TABLE[i][j] = 0.17 + log(i) * log(j - 1) / 2.35;
            else
                LMR_TABLE[i][j] = 0;
        }
    }
}

void updatePv(Stack* ss) {
    auto *pv      = ss->pv;
    auto *childPv = (ss + 1)->pv;

    pv[0]  = ss->move;
    auto i = 0;
    for (; i < MAX_PLY; i++)
    {
        if (childPv[i] != NO_MOVE)
            pv[i + 1] = childPv[i];
        else
            break;
    }
    pv[i + 1] = NO_MOVE;
}

std::string getPV(Stack* stack, Board& board) {
    std::stringstream ss;
    auto *              pv = stack->pv;
    for (int i = 0; i < MAX_PLY; i++)
    {
        if (pv[i] == NO_MOVE)
            break;
        ss << moveToUci(pv[i], board) + " ";
    }
    return ss.str();
}

uint32_t probeTB(Board& pos) {
    if (pos.halfMove != 0 || pos.castlings != 0 || (popcount64(pos.occupied[WHITE] | pos.occupied[BLACK]) > TB_LARGEST))
        return TB_RESULT_FAILED;

    auto white = pos.occupied[WHITE];
    auto black = pos.occupied[BLACK];

    auto knights = pos.bitboards[WHITE_KNIGHT] | pos.bitboards[BLACK_KNIGHT];
    auto bishops = pos.bitboards[WHITE_BISHOP] | pos.bitboards[BLACK_BISHOP];
    auto queens  = pos.bitboards[WHITE_QUEEN] | pos.bitboards[BLACK_QUEEN];
    auto kings   = pos.bitboards[WHITE_KING] | pos.bitboards[BLACK_KING];
    auto rooks   = pos.bitboards[WHITE_ROOK] | pos.bitboards[BLACK_ROOK];
    auto pawns   = pos.bitboards[WHITE_PAWN] | pos.bitboards[BLACK_PAWN];

    return tb_probe_wdl(white, black, kings, queens, rooks, bishops, knights, pawns, pos.halfMove, pos.castlings, pos.enPassant == NO_SQ ? 0 : pos.enPassant, pos.sideToMove == WHITE);
}


Stack::Stack() : played(0), doubleExtension(0), move(NO_MOVE), staticEval(SCORE_NONE), threat(0ull), excludedMove(NO_MOVE) {
    killers[0]      = NO_MOVE;
    killers[1]      = NO_MOVE;
    pv[0]           = NO_MOVE;
}

Search::Search() :
    stopped(false),
    numThread(1),
    m_bestMove(NO_MOVE),
    moveNodes(new uint64_t[1 << 16])
{
    threads.clear();
    initSearchParameters();
}

void Search::setThread(int thread) {
    numThread = thread;
    for (auto* th : threads)
    {
        delete th;
    }
    threads.clear();
    for (int i = 0; i < numThread; i++)
    {
        auto* th = new ThreadData(START_FEN, i);
        threads.emplace_back(th);
    }
}

Search::~Search() {
    for (auto* th : threads)
    {
        delete th;
    }
    threads.clear();
    delete[] moveNodes;
}

void Search::stop() { stopped = true; }

int Search::qsearch(int alpha, int beta, ThreadData& thread, Stack* ss) {
    int    oldAlpha = alpha;
    Board* board    = &thread.board;
    int    PVNode   = (alpha != beta - 1);

    thread.nodes++;

    //TT Probing
    int      ttDepth = 0, ttScore = SCORE_NONE, ttBound = TT_NONE, ttStaticEval = SCORE_NONE;
    uint16_t ttMove = NO_MOVE;
    bool     ttHit  = TT::Instance()->ttProbe(board->key, ss->ply, ttDepth, ttScore, ttBound, ttStaticEval, ttMove);
    if (ttHit && !PVNode)
    {
        if (ttBound == TT_EXACT || (ttBound == TT_UPPERBOUND && alpha >= ttScore) || (ttBound == TT_LOWERBOUND && beta <= ttScore))
            return ttScore;
    }

    if (board->isDraw())
        return 0;

    if (ss->ply > seldepth)
    {
        seldepth = ss->ply;
    }
    if (ss->ply > MAX_PLY)
    {
        return board->eval();
    }

    bool inCheck = board->inCheck();

    int      rawEval = SCORE_NONE;
    int      bestScore, score;
    uint16_t move, bestMove = NO_MOVE;

    if (inCheck)
    {
        // When in check we cannot trust the static eval: search all evasions.
        bestScore = -VALUE_INFINITE;
    }
    else
    {
        rawEval = (ttStaticEval != SCORE_NONE) ? ttStaticEval : board->eval();

        if (!ttHit)
            TT::Instance()->ttSave(board->key, ss->ply, SCORE_NONE, rawEval, TT_NONE, 0, NO_MOVE);

        auto standPat = adjustEvalWithCorrHist(thread, ss, rawEval);

        //ttValue can be used as a better position evaluation
        if (ttHit && (ttBound & (ttScore > standPat ? TT_LOWERBOUND : TT_UPPERBOUND)))
        {
            standPat = ttScore;
        }

        if (standPat >= beta)
        {
            return standPat;
        }
        if (alpha < standPat)
        {
            alpha = standPat;
        }
        bestScore = standPat;
    }
    ss->threat = board->threat();
    MovePicker picker(thread, ss, ttMove, inCheck ? PICK_QSEARCH_CHECK : PICK_QSEARCH);
    int        movesSeen = 0;

    while ((move = picker.next()))
    {
        movesSeen++;

        if (!inCheck && move != ttMove && !SEE(*board, move))
            continue;
        ss->move = move;
        board->makeMove(move);

        score = -qsearch(-beta, -alpha, thread, ss + 1);

        board->unmakeMove(move);

        if (this->stopped)
            return 0;

        if (score > bestScore)
        {
            bestScore = score;

            if (score > alpha)
            {
                bestMove = move;
                alpha    = score;
            }
            if (bestScore >= beta)
                break;
        }
    }

    // checkmate
    if (inCheck && movesSeen == 0)
        return -(MAX_MATE_SCORE - ss->ply);

    TT_BOUND bound = bestScore >= beta ? TT_LOWERBOUND : TT_UPPERBOUND;
    TT::Instance()->ttSave(board->key, ss->ply, bestScore, rawEval, bound, 0, bestMove);
    return bestScore;
}

int Search::alphaBeta(int alpha, int beta, int depth, const bool cutNode, ThreadData& thread, Stack* ss) {
    int oldAlpha  = alpha;
    int bestScore = -VALUE_INFINITE;
    int PVNode    = (alpha != beta - 1);
    int rootNode  = (0 == ss->ply);

    ss->pv[0] = NO_MOVE;


    Board* board = &thread.board;

    if (stopped)
    {
        return 0;
    }
    // The first iteration is never aborted: it is what produces the move we are
    // going to send, and it costs a fraction of a millisecond.
    if (thread.ThreadID == 0 && thread.searchDepth > 1 && timeManager->shouldCheck() && timeManager->checkLimits(totalNodes()))
    {
        stopped = true;
        return 0;
    }

    //Mate Distance Pruning
    int mateValue = MAX_MATE_SCORE - ss->ply;
    if (mateValue < beta)
    {
        beta = mateValue;
        if (alpha >= beta)
            return beta;
    }
    if (mateValue < -alpha)
    {
        alpha = -mateValue;
        if (alpha >= beta)
            return alpha;
    }

    //draw check
    if (!rootNode && board->isDraw())
    {
        return 4 - (thread.nodes & 7);
    }

    if (ss->ply > seldepth)
    {
        seldepth = ss->ply;
    }
    if (ss->ply > MAX_PLY)
    {
        return board->eval();
    }

    bool inCheck = board->inCheck();

    //check Extension
    if (!rootNode && inCheck)
    {
        depth = std::max(1, 1 + depth);
    }

    if (depth <= 0)
    {
        return qsearch(alpha, beta, thread, ss);
    }
    thread.nodes++;

    //TT Probing
    int      ttDepth = 0, ttScore = SCORE_NONE, ttBound = TT_NONE, ttStaticEval = SCORE_NONE;
    uint16_t ttMove = NO_MOVE;
    bool     ttHit  = false;
    if (ss->excludedMove == NO_MOVE)
        ttHit = TT::Instance()->ttProbe(board->key, ss->ply, ttDepth, ttScore, ttBound, ttStaticEval, ttMove);

    if (ttHit && !rootNode && !PVNode)
    {
        if (ttDepth >= depth)
        {
            if (ttBound == TT_EXACT || (ttBound == TT_UPPERBOUND && alpha >= ttScore) || (ttBound == TT_LOWERBOUND && beta <= ttScore))
                return ttScore;
        }
    }
    const bool ttCapture = isTactical(ttMove);

     ss->threat = board->threat();
    // Probe tablebases
    uint32_t tbResult = (rootNode || ss->excludedMove) ? TB_RESULT_FAILED : probeTB(*board);

    if (TB_RESULT_FAILED != tbResult)
    {

        thread.tbHits++;
        int tbScore;


        TT_BOUND bound;

        if (tbResult == TB_LOSS)
        {
            tbScore = -(TB_SCORE - ss->ply);
            bound   = TT_UPPERBOUND;
        }
        else if (tbResult == TB_WIN)
        {
            tbScore = TB_SCORE - ss->ply;
            bound   = TT_LOWERBOUND;
        }
        else
        {
            tbScore = 0;
            bound   = TT_EXACT;
        }

        if ((bound == TT_EXACT) || (bound == TT_LOWERBOUND ? tbScore >= beta : tbScore <= alpha))
        {

            TT::Instance()->ttSave(board->key, ss->ply, tbScore, SCORE_NONE, bound, depth, NO_MOVE);
            return tbScore;
        }

        if (PVNode)
        {
            bestScore = tbScore;
            if (ttBound == TT_LOWERBOUND)
            {
                alpha = std::max(alpha, tbScore);
            }
        }
    }

    int rawEval = SCORE_NONE;
    int eval    = SCORE_NONE;

    if (inCheck)
    {
        ss->staticEval = SCORE_NONE;
    }
    else
    {
        rawEval = (ttStaticEval != SCORE_NONE) ? ttStaticEval : board->eval();
        eval = ss->staticEval = adjustEvalWithCorrHist(thread, ss, rawEval);

        if (!ttHit)
            TT::Instance()->ttSave(board->key, ss->ply, SCORE_NONE, rawEval, TT_NONE, 0, NO_MOVE);
    }

    bool improving = false;
    if (!inCheck)
    {
        if ((ss - 2)->staticEval != SCORE_NONE)
            improving = ss->staticEval > (ss - 2)->staticEval;
        else if ((ss - 4)->staticEval != SCORE_NONE)
            improving = ss->staticEval > (ss - 4)->staticEval;
    }

    //ttValue can be used as a better position evaluation
    if (ttHit && (ttBound & (ttScore > eval ? TT_LOWERBOUND : TT_UPPERBOUND)))
        eval = ttScore;

    //IIR
    if (!ttHit && depth >= 2)
        depth -= 1;
    else if (!PVNode && depth >= 8 && ttMove != NO_MOVE && ttDepth + 4 <= depth)
        depth -= 1;


    if (!rootNode && !PVNode && !inCheck && ss->excludedMove == NO_MOVE && depth <= 8 && std::abs(eval) < MIN_TB_SCORE)
    {
        const int rfpDepth  = std::max(0, depth - improving);
        const int rfpMargin = 115 * rfpDepth;

        if (eval - rfpMargin >= beta)
            return (eval + beta) / 2;
    }

    //Razoring
    if (!PVNode && !inCheck && ss->excludedMove == NO_MOVE && depth <= 4 && eval + 441 * depth < alpha)
    {
        int score = qsearch(alpha, beta, thread, ss);
        if (score < alpha)
            return score;
    }
    (ss + 1)->excludedMove = NO_MOVE;
    (ss + 1)->killers[0]   = NO_MOVE;
    (ss + 1)->killers[1]   = NO_MOVE;
    ss->doubleExtension    = (ss - 1)->doubleExtension;

    int score;

    //Null Move pruning
    if (!PVNode && ss->excludedMove == NO_MOVE && (ss - 1)->move != NULL_MOVE && !inCheck && depth >= 3 && eval > beta && board->hasNonPawnPieces())
    {
        int R = 5 + depth / 3 + std::min(4, (eval - beta) / 155);

        ss->move                = NULL_MOVE;
        ss->continuationHistory = &thread.contHist[PAWN][A1];
        ss->contCorrHist        = &thread.contCorrHist[PAWN][A1];
        board->makeNullMove();

        score = -alphaBeta(-beta, -beta + 1, depth - R, !cutNode, thread, ss + 1);

        board->unmakeNullMove();
        if (score >= beta)
            return score < MIN_MATE_SCORE ? score : beta;
    }
    MovePicker picker(thread, ss, ttMove, PICK_MAIN);
    uint64_t   beforeNodes = 0;
    int        lmr;
    uint16_t   bestMove = NO_MOVE, move = NO_MOVE;

    int moveCount = 0;
    ss->played    = 0;

    //loop moves
    while ((move = picker.next()) != NO_MOVE)
    {

        if (move == ss->excludedMove)
            continue;

        moveCount++;

        if (rootNode)
            beforeNodes = thread.nodes;
        ss->move = move;

        if (isQuiet(move) && moveCount > 3 && !PVNode)
        {
            // late move pruning. Both this and the futility margin below only get
            // stricter as moveCount grows, so the picker can drop every quiet
            // move still to come instead of generating and scoring them.
            if (depth <= 7 && moveCount > 6 + (3 + 3 * improving) * depth)
            {
                picker.skipQuiets();
                continue;
            }

            // futility pruning
            if (depth <= 10 && eval + std::max(180, -moveCount * 10 + 180 + depth * 110) < alpha)
            {
                picker.skipQuiets();
                continue;
            }

            //contHist pruning
            int contHist = getContHistory(thread, ss, move);
            if (depth <= 4 && contHist < -3410)
                continue;
        }
        if (moveCount > 1 && !PVNode && depth <= 6 && !SEE(*board, move, seeThreshold(isQuiet(move), depth)))
        {
            continue;
        }

        ss->playedMoves[ss->played++] = move;

        int history = 0;
        lmr         = 0;
        if (moveCount > 3 && depth > 2)
        {
            lmr = LMR_TABLE[depth][moveCount];
            lmr -= PVNode;  //reduce less for PV nodes
            lmr += !improving;

            if (isQuiet(move))
                history = getQuietHistory(thread, ss, move);
            else
                history = getCaptureHistory(thread, ss, move);

            lmr -= std::clamp(history / 8078, -2, 2);
            lmr += cutNode;
            lmr += ttMove && ttCapture;
            lmr -= std::abs(ss->staticEval - rawEval) > 329;
        }

        lmr                     = std::max(0, std::min(depth - 1, lmr));
        ss->continuationHistory = &thread.contHist[board->pieceBoard[moveFrom(move)]][moveTo(move)];
        ss->contCorrHist        = &thread.contCorrHist[board->pieceBoard[moveFrom(move)]][moveTo(move)];

        int extension = 0;
        if (ss->ply < thread.searchDepth && !rootNode && depth >= 8 && move == ttMove && ss->excludedMove == NO_MOVE && (ttBound & TT_LOWERBOUND) && ttDepth >= depth - 3)
        {
            const int singularBeta  = ttScore - 4 * depth;
            const int singularDepth = (depth - 1) / 2;

            ss->excludedMove  = move;
            int singularScore = alphaBeta(singularBeta - 1, singularBeta, singularDepth, cutNode, thread, ss);
            ss->excludedMove  = NO_MOVE;

            if (singularScore < singularBeta)
            {
                extension  = 1;
                int margin = 300 * PVNode - 200 * !isTactical(ttMove);
                if ((singularScore + margin < singularBeta) && ss->doubleExtension <= 5)
                {
                    ss->doubleExtension = (ss - 1)->doubleExtension + 1;
                    extension++;
                }
            }
            else if (singularScore >= beta)
            {
                return singularScore;
            }

            //negative extensions
            else if (ttScore >= beta)
                extension = -2;

            else if (cutNode)
                extension = -1;

            //reAssign some stack values that might have been changed
            ss->played              = 1;
            ss->move                = move;
            ss->playedMoves[0]      = move;
            ss->continuationHistory = &thread.contHist[board->pieceBoard[moveFrom(move)]][moveTo(move)];
            ss->contCorrHist        = &thread.contCorrHist[board->pieceBoard[moveFrom(move)]][moveTo(move)];
        }
        int newDepth = depth - 1 + extension;
        int d        = newDepth - lmr;
        //make move
        board->makeMove(move);
        if (lmr >= 1)
        {
            score = -alphaBeta(-alpha - 1, -alpha, d, true, thread, ss + 1);
            if (score > alpha && d < newDepth)
            {

                const bool doDeeperSearch    = score > (bestScore + 39 + 3 * newDepth);
                const bool doShallowerSearch = score < bestScore + newDepth;

                newDepth += doDeeperSearch - doShallowerSearch;

                if (newDepth > d)
                    score = -alphaBeta(-alpha - 1, -alpha, newDepth, !cutNode, thread, ss + 1);
            }
        }
        else if (!PVNode || ss->played > 1)
        {
            score = -alphaBeta(-alpha - 1, -alpha, newDepth, !cutNode, thread, ss + 1);
        }

        if (PVNode && (ss->played == 1 || score > alpha))
        {
            score = -alphaBeta(-beta, -alpha, newDepth, false, thread, ss + 1);
        }
        board->unmakeMove(move);

        if (this->stopped)
            return 0;

        if (rootNode && thread.ThreadID == 0)
            moveNodes[move] += thread.nodes - beforeNodes;

        if (score > bestScore)
        {

            bestScore = score;

            if (bestScore > alpha)
            {
                if (PVNode)
                    updatePv(ss);

                bestMove = move;
                alpha    = bestScore;
            }

            if (bestScore >= beta)
            {
                updateHistories(thread, ss, depth, bestMove);
                break;
            }
        }
    }

    // checkmate or stalemate: with lazy generation we only know it once the
    // picker is exhausted. A singular search must return its untouched
    // bestScore instead, as it did when the excluded move was skipped here.
    if (moveCount == 0)
    {
        if (ss->excludedMove != NO_MOVE)
            return bestScore;
        return inCheck ? -(MAX_MATE_SCORE - ss->ply) : 0;
    }
    if (ss->excludedMove == NO_MOVE)
    {
        TT_BOUND bound = bestScore >= beta ? TT_LOWERBOUND : (alpha > oldAlpha ? TT_EXACT : TT_UPPERBOUND);

        // this node failed low, so the opponent's previous quiet move was good
        if (bound == TT_UPPERBOUND && !rootNode)
            updatePrevMoveFailLowBonus(thread, ss, depth);

        if (!inCheck && (!bestMove || !isTactical(bestMove)) && !(bound == TT_LOWERBOUND && bestScore <= ss->staticEval) && !(bound == TT_UPPERBOUND && bestScore >= ss->staticEval))
        {
            updateCorrHistScore(thread, ss, depth, bestScore - ss->staticEval);
        }

        TT::Instance()->ttSave(board->key, ss->ply, bestScore, rawEval, bound, depth, bestMove);
    }
    return bestScore;
}

SearchResult Search::start(Board* board, TimeManager* tm, int ThreadID) {
    SearchResult             res{};
    std::vector<std::thread> runningThreads;

    if (ThreadID == 0)
    {
        stopped  = false;
        seldepth = 0;
        std::fill(moveNodes, moveNodes + (1 << 16), 0);

        // Have a legal move ready before searching anything. m_bestMove is only
        // written when an iteration completes, so a search that is stopped
        // before that would otherwise send the best move of the previous
        // search, which is illegal in this position and loses the game.
        MoveList rootMoves;
        legalmoves<ALL_MOVES>(*board, rootMoves);
        m_bestMove = (rootMoves.numMove > 0) ? rootMoves.moves[0] : NO_MOVE;

        for (int i = 0; i < numThread; i++)
        {
            threads.at(i)->nodes       = 0ull;
            threads.at(i)->tbHits      = 0ull;
            threads.at(i)->searchDepth = 0;
            threads.at(i)->board       = *board;
        }

        this->timeManager = tm;
        for (int i = 1; i < numThread; i++)
        {
            runningThreads.emplace_back(&Search::start, this, board, tm, i);
        }
    }

    auto* ss = new Stack[MAX_PLY + 10];
    for (int i = 0; i < MAX_PLY + 10; i++)
    {
        (ss + i)->ply                 = i - 6;
        (ss + i)->continuationHistory = &threads.at(ThreadID)->contHist[0][0];
        (ss + i)->contCorrHist        = &threads.at(ThreadID)->contCorrHist[0][0];
    }

    int      score            = 0;
    uint16_t previousBestMove = NO_MOVE;
    int      bmStability      = 0;
    for (int i = 1; i <= timeManager->depthLimit; i++)
    {
        threads.at(ThreadID)->searchDepth = i;
        // aspiration window search
        if (i > 4)
        {
            int windowSize  = 30;
            int alpha       = score - windowSize;
            int beta        = score + windowSize;
            int failHighCnt = 0;
            while (true)
            {
                const int adjustedDepth = std::max(1, i - failHighCnt);

                score = alphaBeta(alpha, beta, adjustedDepth, false, *threads.at(ThreadID), ss + 6);
                if (stopped || (score > alpha && score < beta))
                    break;
                if (score <= alpha)
                {
                    beta        = (alpha + beta) / 2;
                    alpha       = std::max(-VALUE_INFINITE, alpha - windowSize);
                    failHighCnt = 0;
                }
                else if (score >= beta)
                {
                    beta = std::min(+VALUE_INFINITE, beta + windowSize);
                    failHighCnt++;
                }

                windowSize += windowSize;
            }
        }
        else
        {
            score = alphaBeta(-VALUE_INFINITE, VALUE_INFINITE, i, false, *threads.at(ThreadID), ss + 6);
        }

        if (stopped)
            break;
        if (ThreadID == 0)
        {
            auto elapsed = 1 + currentTime() - this->timeManager->startTime;

            this->m_bestMove  = (ss + 6)->pv[0];
            auto bestMoveNode = moveNodes[m_bestMove];
            auto nodes        = this->totalNodes();
            auto nps          = (1000 * nodes) / elapsed;

            std::cout << " info depth " << i;
            std::cout << " seldepth " << seldepth;
            if (abs(score) < MIN_MATE_SCORE)
            {
                std::cout << " score cp " << 100 * score / NORMALIZE_TO_PAWN;
            }
            else
            {
                int mate = (MAX_MATE_SCORE - abs(score) + 1) * (2 * (score > 0) - 1) / 2;
                std::cout << " score mate " << mate;
            }
            std::cout << " nps " << nps << " nodes " << nodes << " time " << elapsed << " hashfull " << TT::Instance()->getHashfull() << " tbhits " << totalTbHits() << " pv "
                      << getPV(ss + 6, threads.at(ThreadID)->board) << std::endl;

            // Track how long the best root move has been stable across iterations.
            if (m_bestMove == previousBestMove)
                bmStability = std::min(bmStability + 1, 8);
            else
                bmStability = 0;
            previousBestMove = m_bestMove;

            float bestMoveFraction = static_cast<double>(bestMoveNode) / threads.at(0)->nodes;
            //extra protection
            bestMoveFraction = std::clamp(bestMoveFraction, 0.0f, 1.0f);
            float nodeTm     = (nodeTmBase + bestMoveFraction * nodeTmMultp) / 100.0f;

            int   stabPercent     = std::max<int>(bmStabMin, bmStabBase - bmStabScale * bmStability);
            float stabilityFactor = stabPercent / 100.0f;

            if (elapsed > timeManager->softTime * nodeTm * stabilityFactor)
                break;
        }
    }
    delete[] ss;
    if (ThreadID == 0)
    {
        //wait other threads
        this->stop();
        for (std::thread& th : runningThreads)
        {
            th.join();
        }
        std::cout << "bestmove " << moveToUci(this->m_bestMove, *board) << std::endl;
        runningThreads.clear();

        res.cp    = 100 * score / NORMALIZE_TO_PAWN;
        res.move  = this->m_bestMove;
        res.nodes = totalNodes();
        TT::Instance()->updateAge();
    }
    return res;
}

SearchResult Search::datagenSearch(Stack* ss, int64_t softNodes, int64_t hardNodes, uint16_t rootExclude) {
    ThreadData* td = threads.at(0);

    stopped         = false;
    seldepth        = 0;
    td->nodes       = 0ull;
    td->tbHits      = 0ull;
    td->searchDepth = 0;

    // Only node limits gate this search; time/movetime are disabled.
    TimeManager tm;
    tm.depthLimit     = MAX_PLY;
    tm.nodeLimit      = hardNodes;
    tm.fixedMoveTime  = -1;
    tm.startTime      = currentTime();
    tm.hardTime       = std::numeric_limits<int64_t>::max();
    tm.softTime       = std::numeric_limits<int64_t>::max();
    tm.period         = 1024;
    tm.calls          = tm.period;
    this->timeManager = &tm;

    // Full reset of the reused stack, matching a freshly constructed one.
    for (int i = 0; i < MAX_PLY + 10; i++)
    {
        ss[i]                     = Stack();
        ss[i].ply                 = i - 6;
        ss[i].continuationHistory = &td->contHist[0][0];
        ss[i].contCorrHist        = &td->contCorrHist[0][0];
    }

    // If the caller wants to exclude a particular root move (e.g. the best move
    // from a previous search, to discover the second-best), tell the root node
    // to skip it. alphaBeta checks ss->excludedMove in its move loop (line 447).
    if (rootExclude != NO_MOVE)
        ss[6].excludedMove = rootExclude;

    int      score = 0;
    uint16_t best  = NO_MOVE;
    for (int depth = 1; depth <= MAX_PLY; depth++)
    {
        td->searchDepth = depth;
        if (depth > 4)
        {
            int windowSize = 20;
            int alpha      = score - windowSize;
            int beta       = score + windowSize;
            while (true)
            {
                score = alphaBeta(alpha, beta, depth, false, *td, ss + 6);
                if (stopped || (score > alpha && score < beta))
                    break;
                if (score <= alpha)
                    alpha = std::max(-VALUE_INFINITE, alpha - windowSize);
                else if (score >= beta)
                    beta = std::min(+VALUE_INFINITE, beta + windowSize);
                windowSize += windowSize / 3;
            }
        }
        else
        {
            score = alphaBeta(-VALUE_INFINITE, VALUE_INFINITE, depth, false, *td, ss + 6);
        }

        if (stopped)
            break;

        best = (ss + 6)->pv[0];

        if (static_cast<int64_t>(td->nodes) >= softNodes)
            break;
    }

    SearchResult res{};
    res.cp    = score;
    res.move  = best;
    res.nodes = td->nodes;

    // `tm` is about to go out of scope; drop the member pointer so it can never
    // be dereferenced after this call.
    this->timeManager = nullptr;
    return res;
}

uint64_t Search::totalNodes() {
    uint64_t sum = 0ull;
    for (int i = 0; i < numThread; i++)
    {
        sum += threads[i]->nodes;
    }
    return sum;
}

uint64_t Search::totalTbHits() {
    uint64_t sum = 0ull;
    for (int i = 0; i < numThread; i++)
    {
        sum += threads[i]->tbHits;
    }
    return sum;
}
