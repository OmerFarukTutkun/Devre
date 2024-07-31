#include "threadData.h"

ThreadData::ThreadData(const std::string &fen, int ID) {
    this->board = Board(fen);
    ThreadID = ID;
}

ThreadData::ThreadData(Board &b, int ID) {
    board = Board(b);
    ThreadID = ID;
}

ThreadData::~ThreadData() {

}
