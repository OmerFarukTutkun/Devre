#include "threadData.h"

ThreadData::ThreadData(const std::string& fen, int ID) {
    std::fill_n(&history[0][0][0][0][0], 2*2*2*64*64, -500);
    std::fill_n(&captureHist[0][0][0][0], 2*6*64*6, +1000);
    this->board = Board(fen);
    ThreadID    = ID;
}

ThreadData::ThreadData(Board& b, int ID) {
    std::fill_n(&history[0][0][0][0][0], 2*2*2*64*64, -500);
    std::fill_n(&captureHist[0][0][0][0], 2*6*64*6, +1000);
    board    = Board(b);
    ThreadID = ID;
}

ThreadData::~ThreadData() {}
