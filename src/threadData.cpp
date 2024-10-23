#include "threadData.h"

ThreadData::ThreadData(const std::string& fen, int ID) {
    std::fill_n(&history[0][0][0][0][0], 2*2*2*64*64, -500);
    this->board = Board(fen);
    ThreadID    = ID;
}

ThreadData::ThreadData(Board& b, int ID) {
    std::fill_n(&history[0][0][0][0][0], 2*2*2*64*64, -500);
    board    = Board(b);
    ThreadID = ID;
}

ThreadData::~ThreadData() {}
