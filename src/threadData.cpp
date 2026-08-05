#include "threadData.h"

ThreadData::ThreadData(const std::string& fen, int ID) :
    ThreadID(ID),
    board(fen) {}

ThreadData::ThreadData(Board& b, int ID) :
    ThreadID(ID),
    board(b) {}

ThreadData::~ThreadData() {}
