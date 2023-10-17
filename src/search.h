#ifndef DEVRE_SEARCH_H
#define DEVRE_SEARCH_H

#include "types.h"
#include "ThreadData.h"
#include <thread>
#include "TimeManager.h"

struct Stack {
    PieceTo *continuationHistory;
    int ply;
    uint16_t pv[MAX_PLY + 10];
    uint16_t playedMoves[256];
    uint16_t played;

    uint16_t move;
    uint16_t killers[2];
    int staticEval;
    uint64_t threat;

    Stack();
};

struct SearchResult{
    int cp;
    uint16_t move;
    uint64_t nodes;
};

class Search {
private:
    bool stopped;
    int numThread;
    uint16_t bestMove{};
    TimeManager *timeManager{};
    uint64_t totalNodes();
    static void initSearchParameters();
    int qsearch(int alpha, int beta, ThreadData &thread, Stack *ss);
    int alphaBeta(int alpha, int beta, int depth, ThreadData &thread, Stack *ss);

public:
    std::vector<ThreadData> threads;
    void stop();
    void setThread(int thread);
    SearchResult start(Board *board, TimeManager *timeManager, int ThreadID = 0);
    Search();
    virtual ~Search();
};

#endif //DEVRE_SEARCH_H
