#include "ThreadData.h"

ThreadData::ThreadData(const std::string &fen, int ID) {
    this->board = Board(fen);
    ThreadID = ID;
}

ThreadData::ThreadData(const Board &b, int ID) {
    board = b;
    ThreadID = ID;
}
