#include "attack.h"

uint64_t RookAttacks[102400];
uint64_t BishopAttacks[5248];
FancyMagic RookTable[64];
FancyMagic BishopTable[64];
int slider_index(uint64_t occ, FancyMagic* table)
{
    return (occ & table->mask)* table->magic >> table->shift;
}
uint64_t rook_attacks(uint64_t occ, int sq)
{
    return RookTable[sq].offset[ slider_index(occ, &RookTable[sq] ) ];
}
uint64_t bishop_attacks(uint64_t occ, int sq)
{
    return BishopTable[sq].offset[ slider_index(occ, &BishopTable[sq] ) ];
}
uint64_t sliderAttacks(int sq,uint64_t occ,  int delta[4][2])
{
    uint8_t rank = rank_index(sq);
    uint8_t file = file_index(sq);

    uint64_t attack = 0ull;
    for(int i=0 ; i<4; i++)
    {
        int k=1;
        int* dir = delta[i];
        uint8_t new_rank = rank + k*dir[0];
        uint8_t new_file = file + k*dir[1];
        while( new_rank < 8 && new_file < 8  && new_rank >= 0 && new_file >= 0)
        {   
            set_bit(&attack,    square_index(new_rank, new_file) );
            if(check_bit(occ ,  square_index(new_rank, new_file) ))
                break;
            k++;
            new_rank = rank + k*dir[0];
            new_file = file + k*dir[1];
        }
    }
    return attack;
}
uint64_t pdep_uint64(uint64_t val, uint64_t mask) {
  uint64_t res = 0;
  for (uint64_t bb = 1; mask; bb += bb) {
    if (val & bb)
      res |= mask & -mask;
    mask &= mask - 1;
  }
  return res;
}
void init_slider_attacks(FancyMagic *table, uint64_t mask, uint64_t magic, int shift, int sq, int delta[4][2]) 
{
    uint64_t occ = 0ull;

    table[sq].magic = magic;
    table[sq].mask  = mask;
    table[sq].shift = shift;
    if(sq)
        table[sq].offset = table[sq -1 ].offset + (1 << (64 - table[sq-1].shift));
    for(uint64_t i=0; i < (ONE << (64 - table[sq].shift)) ; i++)
    { 
        occ = pdep_uint64(i, mask);
        int index = slider_index(occ, &table[sq]);
        table[sq].offset[index] = sliderAttacks(sq, occ, delta);
    }
}
void init_attacks()
{
    int bishop_delta[4][2] = {{ -1 ,1} , {1,1},{-1,-1}, { 1,-1}};
    int rook_delta[4][2] = {{ -1 ,0} , {1,0},{0,-1}, { 0,1}};

    RookTable[0].offset = RookAttacks;
    BishopTable[0].offset = BishopAttacks;

    for(int sq=0 ; sq<64; sq++)
    {
        init_slider_attacks(RookTable,   rookMasks[sq],   rookMagics[sq],   rookShifts[sq],   sq, rook_delta);
        init_slider_attacks(BishopTable, bishopMasks[sq], bishopMagics[sq], bishopShifts[sq], sq, bishop_delta);
    }
}
bool is_square_attacked(Position* pos, int sq, int side)
{
    uint64_t occ = pos->occupied[0] | pos->occupied[1];
    if(side)
    {
        return (PawnAttacks[WHITE][sq]  &  pos->bitboards[BLACK_PAWN])
            || (KnightAttacks[sq]       &  pos->bitboards[BLACK_KNIGHT])
            || (KingAttacks[sq]         &  pos->bitboards[BLACK_KING])
            || (rook_attacks(occ, sq)   & (pos->bitboards[BLACK_ROOK]   | pos->bitboards[BLACK_QUEEN] ) )
            || (bishop_attacks(occ, sq) & (pos->bitboards[BLACK_BISHOP] | pos->bitboards[BLACK_QUEEN] ) );
    }
    else
    {
        return (PawnAttacks[BLACK][sq]  &  pos->bitboards[PAWN])
            || (KnightAttacks[sq]       &  pos->bitboards[KNIGHT])
            || (KingAttacks[sq]         &  pos->bitboards[KING])
            || (rook_attacks(occ, sq)   & (pos->bitboards[ROOK] | pos->bitboards[QUEEN] ) )
            || (bishop_attacks(occ, sq) & (pos->bitboards[BISHOP] | pos->bitboards[QUEEN] ) );
    }
}
uint8_t getLeastValuablePiece(Position* pos, uint64_t attackers, int side)
{
    uint64_t temp = 0ull;
    for(int i= piece_index(side, PAWN); i <= piece_index(side, KING); i++)
    {
        temp = attackers & pos->bitboards[i];
        if(temp)
            return bitScanForward(temp);
    }
    return NO_SQ;
}
uint64_t square_attacked_by(Position *pos, uint8_t square)
{
    uint64_t occupied = pos->occupied[0] | pos->occupied[1];
    uint64_t diagonal_attacks   = get_attacks(occupied, square, BISHOP);
    uint64_t horizontal_attacks = get_attacks(occupied, square, ROOK);
    uint64_t knight_attacks     = get_attacks(occupied, square, KNIGHT);
    uint64_t king_attacks       = get_attacks(occupied, square, KING);

    uint64_t attackers = ( knight_attacks     & ( pos->bitboards[KNIGHT]  |  pos->bitboards[BLACK_KNIGHT] ))
                        |( diagonal_attacks   & ( pos->bitboards[BISHOP]  |  pos->bitboards[BLACK_BISHOP] |  pos->bitboards[QUEEN] | pos->bitboards[BLACK_QUEEN] ))
                        |( horizontal_attacks & ( pos->bitboards[ROOK]    |  pos->bitboards[BLACK_ROOK]   |  pos->bitboards[QUEEN] | pos->bitboards[BLACK_QUEEN] ))
                        |( king_attacks       & ( pos->bitboards[KING]    | pos->bitboards[BLACK_KING] ))
                        |( PawnAttacks[BLACK][square] & pos->bitboards[PAWN])
                        |( PawnAttacks[WHITE][square] & pos->bitboards[BLACK_PAWN]);
    return attackers;
}
bool is_legal(Position *pos)
{
    return !(is_square_attacked(pos , bitScanForward(pos->bitboards[piece_index(!pos->side, KING)]) , pos->side));
}
bool is_inCheck (Position *pos)
{
    return is_square_attacked(pos , bitScanForward(pos->bitboards[piece_index(pos->side, KING)]) , !pos->side);
}
uint64_t get_attacks(uint64_t occ, int sq, int piece_type)
{
    switch(piece_type)
    {
        case ROOK:   return rook_attacks(occ, sq);
        case BISHOP: return bishop_attacks(occ, sq);
        case KNIGHT: return KnightAttacks[sq];
        case KING:   return KingAttacks[sq];
        case QUEEN:  return rook_attacks(occ, sq) | bishop_attacks(occ, sq);
        default: return 0ull;
    }
}