#ifndef DEVRE_SEARCH_H
#define DEVRE_SEARCH_H

#include "types.h"
#include "threadData.h"
#include <thread>
#include "timeManager.h"

struct Stack {
    PieceTo* continuationHistory;
    PieceTo* contCorrHist;
    int      ply;
    uint16_t pv[MAX_PLY + 10];
    uint16_t playedMoves[256];
    uint16_t played;
    uint8_t  doubleExtension;
    uint16_t move;
    uint16_t killers[2];
    int      staticEval;
    uint64_t threat;
    uint16_t excludedMove;

    Stack();
};

struct SearchResult {
    int      cp;
    uint16_t move;
    uint64_t nodes;
};

class Search {
   private:
    bool         stopped;
    int          numThread;
    uint16_t     m_bestMove{};
    uint64_t*    moveNodes;
    int          seldepth{};
    TimeManager* timeManager{};

    uint64_t totalNodes();

    uint64_t totalTbHits();

    int qsearch(int alpha, int beta, ThreadData& thread, Stack* ss);

    int alphaBeta(int alpha, int beta, int depth, bool cutNode, ThreadData& thread, Stack* ss);

   public:
    static void initSearchParameters();

    std::vector<ThreadData*> threads;

    void stop();

    void setThread(int thread);

    SearchResult start(Board* board, TimeManager* timeManager, int ThreadID = 0);

    // Lean single-threaded search entry for self-play data generation: runs
    // iterative deepening on threads[0]->board with a soft node budget (checked
    // at each iteration boundary) and a hard node cap, printing nothing. The
    // caller owns and reuses `ss` across searches to avoid per-move allocation.
    SearchResult datagenSearch(Stack* ss, int64_t softNodes, int64_t hardNodes);

    Search();

    // Search owns raw thread/node buffers; copying would double-free them.
    Search(const Search&)            = delete;
    Search& operator=(const Search&) = delete;

    virtual ~Search();
};

#endif  //DEVRE_SEARCH_H
