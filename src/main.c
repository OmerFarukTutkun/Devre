#include "uci.h"
#include "bench.h"
int main(int argc, char **argv)
{
    if (argc > 1 && string_compare(argv[1], "bench", 5)) {
        bench(argc, argv);
       return 0;
    }
    Uci_Loop();
    return 0;
}
