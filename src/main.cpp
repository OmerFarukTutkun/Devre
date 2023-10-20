#include "uci.h"
#include "bench.h"

int main(int argc, char **argv)
{
    if(argc > 1)
        std::cout << std::string(argv[1]);
    if (argc > 1 && std::string(argv[1]) == "bench") {
        bench(argc, argv);
        return 0;
    }

    auto uci = Uci();
    uci.UciLoop();
    return 0;
}
