#include "bitboard.h"
void printBitboard(const uint64_t bb)
{
    for(int i=7 ; i>=0 ;i--)
    {
        for(int j = 0; j < 8 ;j++)
        {
            std::cout << checkBit(bb, squareIndex(i, j)) << " ";
        }
       std::cout << "\n";
    }
}
