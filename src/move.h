#ifndef DEVRE_MOVE_H
#define DEVRE_MOVE_H

#include "types.h"
#include "board.h"
#include "search.h"
#include "threadData.h"

constexpr int MIL              = 1000000;
constexpr int moveTypeScores[] = {MIL, MIL, 3 * MIL, 2 * MIL, 10 * MIL, 11 * MIL, 0, 0, -10 * MIL, -9 * MIL, -8 * MIL, 20 * MIL, -7 * MIL, -6 * MIL, -5 * MIL, 25 * MIL};

enum MoveTypes : uint16_t {
    QUIET                    = 0,
    DOUBLE_PAWN_PUSH         = 1,
    KING_CASTLE              = 2,
    QUEEN_CASTLE             = 3,
    CAPTURE                  = 4,
    EN_PASSANT               = 5,
    NULL_MOVE                = 7,
    KNIGHT_PROMOTION         = 8,
    BISHOP_PROMOTION         = 9,
    ROOK_PROMOTION           = 10,
    QUEEN_PROMOTION          = 11,
    KNIGHT_PROMOTION_CAPTURE = 12,
    BISHOP_PROMOTION_CAPTURE = 13,
    ROOK_PROMOTION_CAPTURE   = 14,
    QUEEN_PROMOTION_CAPTURE  = 15,
};
constexpr uint16_t NO_MOVE = 0;

enum MoveGenerationTypes {
    TACTICAL_MOVES = 0,  // Captures + Queen promotion
    QUIET_MOVES    = 1,  //not exactly quiets move as it also contains under promotions
    ALL_MOVES      = 2,
};

// Lazy staged move picking: cheap stages (ttMove, killers, countermove) resolve
// by identity without scoring; scoring is deferred so a cutoff pays for nothing extra.
enum MovePickStage : uint8_t {
    STAGE_TT,
    STAGE_GOOD_TACTICAL,
    STAGE_KILLER_1,
    STAGE_KILLER_2,
    STAGE_COUNTER,
    STAGE_REMAINDER,
};
constexpr int castlingSquares[N_COLORS][2][2] = {{{G1, F1}, {C1, D1}}, {{G8, F8}, {C8, D8}}};

// 16-bit move
// 001100 011100  0000    -> e2e4 quiet move
//  from    to    type

inline int moveTo(uint16_t move) { return (move >> 4) & 63; }

inline int moveFrom(uint16_t move) { return (move >> 10) & 63; }

inline int moveType(uint16_t move) { return move & 15; }

inline uint16_t createMove(int from, int to, int type) { return (from) << 10 | (to) << 4 | (type); }

inline bool isPromotion(uint16_t move) { return (move >> 3) & 0x01; }

inline bool isCapture(uint16_t move) { return (move >> 2) & 0x01; }

inline bool isTactical(uint16_t move) { return (move >> 2) & 0x03; }

inline bool isQuiet(uint16_t move) { return !isTactical(move); }

class MoveList {
   public:
    uint16_t moves[256];
    int      scores[256];
    uint16_t ttMove;
    uint16_t counterMove;
    uint16_t killer0;
    uint16_t killer1;

    int  numMove;
    bool qsearch;

    uint8_t stage;
    bool    initialized;      // killers/countermove snapshotted on first pick
    bool    tacticalScored;   // tactical moves scored on entry to STAGE_GOOD_TACTICAL
    bool    remainderScored;  // quiets/castles/underpromos scored on entry to STAGE_REMAINDER

    MoveList(uint16_t ttMove = NO_MOVE, bool qsearch = false);

    void addMove(uint16_t move);

    uint16_t pickMove(ThreadData& thread, Stack* ss, int skipThreshold = -50 * MIL);

   private:
    void     scoreTacticals(ThreadData& thread, Stack* ss);
    void     scoreRemainder(ThreadData& thread, Stack* ss);
    int      findMove(uint16_t move) const;
    uint16_t takeMove(int index);
};

std::string moveToUci(uint16_t move, Board& board);

uint16_t moveFromUci(Board& board, std::string move);

bool SEE(Board& board, uint16_t move, int threshold = 0);

#endif  //DEVRE_MOVE_H
