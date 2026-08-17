#include "movepick.h"
#include "attack.h"
#include "history.h"

namespace {

// Is `sq` attacked by `by`, using a caller supplied occupancy? `byOcc` lets the
// caller pretend a piece was just captured without touching the board.
bool attackedWith(const Board& board, int sq, Color by, uint64_t occ, uint64_t byOcc)
{
    const uint64_t pawns   = board.bitboards[pieceIndex(by, PAWN)] & byOcc;
    const uint64_t knights = board.bitboards[pieceIndex(by, KNIGHT)] & byOcc;
    const uint64_t kings   = board.bitboards[pieceIndex(by, KING)] & byOcc;
    const uint64_t bishops = (board.bitboards[pieceIndex(by, BISHOP)] | board.bitboards[pieceIndex(by, QUEEN)]) & byOcc;
    const uint64_t rooks   = (board.bitboards[pieceIndex(by, ROOK)] | board.bitboards[pieceIndex(by, QUEEN)]) & byOcc;

    if (PawnAttacks[~by][sq] & pawns)
        return true;
    if (KnightAttacks[sq] & knights)
        return true;
    if (KingAttacks[sq] & kings)
        return true;
    if (bishopAttacks(occ, sq) & bishops)
        return true;
    if (rookAttacks(occ, sq) & rooks)
        return true;
    return false;
}

// Validated by running the real generator and checking membership: the only way
// to stay bit exact with the Chess960 rules, and castling candidates are rare.
bool isLegalCastle(const Board& board, uint16_t move)
{
    if (board.castlings == 0)
        return false;

    const Color    c      = board.sideToMove;
    const int      kingSq = bitScanForward(board.bitboards[pieceIndex(c, KING)]);
    const uint64_t occAll = board.occupied[WHITE] | board.occupied[BLACK];

    const uint64_t seen = (c == WHITE) ? allAttackedSquares<BLACK>(board, occAll & ~(ONE << kingSq))
                                       : allAttackedSquares<WHITE>(board, occAll & ~(ONE << kingSq));

    //legalmoves() only generates castling when numCheck == 0
    if (seen & (ONE << kingSq))
        return false;

    const uint64_t pinHv = (c == WHITE) ? pinMaskRooks<WHITE>(board, kingSq, board.occupied[WHITE], board.occupied[BLACK])
                                        : pinMaskRooks<BLACK>(board, kingSq, board.occupied[BLACK], board.occupied[WHITE]);

    MoveList list;
    if (c == WHITE)
        generateCastlingMoves<WHITE>(board, kingSq, occAll, seen, pinHv, list);
    else
        generateCastlingMoves<BLACK>(board, kingSq, occAll, seen, pinHv, list);

    for (int i = 0; i < list.numMove; i++)
    {
        if (list.moves[i] == move)
            return true;
    }
    return false;
}

// qsearch generates TACTICAL_MOVES only, so a tt move outside this set is not
// playable there.
bool isQsearchTactical(uint16_t move)
{
    const int type = moveType(move);
    return type == CAPTURE || type == EN_PASSANT || type == QUEEN_PROMOTION || type == QUEEN_PROMOTION_CAPTURE;
}

}  // namespace

bool isPseudoLegal(const Board& board, uint16_t move)
{
    if (move == NO_MOVE)
        return false;

    const int   type = moveType(move);
    const Color us   = board.sideToMove;
    const Color them = ~us;

    // Castling first: in Chess960 the king can already stand on its target
    // square, so from == to is a valid castling encoding.
    if (type == KING_CASTLE || type == QUEEN_CASTLE)
        return isLegalCastle(board, move);

    if (type == 6 || type == NULL_MOVE)
        return false;

    const int from = moveFrom(move);
    const int to   = moveTo(move);
    if (from == to)
        return false;

    const int piece = board.pieceBoard[from];
    if (piece == EMPTY || pieceColor(piece) != us)
        return false;

    const int      captured = board.pieceBoard[to];
    const uint64_t occAll   = board.occupied[WHITE] | board.occupied[BLACK];

    if (captured != EMPTY && pieceType(captured) == KING)
        return false;

    if (type == EN_PASSANT)
    {
        if (pieceType(piece) != PAWN)
            return false;
        if (board.enPassant == NO_SQ || to != board.enPassant)
            return false;
        if (captured != EMPTY)
            return false;
        if (!((PawnAttacks[us][from] >> to) & 1))
            return false;
        const int capSq = squareIndex(rankIndex(from), fileIndex(to));
        return board.pieceBoard[capSq] == pieceIndex(them, PAWN);
    }

    //the capture bit of the move type must match the board
    const bool wantsCapture = (type & 4) != 0;
    if (wantsCapture)
    {
        if (captured == EMPTY || pieceColor(captured) != them)
            return false;
    }
    else if (captured != EMPTY)
    {
        return false;
    }

    if (pieceType(piece) == PAWN)
    {
        const int      up        = (us == WHITE) ? 8 : -8;
        const uint64_t promoRank = (us == WHITE) ? RANK_8_BB : RANK_1_BB;
        const bool     isPromo   = (type & 8) != 0;

        //a pawn reaches the last rank if and only if it promotes
        if (isPromo != (((promoRank >> to) & 1) != 0))
            return false;

        if (wantsCapture)
            return ((PawnAttacks[us][from] >> to) & 1) != 0;

        if (type == DOUBLE_PAWN_PUSH)
        {
            const uint64_t startRank = (us == WHITE) ? RANK_2_BB : RANK_7_BB;
            if (!((startRank >> from) & 1))
                return false;
            if (to != from + 2 * up)
                return false;
            return board.pieceBoard[from + up] == EMPTY;
        }
        return to == from + up;
    }

    //only pawns promote or push twice
    if (type != QUIET && type != CAPTURE)
        return false;

    switch (pieceType(piece))
    {
    case KNIGHT :
        return ((KnightAttacks[from] >> to) & 1) != 0;
    case BISHOP :
        return ((bishopAttacks(occAll, from) >> to) & 1) != 0;
    case ROOK :
        return ((rookAttacks(occAll, from) >> to) & 1) != 0;
    case QUEEN :
        return (((bishopAttacks(occAll, from) | rookAttacks(occAll, from)) >> to) & 1) != 0;
    case KING :
        return ((KingAttacks[from] >> to) & 1) != 0;
    default :
        return false;
    }
}

