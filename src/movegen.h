#ifndef DEVRE_MOVEGEN_H
#define DEVRE_MOVEGEN_H

#include "attack.h"
#include "bitboard.h"
#include "board.h"
#include "move.h"
#include "array"

//some things taken from SmallBrain
static uint64_t betweenSquares(int from, int to) {
    if (from == to)
        return 0ULL;
    auto occ = (ONE << from) | (ONE << to);
    if ((fileIndex(from) == fileIndex(to)) || (rankIndex(from) == rankIndex(to)))
        return (rookAttacks(occ, from) & rookAttacks(occ, to));

    int rankDiff = rankIndex(to) - rankIndex(from);
    int fileDiff = fileIndex(to) - fileIndex(from);
    if ((rankDiff == fileDiff) || (rankDiff == -fileDiff))
        return (bishopAttacks(occ, from) & bishopAttacks(occ, to));

    return 0ULL;
}

static auto genSquaresBetween() {
    std::array<std::array<uint64_t, 64>, 64> res{};
    for (int i = 0; i < 64; ++i)
    {
        for (int j = 0; j < 64; ++j)
        {
            res[i][j] = betweenSquares(i, j);
        }
    }
    return res;
}

static auto SQUARES_BETWEEN = genSquaresBetween();

inline void buildMoves(uint64_t moves, int from, int moveType, MoveList& movelist) {
    while (moves)
    {
        movelist.addMove(createMove(from, poplsb(moves), moveType));
    }
}

inline void buildPawnMoves(uint64_t attacks, int dir, int moveType, MoveList& movelist) {
    int to;
    while (attacks)
    {
        to = poplsb(attacks);
        movelist.addMove(createMove(to + dir, to, moveType));
    }
}

template<MoveGenerationTypes type>
inline void buildPromotionMoves(uint64_t attacks, int dir, int moveType, MoveList& movelist) {
    int to;
    while (attacks)
    {
        to = poplsb(attacks);
        if (type == ALL_MOVES || type == QUIET_MOVES)
        {
            movelist.addMove(createMove(to + dir, to, moveType));
            movelist.addMove(createMove(to + dir, to, moveType + 1));
            movelist.addMove(createMove(to + dir, to, moveType + 2));
        }
        if (type == ALL_MOVES || type == TACTICAL_MOVES)
        {
            movelist.addMove(createMove(to + dir, to, moveType + 3));
        }
    }
}

template<Color c>
uint64_t CheckMask(const Board& board, int sq, uint64_t occAll, int& numCheck) {
    uint64_t checks      = 0ULL;
    uint64_t pawn_mask   = board.bitboards[pieceIndex(~c, PAWN)] & PawnAttacks[c][sq];
    uint64_t knight_mask = board.bitboards[pieceIndex(~c, KNIGHT)] & KnightAttacks[sq];
    uint64_t bishop_mask =
      (board.bitboards[pieceIndex(~c, BISHOP)] | board.bitboards[pieceIndex(~c, QUEEN)])
      & bishopAttacks(occAll, sq);
    uint64_t rook_mask =
      (board.bitboards[pieceIndex(~c, ROOK)] | board.bitboards[pieceIndex(~c, QUEEN)])
      & rookAttacks(occAll, sq);


    if (pawn_mask)
    {
        checks |= pawn_mask;
        numCheck++;
    }
    if (knight_mask)
    {
        checks |= knight_mask;
        numCheck++;
    }
    if (bishop_mask)
    {
        auto index = bitScanForward(bishop_mask);
        checks |= SQUARES_BETWEEN[sq][index] | (1ULL << index);
        numCheck++;
    }
    if (rook_mask)
    {
        if (popcount64(rook_mask) > 1)
        {
            numCheck = 2;
            return checks;
        }

        auto index = bitScanForward(rook_mask);
        checks |= SQUARES_BETWEEN[sq][index] | (1ULL << index);
        numCheck++;
    }
    if (checks == 0ULL)
        return DEFAULT_CHECKMASK;
    return checks;
}

