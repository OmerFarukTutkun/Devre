#include "bench.h"
#include "tt.h"
#include "search.h"
#include "util.h"

void bench(int argc, char **argv) {

    //bench file is taken from Ethereal
    std::string Benchmarks[] = {
        #include "bench.csv"
    };

    uint64_t totalNodes = 0ull;

    int depth     = argc > 2 ? std::stoi( std::string(argv[2])) : 10;
    int megabytes = argc > 4 ? std::stoi( std::string(argv[4])) : 16;

    TT::Instance()->ttAllocate(megabytes);

    auto time = currentTime();
    auto search = Search();
    auto tm = TimeManager();
    tm.depthLimit = depth;
    std::cout << "Depth: " << depth;
    for (auto& fen : Benchmarks) {

        tm.start();
        std::cout << "fen :" << fen << std::endl;
        auto board = Board(fen);
        auto res = search.start(&board, &tm, 0);
        TT::Instance()->ttClear();
        totalNodes += res.nodes;

    }

    time = currentTime() - time;

    std::cout <<  totalNodes << " nodes " << 1000 * totalNodes / (time + 1) << " nps" << std::endl;

}