// Play the move on a patched occupancy and ask whether our king is attacked.
// That covers pins, discovered checks and the en passant discovery at once.
bool isLegal(const Board& board, uint16_t move)
{
    const int type = moveType(move);

    //castling was already fully validated by isLegalCastle()
    if (type == KING_CASTLE || type == QUEEN_CASTLE)
        return true;

    const Color us   = board.sideToMove;
    const Color them = ~us;
    const int   from = moveFrom(move);
    const int   to   = moveTo(move);

    uint64_t occ    = board.occupied[WHITE] | board.occupied[BLACK];
    uint64_t themBB = board.occupied[them];

    clearBit(occ, from);
    setBit(occ, to);
    clearBit(themBB, to);  //no-op for quiet moves

    if (type == EN_PASSANT)
    {
        const int capSq = squareIndex(rankIndex(from), fileIndex(to));
        clearBit(occ, capSq);
        clearBit(themBB, capSq);
    }

    const int kingSq = (pieceType(board.pieceBoard[from]) == KING) ? to : bitScanForward(board.bitboards[pieceIndex(us, KING)]);

    return !attackedWith(board, kingSq, them, occ, themBB);
}

MovePicker::MovePicker(ThreadData& thread, Stack* ss, uint16_t ttMove, PickMode mode, int seeThreshold) :
    m_thread(thread),
    m_ss(ss),
    m_board(&thread.board),
    m_ttMove(ttMove),
    m_refutationIndex(0),
    m_seeThreshold(seeThreshold),
    m_skipQuiets(false),
    m_mode(mode)
{
    m_refutations[0] = NO_MOVE;
    m_refutations[1] = NO_MOVE;
    m_refutations[2] = NO_MOVE;

    //an in check qsearch node searches quiets too, so it wants them ordered
    if (mode != PICK_QSEARCH)
    {
        m_refutations[0] = ss->killers[0];
        m_refutations[1] = ss->killers[1];
        m_refutations[2] = thread.counterMoves[m_board->sideToMove][moveFrom((ss - 1)->move)][moveTo((ss - 1)->move)];
    }

    m_stage = STAGE_TT;
}

int MovePicker::bestIndex() const
{
    if (m_list.numMove <= 0)
        return -1;
    int best = 0;
    for (int i = 1; i < m_list.numMove; i++)
    {
        if (m_list.scores[i] > m_list.scores[best])
            best = i;
    }
    return best;
}

void MovePicker::removeAt(int index)
{
    m_list.moves[index]  = m_list.moves[m_list.numMove - 1];
    m_list.scores[index] = m_list.scores[m_list.numMove - 1];
    m_list.numMove--;
}

void MovePicker::skipQuiets()
{
    if (m_skipQuiets)
        return;
    m_skipQuiets = true;

    //quiets are only in the list once they have been generated
    if (m_stage != STAGE_REST)
        return;

    int write = 0;
    for (int i = 0; i < m_list.numMove; i++)
    {
        if (isQuiet(m_list.moves[i]))
            continue;
        m_list.moves[write]  = m_list.moves[i];
        m_list.scores[write] = m_list.scores[i];
        write++;
    }
    m_list.numMove = write;
}

bool MovePicker::refutationOk(uint16_t move, int index) const
{
    //killers and counter moves are quiet
    if (m_skipQuiets)
        return false;
    if (move == NO_MOVE || move == m_ttMove)
        return false;
    for (int i = 0; i < index; i++)
    {
        if (move == m_refutations[i])
            return false;
    }
    return isPseudoLegal(*m_board, move) && isLegal(*m_board, move);
}

