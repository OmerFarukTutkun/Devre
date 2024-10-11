#include "attack.h"

#ifdef USE_PEXT

#include <immintrin.h>

#endif

int slider_index(uint64_t occ, FancyMagic *table) {
#ifdef USE_PEXT
    return _pext_u64(occ, table->mask);
#else
    return (occ & table->mask) * table->magic >> table->shift;
#endif
}

template<typename T>
T pdep(T val, T mask) {
    T res = 0;
    for (T bb = 1; mask; bb += bb) {
        if (val & bb)
            res |= mask & -mask;
        mask &= mask - 1;
    }
    return res;
}

uint64_t sliderAttacks(int sq, uint64_t occ, int delta[4][2]) {
    int rank = rankIndex(sq);
    int file = fileIndex(sq);

    uint64_t attack = 0ull;
    for (int i = 0; i < 4; i++) {
        int k = 1;
        int *dir = delta[i];
        int new_rank = rank + k * dir[0];
        int new_file = file + k * dir[1];
        while (new_rank < 8 && new_file < 8 && new_rank >= 0 && new_file >= 0) {
            setBit(attack, squareIndex(new_rank, new_file));
            if (checkBit(occ, squareIndex(new_rank, new_file)))
                break;
            k++;
            new_rank = rank + k * dir[0];
            new_file = file + k * dir[1];
        }
    }
    return attack;
}

void init_slider_attacks(FancyMagic *table, uint64_t mask, uint64_t magic, int shift, int sq, int delta[4][2]) {
    uint64_t occ;

    table[sq].magic = magic;
    table[sq].mask = mask;
    table[sq].shift = shift;
    if (sq)
        table[sq].offset = table[sq - 1].offset + (1 << (64 - table[sq - 1].shift));
    for (uint64_t i = 0; i < (ONE << (64 - table[sq].shift)); i++) {
        occ = pdep<uint64_t>(i, mask);
        int index = slider_index(occ, &table[sq]);
        table[sq].offset[index] = sliderAttacks(sq, occ, delta);
    }
}

AttackTables::AttackTables() {
    int bishop_delta[4][2] = {{-1, 1},
                              {1,  1},
                              {-1, -1},
                              {1,  -1}};
    int rook_delta[4][2] = {{-1, 0},
                            {1,  0},
                            {0,  -1},
                            {0,  1}};

    RookTable[0].offset = RookAttacks;
    BishopTable[0].offset = BishopAttacks;

    for (int sq = 0; sq < 64; sq++) {
        init_slider_attacks(RookTable, rookMasks[sq], rookMagics[sq], rookShifts[sq], sq, rook_delta);
        init_slider_attacks(BishopTable, bishopMasks[sq], bishopMagics[sq], bishopShifts[sq], sq, bishop_delta);
    }
}

AttackTables *AttackTables::Instance() {
    static auto mInstance = AttackTables();
    return &mInstance;
}

AttackTables::~AttackTables() = default;

uint64_t rookAttacks(uint64_t occ, int sq) {
    auto table = &AttackTables::Instance()->RookTable[sq];
    return table->offset[slider_index(occ, table)];
}

uint64_t bishopAttacks(uint64_t occ, int sq) {
    auto table = &AttackTables::Instance()->BishopTable[sq];
    return table->offset[slider_index(occ, table)];
}

bool isSquareAttacked(Board &board, int sq, int side) {
    uint64_t occ = board.occupied[WHITE] | board.occupied[BLACK];
    if (side == BLACK) {
        return (PawnAttacks[WHITE][sq] & board.bitboards[BLACK_PAWN])
               || (KnightAttacks[sq] & board.bitboards[BLACK_KNIGHT])
               || (KingAttacks[sq] & board.bitboards[BLACK_KING])
               || (rookAttacks(occ, sq) & (board.bitboards[BLACK_ROOK] | board.bitboards[BLACK_QUEEN]))
               || (bishopAttacks(occ, sq) & (board.bitboards[BLACK_BISHOP] | board.bitboards[BLACK_QUEEN]));
    } else {
        return (PawnAttacks[BLACK][sq] & board.bitboards[WHITE_PAWN])
               || (KnightAttacks[sq] & board.bitboards[WHITE_KNIGHT])
               || (KingAttacks[sq] & board.bitboards[WHITE_KING])
               || (rookAttacks(occ, sq) & (board.bitboards[WHITE_ROOK] | board.bitboards[WHITE_QUEEN]))
               || (bishopAttacks(occ, sq) & (board.bitboards[WHITE_BISHOP] | board.bitboards[WHITE_QUEEN]));
    }
}

uint64_t squareAttackedBy(Board &board, int square) {
    uint64_t occupied = board.occupied[0] | board.occupied[1];
    uint64_t diagonal_attacks = getAttacks<BISHOP>(occupied, square);
    uint64_t horizontal_attacks = getAttacks<ROOK>(occupied, square);
    uint64_t knight_attacks = getAttacks<KNIGHT>(occupied, square);
    uint64_t king_attacks = getAttacks<KING>(occupied, square);

    uint64_t attackers = (knight_attacks & (board.bitboards[WHITE_KNIGHT] | board.bitboards[BLACK_KNIGHT]))
                         | (diagonal_attacks & (board.bitboards[WHITE_BISHOP] | board.bitboards[BLACK_BISHOP] |
                                                board.bitboards[WHITE_QUEEN] | board.bitboards[BLACK_QUEEN]))
                         | (horizontal_attacks & (board.bitboards[WHITE_ROOK] | board.bitboards[BLACK_ROOK] |
                                                  board.bitboards[WHITE_QUEEN] | board.bitboards[BLACK_QUEEN]))
                         | (king_attacks & (board.bitboards[WHITE_KING] | board.bitboards[BLACK_KING]))
                         | (PawnAttacks[BLACK][square] & board.bitboards[WHITE_PAWN])
                         | (PawnAttacks[WHITE][square] & board.bitboards[BLACK_PAWN]);
    return attackers;
}

int getLeastValuableAttacker(Board &board, uint64_t attackers, int side) {
    if ((attackers & board.occupied[side]) == 0ull)
        return NO_SQ;
    uint64_t temp;
    for (int i = pieceIndex(side, PAWN); i <= pieceIndex(side, KING); i++) {
        temp = attackers & board.bitboards[i];
        if (temp)
            return bitScanForward(temp);
    }
    return NO_SQ;
}


