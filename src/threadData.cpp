#include "threadData.h"

ThreadData::ThreadData(const std::string &fen, int ID) {
    this->board = new Board(fen);
    ThreadID = ID;
}

ThreadData::ThreadData(const Board &b, int ID) {
    board = new Board(b);
    ThreadID = ID;
}