void MovePicker::generateTacticals()
{
    if (m_board->sideToMove == WHITE)
    {
        prepareMoveGen<WHITE>(*m_board, m_info);
        generateMoves<WHITE, TACTICAL_MOVES>(*m_board, m_info, m_list);
    }
    else
    {
        prepareMoveGen<BLACK>(*m_board, m_info);
        generateMoves<BLACK, TACTICAL_MOVES>(*m_board, m_info, m_list);
    }

    // Score and drop the tt move, which was already played. Compacting instead
    // of swapping keeps the generator's order, which breaks equal scores.
    int write = 0;
    for (int read = 0; read < m_list.numMove; read++)
    {
        const uint16_t move = m_list.moves[read];
        if (move == m_ttMove)
            continue;

        const int type  = moveType(move);
        int       score = moveTypeScores[type];
        if (type == CAPTURE)
            score += getCaptureHistory(m_thread, m_ss, move);

        m_list.moves[write]  = move;
        m_list.scores[write] = score;
        write++;
    }
    m_list.numMove = write;
}

void MovePicker::generateQuiets()
{
    // m_info was filled by generateTacticals() and the board has not changed
    // since. Quiets are appended after the captures that are still in the list,
    // the SEE losing ones, so that both compete in the last stage.
    const int start = m_list.numMove;

    if (m_board->sideToMove == WHITE)
        generateMoves<WHITE, QUIET_MOVES>(*m_board, m_info, m_list);
    else
        generateMoves<BLACK, QUIET_MOVES>(*m_board, m_info, m_list);

    int write = start;
    for (int read = start; read < m_list.numMove; read++)
    {
        const uint16_t move = m_list.moves[read];
        if (move == m_ttMove || move == m_refutations[0] || move == m_refutations[1] || move == m_refutations[2])
            continue;

        const int type  = moveType(move);
        int       score = moveTypeScores[type];
        if (type < 2)  //castles and under promotions get no history
            score += getQuietHistory(m_thread, m_ss, move);

        m_list.moves[write]  = move;
        m_list.scores[write] = score;
        write++;
    }
    m_list.numMove = write;
}

uint16_t MovePicker::next()
{
    switch (m_stage)
    {
    case STAGE_TT :
        m_stage = STAGE_GEN_TACTICAL;
        if (m_ttMove != NO_MOVE && (m_mode != PICK_QSEARCH || isQsearchTactical(m_ttMove)) && (m_mode != PICK_PROBCUT || (isTactical(m_ttMove) && SEE(*m_board, m_ttMove, m_seeThreshold)))
            && isPseudoLegal(*m_board, m_ttMove) && isLegal(*m_board, m_ttMove))
            return m_ttMove;
        [[fallthrough]];

    case STAGE_GEN_TACTICAL :
        generateTacticals();
        m_stage = STAGE_GOOD_TACTICAL;
        [[fallthrough]];

    case STAGE_GOOD_TACTICAL :
        if (m_mode == PICK_PROBCUT)
        {
            while (m_list.numMove > 0)
            {
                const int       index = bestIndex();
                const uint16_t  move  = m_list.moves[index];
                removeAt(index);
                if (SEE(*m_board, move, m_seeThreshold))
                    return move;
            }
            m_stage = STAGE_DONE;
            return NO_MOVE;
        }
        while (true)
        {
            const int index = bestIndex();
            if (index < 0 || m_list.scores[index] < GOOD_TACTICAL_THRESHOLD)
                break;

            const uint16_t move = m_list.moves[index];

            //qsearch does not use SEE for ordering
            if (m_mode == PICK_MAIN && moveType(move) == CAPTURE && !SEE(*m_board, move))
            {
                m_list.scores[index] -= 9 * MIL;  // 10M + capthist -> 1M + capthist
                continue;                         //stays in the list for STAGE_REST
            }
            removeAt(index);
            return move;
        }
        //out of check qsearch searches tacticals only, in check it needs every evasion
        if (m_mode == PICK_QSEARCH)
        {
            m_stage = STAGE_DONE;
            return NO_MOVE;
        }
        m_stage = STAGE_REFUTATION;
        [[fallthrough]];

    case STAGE_REFUTATION :
        while (m_refutationIndex < 3)
        {
            const uint16_t move = m_refutations[m_refutationIndex++];
            if (refutationOk(move, m_refutationIndex - 1))
                return move;
        }
        m_stage = STAGE_GEN_QUIET;
        [[fallthrough]];

    case STAGE_GEN_QUIET :
        if (!m_skipQuiets)
            generateQuiets();
        m_stage = STAGE_REST;
        [[fallthrough]];

    case STAGE_REST : {
        const int index = bestIndex();
        if (index < 0)
        {
            m_stage = STAGE_DONE;
            return NO_MOVE;
        }
        const uint16_t move = m_list.moves[index];
        removeAt(index);
        return move;
    }

    case STAGE_DONE :
    default :
        return NO_MOVE;
    }
}
