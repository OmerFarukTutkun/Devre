#ifndef DEVRE_SEARCH_H
#define DEVRE_SEARCH_H
#include "types.h"
#include "Thread.h"
#include "TimeManager.h"
struct Stack {
    PieceTo* continuationHistory;
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

class Search{
    private:
        bool stopped;
        int numThread;
        TimeManager* timeManager{};
    public:
        Thread* threads;
        void stop();
        void setThread(int thread);
        int qsearch(int alpha, int beta, Thread &thread, Stack *ss);
        int alphaBeta(int alpha, int beta, int depth, Thread &thread, Stack *ss);
        void start(Board board, TimeManager timeManager);
        Search();
        virtual ~Search();

};
#endif //DEVRE_SEARCH_H
