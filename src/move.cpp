#include "move.h"
#include "attack.h"
#include "history.h"
#include "movegen.h"
#include "uciOptions.h"
#include <climits>
#include <sstream>

constexpr int16_t SEE_VALUE[] = {100, 300, 300, 500, 1000, 150, 0, 0, 100, 300, 300, 500, 1000, 150, 0, 0};

// Sentinel for a not-yet-scored move slot.
constexpr int SCORE_UNKNOWN = INT_MIN;
// Captures are scored optimistically as good (10M + capthist); a losing-SEE capture
// is demoted by this amount to the bad-capture band (1M + capthist), interleaving it
// with the quiets exactly as the old eager scorer did.
constexpr int BAD_CAPTURE_PENALTY = 9 * MIL;
// A tactical move at or above this score is "good" (queen promotions 20M/25M, en
// passant 11M, winning captures 10M ± capthist). It sits below the good-capture band
// minimum (~9.98M) and above killer0 (9M), so the split is exact.
constexpr int GOOD_TACTICAL_THRESHOLD = 9 * MIL + MIL / 2;

std::string moveToUci(uint16_t move, Board& board) {
    std::stringstream ss;
    if (move == NULL_MOVE)
    {
        ss << "0000";
        return ss.str();
    }
    int  from   = moveFrom(move);
    int  to     = moveTo(move);
    int  type   = moveType(move);
    auto option = Options.at("UCI_Chess960");
    bool frc    = option.currentValue == "true";
    if (frc && (type == KING_CASTLE || type == QUEEN_CASTLE))
    {
        int   queenSide = type == QUEEN_CASTLE;
        Color c         = (rankIndex(from) == 0) ? WHITE : BLACK;
        to              = board.castlingRooks[2 * c + queenSide];
    }

    ss << SQUARE_IDENTIFIER[from] << SQUARE_IDENTIFIER[to];
    if (isPromotion(type))
    {
        auto x   = "nbrq";
        int  idx = type & 0x03;
        ss << x[idx];
    }
    return ss.str();
}

uint16_t moveFromUci(Board& board, std::string uciMove) {
    if (uciMove == "0000")
    {
        return NULL_MOVE;
    }
    auto moveList = MoveList();
    legalmoves<ALL_MOVES>(board, moveList);

    int from = (uciMove[0] - 'a') + 8 * (uciMove[1] - '1');
    int to   = (uciMove[2] - 'a') + 8 * (uciMove[3] - '1');

    //FRC castling. king captures own rook
    if (board.pieceBoard[to] != EMPTY && pieceColor(board.pieceBoard[to]) == board.sideToMove)
    {
        if (to == board.castlingRooks[0])
            return createMove(from, G1, KING_CASTLE);

        else if (to == board.castlingRooks[1])
            return createMove(from, C1, QUEEN_CASTLE);

        else if (to == board.castlingRooks[2])
            return createMove(from, G8, KING_CASTLE);

        else if (to == board.castlingRooks[3])
            return createMove(from, C8, QUEEN_CASTLE);
    }
    for (int i = 0; i < moveList.numMove; i++)
    {
        auto move = moveList.moves[i];
        if (from == moveFrom(move) && moveTo(move) == to)
        {
            if (isPromotion(move))  //promotion
            {
                switch (uciMove[4])
                {
                case 'n' :
                case 'N' :
                    return moveList.moves[i];
                case 'b' :
                case 'B' :
                    return moveList.moves[i + 1];
                case 'r' :
                case 'R' :
                    return moveList.moves[i + 2];
                case 'q' :
                case 'Q' :
                    return moveList.moves[i + 3];
                default :
                    break;
                }
            }
            return move;
        }
    }
    return NO_MOVE;
}

bool SEE(Board& board, uint16_t move, int threshold) {

    // Do not calculate SEE for castlings, ep, promotions
    if (moveType(move) >= KING_CASTLE && moveType(move) != CAPTURE)
    {
        return true;
    }

    int from       = moveFrom(move);
    int to         = moveTo(move);
    int side       = board.sideToMove;
    int nextVictim = board.pieceBoard[from];
    int balance    = SEE_VALUE[board.pieceBoard[to]] - threshold;

    if (balance < 0)
        return false;
    balance -= SEE_VALUE[nextVictim];
    if (balance >= 0)
        return true;

    uint64_t occ = board.occupied[0] | board.occupied[1];
    clearBit(occ, from);
    clearBit(occ, to);

    uint64_t attackers   = squareAttackedBy(board, to) & occ;
    uint64_t diagonalX   = board.bitboards[WHITE_BISHOP] | board.bitboards[BLACK_BISHOP] | board.bitboards[WHITE_QUEEN] | board.bitboards[BLACK_QUEEN];
    uint64_t horizontalX = board.bitboards[WHITE_ROOK] | board.bitboards[BLACK_ROOK] | board.bitboards[WHITE_QUEEN] | board.bitboards[BLACK_QUEEN];


    side = !side;

    while (true)
    {
        from = getLeastValuableAttacker(board, attackers, side);
        if (from == NO_SQ)
            break;
        //remove attacker from occupancy and attackers
        clearBit(occ, from);
        clearBit(attackers, from);

        auto piece = pieceType(board.pieceBoard[from]);
        if (piece == PAWN || piece == BISHOP || piece == QUEEN)
            attackers |= bishopAttacks(occ, to) & diagonalX & occ;
        if (piece == ROOK || piece == QUEEN)
            attackers |= rookAttacks(occ, to) & horizontalX & occ;

        side    = !side;
        balance = -balance - 1 - SEE_VALUE[piece];
        if (balance >= 0)
        {
            if (piece == KING && (attackers & board.occupied[side]))
                side = !side;

            break;
        }
    }
    return side != board.sideToMove;
}