template<Color c>
uint64_t pinMaskRooks(const Board& board, int sq, uint64_t occ_us, uint64_t occ_enemy) {
    uint64_t rookMask =
      (board.bitboards[pieceIndex(~c, ROOK)] | board.bitboards[pieceIndex(~c, QUEEN)])
      & rookAttacks(occ_enemy, sq);
    uint64_t pin_hv = 0ULL;
    while (rookMask)
    {
        const int index = poplsb(rookMask);

        const uint64_t possible_pin = (SQUARES_BETWEEN[sq][index] | (ONE << index));
        if (popcount64(possible_pin & occ_us) == 1)
            pin_hv |= possible_pin;
    }
    return pin_hv;
}

template<Color c>
uint64_t pinMaskBishops(const Board& board, int sq, uint64_t occ_us, uint64_t occ_enemy) {
    uint64_t bishopMask =
      (board.bitboards[pieceIndex(~c, BISHOP)] | board.bitboards[pieceIndex(~c, QUEEN)])
      & bishopAttacks(occ_enemy, sq);

    uint64_t pin_d = 0ULL;
    while (bishopMask)
    {
        const int index = poplsb(bishopMask);

        const uint64_t possible_pin = (SQUARES_BETWEEN[sq][index] | (ONE << index));
        if (popcount64(possible_pin & occ_us) == 1)
            pin_d |= possible_pin;
    }
    return pin_d;
}

template<MoveGenerationTypes type>
void generateLegalKingMoves(
  int sq, uint64_t movable, uint64_t occEnemy, uint64_t seen, MoveList& movelist) {
    uint64_t temp = KingAttacks[sq] & ~seen;

    if (type == ALL_MOVES || type == TACTICAL_MOVES)
    {
        uint64_t captures = temp & occEnemy;
        buildMoves(captures, sq, CAPTURE, movelist);
    }
    if (type == ALL_MOVES || type == QUIET_MOVES)
    {
        uint64_t quiets = temp & movable & ~occEnemy;
        buildMoves(quiets, sq, QUIET, movelist);
    }
}

template<int pieceType, MoveGenerationTypes type>
void generatePieceMoves(uint64_t  pieces,
                        uint64_t  movable,
                        uint64_t  occAll,
                        uint64_t  occEnemy,
                        uint64_t  pinMask,
                        MoveList& movelist) {

    while (pieces)
    {
        int      sq      = poplsb(pieces);
        uint64_t attacks = getAttacks<pieceType>(occAll, sq) & movable;

        if (checkBit(pinMask, sq))
            attacks &= pinMask;
        if (type == ALL_MOVES || type == TACTICAL_MOVES)
        {
            uint64_t captures = attacks & occEnemy;
            buildMoves(captures, sq, CAPTURE, movelist);
        }
        if (type == ALL_MOVES || type == QUIET_MOVES)
        {
            uint64_t quiets = attacks & ~occEnemy;
            buildMoves(quiets, sq, QUIET, movelist);
        }
    }
}

template<Color c>
void generateCastlingMoves(
  const Board& board, int kingSq, uint64_t occ, uint64_t seen, MoveList& moveList) {
    int castlings = board.castlings;

    if (c == BLACK)
        castlings = castlings >> 2;

    for (int i = 0; i < 2; i++)
    {
        bool canCastle = castlings & 1;
        if (canCastle)
        {
            int rook   = board.castlingRooks[2 * c + i];
            int kingTo = castlingSquares[c][i][0];
            int rookTo = castlingSquares[c][i][1];

            //supports frc
            uint64_t path = SQUARES_BETWEEN[kingSq][kingTo] | SQUARES_BETWEEN[rook][rookTo]
                          | (ONE << kingTo) | (ONE << rookTo);
            uint64_t squaresKingPasses =
              SQUARES_BETWEEN[kingSq][kingTo] | (ONE << kingSq) | (ONE << kingTo);
            occ &= ~((ONE << kingSq) | (ONE << rook));

            if (!((occ & path) | (squaresKingPasses & seen)))
            {
                moveList.addMove(createMove(kingSq, kingTo, KING_CASTLE + i));
            }
        }
        castlings = castlings >> 1;
    }
}

