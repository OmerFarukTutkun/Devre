#include "perft.h"
#include "movegen.h"
#include "util.h"

uint64_t perft(Board& board, int depth, bool updateNNUE) {
    if (depth == 0)
        return 1;
    if (depth == 1)
    {
        MoveList movelist;
        legalmoves<ALL_MOVES>(board, movelist);
        return movelist.numMove;
    }

    MoveList movelist;
    legalmoves<ALL_MOVES>(board, movelist);
    uint64_t count = 0;
    for (int i = 0; i < movelist.numMove; i++)
    {
        board.makeMove(movelist.moves[i], updateNNUE);
        count += perft(board, depth - 1, updateNNUE);
        board.unmakeMove(movelist.moves[i], updateNNUE);
    }
    return count;
}

void perftTest(Board& board, int depth, bool updateNNUE) {

    auto     start = currentTime();
    uint64_t total = 0;
    uint64_t count = 0;

    MoveList movelist;
    legalmoves<ALL_MOVES>(board, movelist);
    for (int i = 0; i < movelist.numMove; i++)
    {
        auto move = movelist.moves[i];
        board.makeMove(move, updateNNUE);

        count = perft(board, depth - 1, updateNNUE);
        std::cout << moveToUci(move, board) + ": " << count << std::endl;
        total = total + count;

        board.unmakeMove(move, updateNNUE);
    }
    auto end     = currentTime();
    auto elapsed = (end - start) + 1;
    auto speed   = (total * 1000) / elapsed;
    std::cout << "Time: " << elapsed << " ms\t"
              << "Speed: " << speed << " nodes/sec\t"
              << "Total: " << total << std::endl;
}