MoveList::MoveList(uint16_t ttMove, bool qsearch) {
    this->qsearch   = qsearch;
    this->ttMove    = ttMove;
    counterMove     = NO_MOVE;
    killer0         = NO_MOVE;
    killer1         = NO_MOVE;
    numMove         = 0;
    stage           = STAGE_TT;
    initialized     = false;
    tacticalScored  = false;
    remainderScored = false;
}

void MoveList::addMove(uint16_t move) {
    scores[numMove] = SCORE_UNKNOWN;
    moves[numMove]  = move;
    numMove++;
}

// Linear identity search; scoring plays no part, so a stage that resolves by identity
// (ttMove, killers, countermove) touches no history tables.
int MoveList::findMove(uint16_t move) const {
    if (move == NO_MOVE)
        return -1;
    for (int i = 0; i < numMove; i++)
        if (moves[i] == move)
            return i;
    return -1;
}

// Remove by swapping in the last entry (order among the survivors is irrelevant since
// each pick rescans for the max).
uint16_t MoveList::takeMove(int index) {
    uint16_t move = moves[index];
    numMove--;
    moves[index]  = moves[numMove];
    scores[index] = scores[numMove];
    return move;
}

// Captures get 10M + capthist optimistically (SEE is deferred to pick time); en passant
// and promotions keep their flat type score. Matches the old eager scorer for tacticals.
void MoveList::scoreTacticals(ThreadData& thread, Stack* ss) {
    for (int i = 0; i < numMove; i++)
    {
        uint16_t move = moves[i];
        if (!isTactical(move))
            continue;
        int type  = moveType(move);
        int score = moveTypeScores[type];
        if (type == CAPTURE)
            score += getCaptureHistory(thread, ss, move);
        scores[i] = score;
    }
}

// Everything still unscored at the remainder stage: quiets/double-pushes take history,
// castles and under-promotions take their flat type score. Demoted bad captures are
// already scored and are left untouched.
void MoveList::scoreRemainder(ThreadData& thread, Stack* ss) {
    for (int i = 0; i < numMove; i++)
    {
        if (scores[i] != SCORE_UNKNOWN)
            continue;
        uint16_t move  = moves[i];
        int      type  = moveType(move);
        int      score = moveTypeScores[type];
        if (type < KING_CASTLE)  // QUIET or DOUBLE_PAWN_PUSH
            score += getQuietHistory(thread, ss, move);
        scores[i] = score;
    }
}

uint16_t MoveList::pickMove(ThreadData& thread, Stack* ss, int skipThreshold) {
    Board* board = &thread.board;
    if (!initialized)
    {
        initialized = true;
        killer0     = ss->killers[0];
        killer1     = ss->killers[1];
        counterMove = thread.counterMoves[board->sideToMove][moveFrom((ss - 1)->move)][moveTo((ss - 1)->move)];
    }

    while (true)
    {
        switch (stage)
        {
        case STAGE_TT : {
            stage     = STAGE_GOOD_TACTICAL;
            int index = findMove(ttMove);
            if (index != -1)
                return takeMove(index);
            break;
        }
        case STAGE_GOOD_TACTICAL : {
            if (!tacticalScored)
            {
                scoreTacticals(thread, ss);
                tacticalScored = true;
            }
            // Pick the best tactical still above the good threshold. A capture whose SEE
            // loses is demoted in place and re-picked, so good captures still come out in
            // descending capthist order (exactly the old eager order).
            while (true)
            {
                int best = -1;
                for (int i = 0; i < numMove; i++)
                {
                    if (!isTactical(moves[i]))
                        continue;
                    if (best == -1 || scores[i] > scores[best])
                        best = i;
                }
                if (best == -1 || scores[best] < GOOD_TACTICAL_THRESHOLD)
                {
                    stage = STAGE_KILLER_1;
                    break;
                }
                uint16_t move = moves[best];
                if (!qsearch && moveType(move) == CAPTURE && !SEE(*board, move))
                {
                    scores[best] -= BAD_CAPTURE_PENALTY;
                    continue;
                }
                return takeMove(best);
            }
            break;
        }
        case STAGE_KILLER_1 : {
            stage     = STAGE_KILLER_2;
            int index = findMove(killer0);
            if (index != -1 && isQuiet(moves[index]))
                return takeMove(index);
            break;
        }
        case STAGE_KILLER_2 : {
            stage     = STAGE_COUNTER;
            int index = findMove(killer1);
            if (index != -1 && isQuiet(moves[index]))
                return takeMove(index);
            break;
        }
        case STAGE_COUNTER : {
            stage     = STAGE_REMAINDER;
            int index = findMove(counterMove);
            if (index != -1 && isQuiet(moves[index]))
                return takeMove(index);
            break;
        }
        case STAGE_REMAINDER : {
            if (!remainderScored)
            {
                scoreRemainder(thread, ss);
                remainderScored = true;
            }
            if (numMove == 0)
                return NO_MOVE;
            int best = 0;
            for (int i = 1; i < numMove; i++)
                if (scores[i] > scores[best])
                    best = i;
            if (scores[best] < skipThreshold)
                return NO_MOVE;
            return takeMove(best);
        }
        default :
            return NO_MOVE;
        }
    }
}