template<int8_t direction>
constexpr uint64_t shift(const uint64_t b) {
    switch (direction)
    {
    case NORTH :
        return b << 8;
    case SOUTH :
        return b >> 8;
    case NORTH_WEST :
        return (b & ~FILE_A_BB) << 7;
    case WEST :
        return (b & ~FILE_A_BB) >> 1;
    case SOUTH_WEST :
        return (b & ~FILE_A_BB) >> 9;
    case NORTH_EAST :
        return (b & ~FILE_H_BB) << 9;
    case EAST :
        return (b & ~FILE_H_BB) << 1;
    case SOUTH_EAST :
        return (b & ~FILE_H_BB) >> 7;
    default :
        return b;
    }
}

template<Color c, MoveGenerationTypes type>
void generateLegalPawnMoves(const Board& board,
                            int          kingSq,
                            uint64_t     occAll,
                            uint64_t     occEnemy,
                            uint64_t     check_mask,
                            uint64_t     pin_hv,
                            uint64_t     pin_d,
                            MoveList&    movelist) {
    const uint64_t pawns_mask = board.bitboards[pieceIndex(c, PAWN)];

    constexpr Direction UP                = c == WHITE ? NORTH : SOUTH;
    constexpr Direction DOWN              = c == BLACK ? NORTH : SOUTH;
    constexpr Direction DOWN_LEFT         = c == BLACK ? NORTH_EAST : SOUTH_WEST;
    constexpr Direction DOWN_RIGHT        = c == BLACK ? NORTH_WEST : SOUTH_EAST;
    constexpr uint64_t  RANK_BEFORE_PROMO = c == WHITE ? RANK_7_BB : RANK_2_BB;
    constexpr uint64_t  RANK_PROMO        = c == WHITE ? RANK_8_BB : RANK_1_BB;
    constexpr uint64_t  DOUBLE_PUSH_RANK  = c == WHITE ? RANK_3_BB : RANK_6_BB;

    const uint64_t pawns_lr          = pawns_mask & ~pin_hv;
    const uint64_t unpinned_pawns_lr = pawns_lr & ~pin_d;
    const uint64_t pinned_pawns_lr   = pawns_lr & pin_d;


    uint64_t l_pawns =
      (pawnLeftAttacks<c>(unpinned_pawns_lr)) | (pawnLeftAttacks<c>(pinned_pawns_lr) & pin_d);
    uint64_t r_pawns =
      (pawnRightAttacks<c>(unpinned_pawns_lr)) | (pawnRightAttacks<c>(pinned_pawns_lr) & pin_d);

    l_pawns &= occEnemy & check_mask;
    r_pawns &= occEnemy & check_mask;

    // These pawns can walk Forward
    const uint64_t pawns_hv = pawns_mask & ~pin_d;

    const uint64_t pawns_pinned_hv   = pawns_hv & pin_hv;
    const uint64_t pawns_unpinned_hv = pawns_hv & ~pin_hv;

    // Prune moves that are blocked by a pieceIndex
    const uint64_t single_push_unpinned = shift<UP>(pawns_unpinned_hv) & ~occAll;
    const uint64_t single_push_pinned   = shift<UP>(pawns_pinned_hv) & pin_hv & ~occAll;
    // Prune moves that are not on the check_mask.
    uint64_t single_push = (single_push_unpinned | single_push_pinned) & check_mask;

    uint64_t double_push = ((shift<UP>(single_push_unpinned & DOUBLE_PUSH_RANK) & ~occAll)
                            | (shift<UP>(single_push_pinned & DOUBLE_PUSH_RANK) & ~occAll))
                         & check_mask;

    if (pawns_mask & RANK_BEFORE_PROMO)
    {
        uint64_t promote_left  = l_pawns & RANK_PROMO;
        uint64_t promote_right = r_pawns & RANK_PROMO;
        uint64_t promote_move  = single_push & RANK_PROMO;
        buildPromotionMoves<type>(promote_move, DOWN, KNIGHT_PROMOTION, movelist);
        buildPromotionMoves<type>(promote_left, DOWN_RIGHT, KNIGHT_PROMOTION_CAPTURE, movelist);
        buildPromotionMoves<type>(promote_right, DOWN_LEFT, KNIGHT_PROMOTION_CAPTURE, movelist);
    }
    single_push &= ~RANK_PROMO;
    l_pawns &= ~RANK_PROMO;
    r_pawns &= ~RANK_PROMO;
    if (type == ALL_MOVES || type == QUIET_MOVES)
    {
        buildPawnMoves(single_push, DOWN, QUIET, movelist);
        buildPawnMoves(double_push, DOWN * 2, DOUBLE_PAWN_PUSH, movelist);
    }
    if (type == ALL_MOVES || type == TACTICAL_MOVES)
    {
        buildPawnMoves(l_pawns, DOWN_RIGHT, CAPTURE, movelist);
        buildPawnMoves(r_pawns, DOWN_LEFT, CAPTURE, movelist);
    }
    if (type == ALL_MOVES || type == TACTICAL_MOVES)
    {
        if (board.enPassant != NO_SQ)
        {
            const int ep     = board.enPassant;
            const int epPawn = ep + DOWN;
            uint64_t  epBB   = PawnAttacks[~c][ep] & pawns_lr;
            uint64_t  enemy_queen_rook =
              (board.bitboards[pieceIndex(~c, ROOK)] | board.bitboards[pieceIndex(~c, QUEEN)]);
            uint64_t enemy_queen_bishop =
              (board.bitboards[pieceIndex(~c, BISHOP)] | board.bitboards[pieceIndex(~c, QUEEN)]);
            while (epBB)
            {
                uint64_t temp = occAll;
                int      from = poplsb(epBB);
                int      to   = ep;

                //make en passant move on occupancy
                setBit(temp, to);
                clearBit(temp, from);
                clearBit(temp, epPawn);

                //if we are in check continue
                if (getAttacks<ROOK>(temp, kingSq) & enemy_queen_rook)
                    continue;
                if (getAttacks<BISHOP>(temp, kingSq) & enemy_queen_bishop)
                    continue;

                movelist.addMove(createMove(from, to, EN_PASSANT));
            }
        }
    }
}

