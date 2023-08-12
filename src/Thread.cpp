#include "Thread.h"

Thread::Thread(const std::string &fen, int ID) {
    this->board = Board(fen);
    ThreadID = ID;
}

Thread::Thread(const Board &b, int ID) {
    board = b;
    ThreadID = ID;
}
