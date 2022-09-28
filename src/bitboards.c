#include "bitboards.h"
#include "stdio.h"
const uint8_t PieceTypes[12] = {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};
const uint8_t PieceIndexs[2][6]= { {PAWN, KNIGHT,BISHOP,ROOK,QUEEN,KING}, {BLACK_PAWN, BLACK_KNIGHT,BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN, BLACK_KING}};

void set_bit(uint64_t* bb, uint8_t sq)
{
    *bb |= (ONE << sq);
}
void clear_bit(uint64_t* bb, uint8_t sq)
{
    *bb &= ~(ONE << sq);
}
int check_bit(uint64_t bb, uint8_t sq)
{
    return (bb >> sq) & ONE;
}   
int bitScanForward(uint64_t bb)
{
    return __builtin_ctzll(bb);
}
int poplsb(uint64_t *bb) {
    int lsb = bitScanForward(*bb);
    *bb &= *bb - 1;
    return lsb;
}
uint8_t file_index(uint8_t sq)
{
    return sq & 7;
}
uint8_t rank_index(uint8_t sq)
{
    return sq>>3;
}
uint8_t square_index(uint8_t rank, uint8_t file)
{
    return rank*8 + file;
}
uint8_t mirror_vertically(uint8_t sq)
{
    return sq ^ 56;
}
uint8_t mirror_horizontally(uint8_t sq)
{
    return sq ^ 7;
}
uint8_t piece_type(uint8_t piece)
{
    return PieceTypes[piece];
}
uint8_t piece_index(uint8_t color, uint8_t piece_type)
{
    return PieceIndexs[color][piece_type];
}
int popcount64(uint64_t x)
{
    int count;
    for (count=0; x; count++)
        x &= x - 1;
    return count;
}
void print_bitboard(uint64_t bb)
{
    for(int i=7 ; i>=0 ;i--)
    {
        for(int j = 0; j < 8 ;j++)
        {
            printf("%d ",check_bit(bb, square_index(i,j)));
        }
        printf("\n");
    }
}
int piece_color(int piece)
{
    return piece >= BLACK_PAWN;
}
uint64_t betweenMask(uint8_t from, uint8_t to)
{
    if(from > to)
        return ( (ONE << from) -1 ) ^ ((ONE << (to + 1)) -1); 
    else
        return ( (ONE << to) -1 ) ^ ((ONE << (from + 1)) -1); 
}