// all legal moves for a position
template<Color c, MoveGenerationTypes type>
void legalmoves(const Board& board, MoveList& movelist) {

    int      kingSq   = bitScanForward(board.bitboards[pieceIndex(c, KING)]);
    uint64_t occUs    = board.occupied[c];
    uint64_t occEnemy = board.occupied[~c];
    uint64_t occAll   = occUs | occEnemy;
    int      numCheck = 0;

    const uint64_t checkMask = CheckMask<c>(board, kingSq, occAll, numCheck);
    auto           seen      = allAttackedSquares<~c>(board, occAll & (~(ONE << kingSq)));
    auto           pin_hv    = pinMaskRooks<c>(board, kingSq, occUs, occEnemy);
    auto           pin_d     = pinMaskBishops<c>(board, kingSq, occUs, occEnemy);
    uint64_t       movable   = ~occUs;

    generateLegalKingMoves<type>(kingSq, movable, occEnemy, seen, movelist);

    if (numCheck >= 2)
        return;

    if (type == ALL_MOVES && board.castlings && numCheck == 0)
        generateCastlingMoves<c>(board, kingSq, occAll, seen, movelist);


    movable &= checkMask;

    uint64_t knights = board.bitboards[pieceIndex(c, KNIGHT)] & ~(pin_d | pin_hv);
    uint64_t rooks =
      (board.bitboards[pieceIndex(c, ROOK)] | board.bitboards[pieceIndex(c, QUEEN)]) & ~(pin_d);
    uint64_t bishops =
      (board.bitboards[pieceIndex(c, BISHOP)] | board.bitboards[pieceIndex(c, QUEEN)]) & ~(pin_hv);

    generateLegalPawnMoves<c, type>(board, kingSq, occAll, occEnemy, checkMask, pin_hv, pin_d,
                                    movelist);
    generatePieceMoves<KNIGHT, type>(knights, movable, occAll, occEnemy, 0ULL, movelist);
    generatePieceMoves<BISHOP, type>(bishops, movable, occAll, occEnemy, pin_d, movelist);
    generatePieceMoves<ROOK, type>(rooks, movable, occAll, occEnemy, pin_hv, movelist);
}

// all legal moves for a position
template<MoveGenerationTypes type>
void legalmoves(const Board& board, MoveList& movelist) {
    if (board.sideToMove == WHITE)
        legalmoves<WHITE, type>(board, movelist);
    else
        legalmoves<BLACK, type>(board, movelist);
}

#endif  //DEVRE_MOVEGEN_H
