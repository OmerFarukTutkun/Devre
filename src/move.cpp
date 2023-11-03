#include "move.h"
#include "attack.h"
#include "history.h"
#include "movegen.h"
#include "uciOptions.h"
#include <sstream>

constexpr int16_t SEE_VALUE[] = {100, 300, 300, 500, 1000, 150 ,0,0,100, 300, 300, 500, 1000, 150 , 0 , 0};
std::string moveToUci(uint16_t move, Board &board) {
    std::stringstream ss;
    if (move == NULL_MOVE) {
        ss << "0000";
        return ss.str();
    }
    int from = moveFrom(move);
    int to = moveTo(move);
    int type = moveType(move);
    auto option = Options.at("UCI_Chess960");
    bool frc = option.currentValue == "true";
    if (frc && (type == KING_CASTLE || type == QUEEN_CASTLE)) {
        int queenSide = type == QUEEN_CASTLE;
        to = board.castlingRooks[2 * board.sideToMove + queenSide];
    }

    ss << SQUARE_IDENTIFIER[from] << SQUARE_IDENTIFIER[to];
    if (isPromotion(type)) {
        auto x = "nbrq";
        int idx = type & 0x03;
        ss << x[idx];
    }
    return ss.str();
}

uint16_t moveFromUci(Board &board, std::string uciMove) {
    if (uciMove == "0000") {
        return NULL_MOVE;
    }
    auto moveList = MoveList();
    legalmoves<ALL_MOVES>(board, moveList);

    int from = (uciMove[0] - 'a') + 8 * (uciMove[1] - '1');
    int to = (uciMove[2] - 'a') + 8 * (uciMove[3] - '1');

    //FRC castling. king captures own rook
    if (board.pieceBoard[to] != EMPTY && pieceColor(board.pieceBoard[to]) == board.sideToMove) {
        if (to == board.castlingRooks[0])
            return createMove(from, G1, KING_CASTLE);

        else if (to == board.castlingRooks[1])
            return createMove(from, C1, QUEEN_CASTLE);

        else if (to == board.castlingRooks[2])
            return createMove(from, G8, KING_CASTLE);

        else if (to == board.castlingRooks[3])
            return createMove(from, C8, QUEEN_CASTLE);
    }
    for (int i = 0; i < moveList.numMove; i++) {
        auto move = moveList.moves[i];
        if (from == moveFrom(move) && moveTo(move) == to) {
            if (isPromotion(move))//promotion
            {
                switch (uciMove[4]) {
                    case 'n':
                    case 'N':
                        return moveList.moves[i];
                    case 'b':
                    case 'B':
                        return moveList.moves[i + 1];
                    case 'r':
                    case 'R':
                        return moveList.moves[i + 2];
                    case 'q':
                    case 'Q':
                        return moveList.moves[i + 3];
                    default:
                        break;
                }
            }
            return move;
        }
    }
    return NO_MOVE;
}
bool SEE(Board &board, uint16_t move, int threshold) {

    // Do not calculate SEE for castlings, ep, promotions
    if (moveType(move) >= KING_CASTLE && moveType(move) != CAPTURE) {
        return true;
    }

    int from = moveFrom(move);
    int to = moveTo(move);
    int side = board.sideToMove;
    int nextVictim = board.pieceBoard[from];
    int balance    = SEE_VALUE[board.pieceBoard[to]] - threshold;

    if(balance < 0)
        return false;
    balance -= SEE_VALUE[nextVictim];
    if(balance >= 0)
        return true;

    uint64_t occ = board.occupied[0] | board.occupied[1];
    clearBit(occ, from);
    clearBit(occ, to);

    uint64_t attackers = squareAttackedBy(board, to) & occ;
    uint64_t diagonalX = board.bitboards[WHITE_BISHOP] | board.bitboards[BLACK_BISHOP] | board.bitboards[WHITE_QUEEN] |
                         board.bitboards[BLACK_QUEEN];
    uint64_t horizontalX =
            board.bitboards[WHITE_ROOK] | board.bitboards[BLACK_ROOK] | board.bitboards[WHITE_QUEEN] |
            board.bitboards[BLACK_QUEEN];


    side = !side;

    while(true)
    {
        from = getLeastValuableAttacker(board, attackers, side);
        if(from == NO_SQ)
            break;
        //remove attacker from occupancy and attackers
        clearBit(occ, from);
        clearBit(attackers, from);

        auto piece = pieceType(board.pieceBoard[from]);
        if (piece == PAWN || piece == BISHOP || piece == QUEEN)
            attackers |= bishopAttacks(occ, to) & diagonalX & occ;
        if (piece == ROOK || piece == QUEEN)
            attackers |= rookAttacks(occ, to) & horizontalX & occ;

        side = !side;
        balance = - balance - 1 - SEE_VALUE[piece];
        if(balance >= 0)
        {
            if(piece == KING && (attackers & board.occupied[side]))
                side = !side;

            break;
        }
    }
    return side != board.sideToMove;
}

MoveList::MoveList(uint16_t ttMove) {
    this->ttMove = ttMove;
    numMove = 0;
    isSorted = false;
}

void MoveList::addMove(uint16_t move) {
    moves[numMove++] = move;
}

void MoveList::scoreMoves(ThreadData &thread, Stack *ss) {
    Board *board = &thread.board;
    auto counterMove = thread.counterMoves[board->sideToMove][moveFrom((ss - 1)->move)][moveTo((ss - 1)->move)];
    for (int i = 0; i < numMove; i++) {
        auto move = moves[i];
        if (move == ttMove) {
            scores[i] = 30 * MIL;
        } else {
            auto type = moveType(move);
            scores[i] = moveTypeScores[type];
            if (ss->killers[0] == move) {
                scores[i] = 9 * MIL;
            } else if (ss->killers[1] == move) {
                scores[i] = 8.5 * MIL;
            } else if (counterMove == move) {
                scores[i] = 8 * MIL;
            } else if (type < 2) //history heuristic
            {
                scores[i] += getQuietHistory(thread, ss, move);
            } else if (type == CAPTURE) {
                if (SEE(*board, move))
                    scores[i] += getCaptureHistory(thread, ss, move);
                else
                    scores[i] = getCaptureHistory(thread, ss, move);
            }
        }
    }
}

uint16_t MoveList::pickMove(ThreadData &thread, Stack *ss, int skipThreshold) {
    if (!isSorted) {
        scoreMoves(thread, ss);
        isSorted = true;
    }
    if (numMove <= 0) {
        return NO_MOVE;
    }
    int maxScoreIndex = 0;
    for (int i = 1; i < numMove; i++) {
        if (scores[i] > scores[maxScoreIndex]) {
            maxScoreIndex = i;
        }
    }

    //skip remaining moves
    if (scores[maxScoreIndex] < skipThreshold) {
        return NO_MOVE;
    }

    uint16_t move = moves[maxScoreIndex];
    scores[maxScoreIndex] = scores[numMove - 1];
    moves[maxScoreIndex] = moves[numMove  - 1];
    numMove--;

    return move;
}
