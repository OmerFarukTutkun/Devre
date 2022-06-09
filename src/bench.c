#include "bench.h"
void bench(int argc, char **argv) {

    static const char *Benchmarks[] = {
        #include "bench.csv"
        ""
    };

    init_attacks();
    set_weights();
    init_keys();
    Position pos;
    fen_to_board(&pos, STARTING_FEN); 
    initZobristKey(&pos);
    SearchInfo info;
    memset(&info, 0, sizeof(SearchInfo));

    uint64_t nodes[256];

    double time;
    uint64_t totalNodes = 0ull;

    int depth     = argc > 2 ? atoi(argv[2]) : 10;
    int megabytes = argc > 4 ? atoi(argv[4]) : 16;

    tt_init(megabytes);
    time = timeInMilliseconds();
    for (int i = 0; strcmp(Benchmarks[i], ""); i++) {
        fen_to_board(&pos, Benchmarks[i]);
        char line[256];
        sprintf(line , "go depth %d\n", depth);
        go(line, &info, &pos);
        nodes[i] = info.qnodes + info.nodes;
        tt_clear(); 
        memset(&info, 0, sizeof(SearchInfo));
        memset(&pos, 0,  sizeof(Position));
    }

    time = timeInMilliseconds() - time;
    for (int i = 0; strcmp(Benchmarks[i], ""); i++) totalNodes += nodes[i];
    printf("%12d nodes %12d nps\n", (int)totalNodes, (int)(1000.0f * totalNodes / (time + 1)));

}
