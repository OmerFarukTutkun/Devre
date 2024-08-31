#ifndef DEVRE_SEARCH_H
#define DEVRE_SEARCH_H

#include "types.h"
#include "threadData.h"
#include <thread>
#include "timeManager.h"

struct Stack {
    PieceTo *continuationHistory;
    int ply;
    uint16_t pv[MAX_PLY + 10];
    uint16_t playedMoves[256];
    uint16_t played;
    uint8_t doubleExtension;
    uint16_t move;
    uint16_t killers[2];
    int staticEval;
    uint64_t threat;
    uint16_t excludedMove;

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
    uint16_t m_bestMove{};
    TimeManager *timeManager{};
    uint64_t totalNodes();

    int qsearch(int alpha, int beta, ThreadData &thread, Stack *ss);
    int alphaBeta(int alpha, int beta, int depth, bool cutNode, ThreadData &thread, Stack *ss);

public:
    static void initSearchParameters();
    std::vector<ThreadData> threads;
    void stop();
    void setThread(int thread);
    SearchResult start(Board *board, TimeManager *timeManager, int ThreadID = 0);
    Search();
    virtual ~Search();
};

#endif //DEVRE_SEARCH_H
