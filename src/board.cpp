#include <sstream>
#include "board.h"
#include "util.h"
#include "attack.h"
#include "move.h"
#include "nnue.h"
#include "zobrist.h"
#include "tt.h"

Board::Board(const std::string& fen) {
    key           = 0;
    pawnKey       = 0;
    majorKey      = 0;
    nonPawnKey[0] = 0;
    nonPawnKey[1] = 0;
    enPassant     = NO_SQ;
    halfMove      = 0;
    fullMove      = 1;
    sideToMove    = Color::WHITE;
    boardHistory.reserve(1024);
    boardHistory.clear();
    castlings = 0;

    std::fill(bitboards, bitboards + N_PIECES, 0ull);
    std::fill(occupied, occupied + N_COLORS, 0ull);
    std::fill(pieceBoard, pieceBoard + N_SQUARES, EMPTY);
    std::fill(castlingRooks, castlingRooks + 4, NO_SQ);

    auto split = splitString(fen);
    int  k     = 0;

    //1: pieces
    for (int i = 7; i >= 0; i--)
    {
        for (int j = 0; j < 8; j++)
        {
            char c     = split[0][k];
            int  piece = charToPiece(c);
            if (piece != EMPTY)
            {
                addPiece(piece, squareIndex(i, j));
            }
            else if (c == '/')
                j--;
            else if (isdigit(c))
            {
                j += c - '1';
            }
            else
            {
                std::cout << fen << "\n"
                          << "Error: Fen isn't proper!!! "
                          << "\n";
            }
            k++;
        }
    }

    if (split[1][0] == 'b')
    {
        sideToMove = Color::BLACK;
    }

    if (split.size() >= 3)
    {
        int wking = bitScanForward(bitboards[WHITE_KING]);
        int bking = bitScanForward(bitboards[BLACK_KING]);
        for (char c : split[2])
        {
            if (c == 'K')
            {
                castlings |= WHITE_SHORT_CASTLE;
                castlingRooks[0] = H1;
            }
            else if (c == 'Q')
            {
                castlings |= WHITE_LONG_CASTLE;
                castlingRooks[1] = A1;
            }
            else if (c == 'k')
            {
                castlings |= BLACK_SHORT_CASTLE;
                castlingRooks[2] = H8;
            }
            else if (c == 'q')
            {
                castlings |= BLACK_LONG_CASTLE;
                castlingRooks[3] = A8;
            }
            //FRC castling rights
            else if (c <= 'H' && c >= 'A')
            {
                int queenSide = (c - 'A') < fileIndex(wking);
                if (queenSide)
                    castlings |= WHITE_LONG_CASTLE;
                else
                    castlings |= WHITE_SHORT_CASTLE;
                castlingRooks[queenSide] = squareIndex(0, c - 'A');
            }
            else if (c <= 'h' && c >= 'a')
            {
                int queenSide = (c - 'a') < fileIndex(bking);
                if (queenSide)
                    castlings |= BLACK_LONG_CASTLE;
                else
                    castlings |= BLACK_SHORT_CASTLE;
                castlingRooks[queenSide + 2] = squareIndex(7, c - 'a');
            }
        }
    }
    //En passant
    if (split.size() >= 4)
    {
        if (split[3][0] != '-')
            enPassant = squareIndex(split[3]);
    }
    if (split.size() >= 5)
    {
        halfMove = std::stoi(split[4]);
    }
    if (split.size() >= 6)
    {
        fullMove = std::stoi(split[5]);
    }

    key ^= Zobrist::Instance()->EnPassantKeys[enPassant];
    key ^= Zobrist::Instance()->CastlingKeys[castlings];
    if (sideToMove == BLACK)
        key ^= Zobrist::Instance()->SideToPlayKey;

    nnueData.size = 0;
    nnueData.accumulator[0].clear();
}

void Board::updateThreatsForPiece(int piece, int sq, int sign) {
    if (piece == EMPTY || pieceType(piece) == KING) return;
    auto& acc = nnueData.accumulator[nnueData.size];
    int pType = pieceType(piece);
    int pCol  = pieceColor(piece);
    uint64_t occ = occupied[WHITE] | occupied[BLACK];

    uint64_t bAtt = 0, rAtt = 0;
    bool     haveB = false, haveR = false;

    auto getBAtt = [&] { if (!haveB) { bAtt = bishopAttacks(occ, sq); haveB = true; } return bAtt; };
    auto getRAtt = [&] { if (!haveR) { rAtt = rookAttacks(occ, sq); haveR = true; } return rAtt; };

    // Outgoing threats; the masks are the complement form of THREAT_VICTIMS.
    const uint64_t kings = bitboards[WHITE_KING] | bitboards[BLACK_KING];
    uint64_t       attacked = 0;
    switch (pType)
    {
    case PAWN :
        attacked = PawnAttacks[pCol][sq] & (bitboards[WHITE_KNIGHT] | bitboards[BLACK_KNIGHT]
                                            | bitboards[WHITE_ROOK] | bitboards[BLACK_ROOK]);
        break;
    case KNIGHT :
        attacked = KnightAttacks[sq] & occ & ~kings;
        break;
    case BISHOP :
        attacked = getBAtt() & occ & ~(kings | bitboards[WHITE_QUEEN] | bitboards[BLACK_QUEEN]);
        break;
    case ROOK :
        attacked = getRAtt() & occ & ~(kings | bitboards[WHITE_QUEEN] | bitboards[BLACK_QUEEN]);
        break;
    case QUEEN :
        attacked = (getBAtt() | getRAtt()) & occ & ~kings;
        break;
    default :
        break;
    }

    while (attacked)
    {
        const int dest   = poplsb(attacked);
        const int victim = pieceBoard[dest];
        acc.addThreatChange(pType, pCol, pieceType(victim), pieceColor(victim), sq, dest, sign);
    }

    if (pType == KNIGHT || pType == ROOK) {
        uint64_t whitePawnAtt = PawnAttacks[BLACK][sq] & bitboards[WHITE_PAWN];
        while (whitePawnAtt) {
            int attSq = poplsb(whitePawnAtt);
            acc.addThreatChange(
                PAWN, WHITE, pType, pCol, attSq, sq, sign);
        }
        uint64_t blackPawnAtt = PawnAttacks[WHITE][sq] & bitboards[BLACK_PAWN];
        while (blackPawnAtt) {
            int attSq = poplsb(blackPawnAtt);
            acc.addThreatChange(
                PAWN, BLACK, pType, pCol, attSq, sq, sign);
        }
    }

    uint64_t knightAtt = KnightAttacks[sq] & (bitboards[WHITE_KNIGHT] | bitboards[BLACK_KNIGHT]);
    while (knightAtt) {
        int attSq = poplsb(knightAtt);
        int attCol = pieceColor(pieceBoard[attSq]);
        acc.addThreatChange(
            KNIGHT, attCol, pType, pCol, attSq, sq, sign);
    }

    // A queen victim narrows the attackers to queens; nothing else reaches one.
    {
        uint64_t sliders = (pType != QUEEN)
                             ? bitboards[WHITE_BISHOP] | bitboards[BLACK_BISHOP] | bitboards[WHITE_QUEEN] | bitboards[BLACK_QUEEN]
                             : bitboards[WHITE_QUEEN] | bitboards[BLACK_QUEEN];
        sliders &= DIR_RAYS.diag[sq];
        if (sliders) {
            uint64_t diagAtt = getBAtt() & sliders;
            while (diagAtt) {
                int attSq = poplsb(diagAtt);
                int attPiece = pieceBoard[attSq];
                acc.addThreatChange(
                    pieceType(attPiece), pieceColor(attPiece), pType, pCol, attSq, sq, sign);
            }
        }
    }

    {
        uint64_t sliders = (pType != QUEEN)
                             ? bitboards[WHITE_ROOK] | bitboards[BLACK_ROOK] | bitboards[WHITE_QUEEN] | bitboards[BLACK_QUEEN]
                             : bitboards[WHITE_QUEEN] | bitboards[BLACK_QUEEN];
        sliders &= DIR_RAYS.straight[sq];
        if (sliders) {
            uint64_t straightAtt = getRAtt() & sliders;
            while (straightAtt) {
                int attSq = poplsb(straightAtt);
                int attPiece = pieceBoard[attSq];
                acc.addThreatChange(
                    pieceType(attPiece), pieceColor(attPiece), pType, pCol, attSq, sq, sign);
            }
        }
    }
}

void Board::updateDiscoveredThreats(int sq, int sign) {
    auto&          acc = nnueData.accumulator[nnueData.size];
    const uint64_t occ = (occupied[WHITE] | occupied[BLACK]) & ~(1ULL << sq);

    // Occupying or vacating sq blocks or opens the line between the nearest
    // piece on each side of it. Only a slider of the matching type, or a queen,
    // can hold that line.
    auto line = [&](uint64_t attacks, const uint64_t* rayA, const uint64_t* rayB, int slider) {
        const uint64_t a = attacks & rayA[sq] & occ;
        const uint64_t b = attacks & rayB[sq] & occ;
        if (!a || !b)
            return;

        const int sqA = bitScanForward(a), sqB = bitScanForward(b);
        const int pA = pieceBoard[sqA], pB = pieceBoard[sqB];
        const int tA = pieceType(pA), cA = pieceColor(pA);
        const int tB = pieceType(pB), cB = pieceColor(pB);
        if (tA == slider || tA == QUEEN)
            acc.addThreatChange(tA, cA, tB, cB, sqA, sqB, sign);
        if (tB == slider || tB == QUEEN)
            acc.addThreatChange(tB, cB, tA, cA, sqB, sqA, sign);
    };

    const uint64_t diagSliders = (bitboards[WHITE_BISHOP] | bitboards[BLACK_BISHOP] | bitboards[WHITE_QUEEN] | bitboards[BLACK_QUEEN]) & DIR_RAYS.diag[sq];
    if (diagSliders)
    {
        const uint64_t diag = bishopAttacks(occ, sq);
        line(diag, DIR_RAYS.north_east, DIR_RAYS.south_west, BISHOP);
        line(diag, DIR_RAYS.north_west, DIR_RAYS.south_east, BISHOP);
    }

    const uint64_t straightSliders = (bitboards[WHITE_ROOK] | bitboards[BLACK_ROOK] | bitboards[WHITE_QUEEN] | bitboards[BLACK_QUEEN]) & DIR_RAYS.straight[sq];
    if (straightSliders)
    {
        const uint64_t straight = rookAttacks(occ, sq);
        line(straight, DIR_RAYS.north, DIR_RAYS.south, ROOK);
        line(straight, DIR_RAYS.east, DIR_RAYS.west, ROOK);
    }
}

void Board::addPiece(int piece, int sq, bool updateNNUE) {
    if (updateNNUE) {
        updateDiscoveredThreats(sq, -1);
    }

    pieceBoard[sq] = piece;
    setBit(bitboards[piece], sq);
    setBit(occupied[pieceColor(piece)], sq);

    if (updateNNUE) {
        updateThreatsForPiece(piece, sq, 1);
        nnueData.accumulator[nnueData.size].addChange(piece, sq, 1);
    }

    key ^= Zobrist::Instance()->PieceKeys[piece][sq];

    auto type = pieceType(piece);
    if (type == PAWN)
        pawnKey ^= Zobrist::Instance()->PieceKeys[piece][sq];
    else
    {
        nonPawnKey[pieceColor(piece)] ^= Zobrist::Instance()->PieceKeys[piece][sq];

        if (type == KING || type == ROOK || type == QUEEN)
            majorKey ^= Zobrist::Instance()->PieceKeys[piece][sq];
    }
}

void Board::removePiece(int piece, int sq, bool updateNNUE) {
    if (updateNNUE) {
        updateThreatsForPiece(piece, sq, -1);
    }

    pieceBoard[sq] = EMPTY;
    clearBit(bitboards[piece], sq);
    clearBit(occupied[pieceColor(piece)], sq);

    if (updateNNUE) {
        updateDiscoveredThreats(sq, 1);
        nnueData.accumulator[nnueData.size].addChange(piece, sq, -1);
    }

    key ^= Zobrist::Instance()->PieceKeys[piece][sq];

    auto type = pieceType(piece);
    if (type == PAWN)
        pawnKey ^= Zobrist::Instance()->PieceKeys[piece][sq];
    else
    {
        nonPawnKey[pieceColor(piece)] ^= Zobrist::Instance()->PieceKeys[piece][sq];

        if (type == KING || type == ROOK || type == QUEEN)
            majorKey ^= Zobrist::Instance()->PieceKeys[piece][sq];
    }
}

void Board::movePiece(int piece, int from, int to, bool updateNNUE) {
    removePiece(piece, from, updateNNUE);
    addPiece(piece, to, updateNNUE);
}

void Board::print() {
    std::string fen = getFen();
    std::cout << "fen : " + fen << std::endl;
    std::cout << "eval: " << eval() << std::endl;
    std::cout << "key : " << key << std::endl;
    for (int i = 7; i >= 0; i--)
    {
        printf("\n  |----|----|----|----|----|----|----|----|\n");
        for (int j = 0; j < 8; j++)
        {
            if (pieceBoard[squareIndex(i, j)] != EMPTY)
                printf("%5c", pieceToChar(pieceBoard[squareIndex(i, j)]));
            else
                printf("%5c", ' ');
        }
    }
    printf("\n  |----|----|----|----|----|----|----|----|\n");
    printf("\n%5c%5c%5c%5c%5c%5c%5c%5c\n", 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h');
    std::cout << std::flush;
}

void Board::makeMove(uint16_t move, bool updateNNUE) {
    auto from          = moveFrom(move);
    auto to            = moveTo(move);
    auto movetype      = moveType(move);
    auto piece         = this->pieceBoard[from];
    auto capturedPiece = this->pieceBoard[to];
    if (movetype == EN_PASSANT)
    {
        capturedPiece = pieceIndex(~sideToMove, PAWN);
    }

    //tt prefetch
    auto keyPrefetch = key;
    keyPrefetch ^= Zobrist::Instance()->SideToPlayKey;
    keyPrefetch ^= Zobrist::Instance()->PieceKeys[piece][from];
    keyPrefetch ^= Zobrist::Instance()->PieceKeys[piece][to];
    if (capturedPiece != EMPTY)
        keyPrefetch ^= Zobrist::Instance()->PieceKeys[capturedPiece][to];
    TT::Instance()->ttPrefetch(keyPrefetch);

    boardHistory.emplace_back(enPassant, castlings, capturedPiece, halfMove, key);

    if (updateNNUE)
    {
        nnueData.size++;
        nnueData.accumulator[nnueData.size].clear();
    }

    //remove enPassant and Castling keys
    key ^= Zobrist::Instance()->EnPassantKeys[enPassant];
    key ^= Zobrist::Instance()->CastlingKeys[castlings];

    enPassant = NO_SQ;
    halfMove++;
    fullMove += sideToMove;

    if (castlings)
    {
        if (piece == WHITE_KING || from == castlingRooks[0] || to == castlingRooks[0])
            castlings &= 0b1110;
        if (piece == WHITE_KING || from == castlingRooks[1] || to == castlingRooks[1])
            castlings &= 0b1101;
        if (piece == BLACK_KING || from == castlingRooks[2] || to == castlingRooks[2])
            castlings &= 0b1011;
        if (piece == BLACK_KING || from == castlingRooks[3] || to == castlingRooks[3])
            castlings &= 0b0111;
    }

    switch (static_cast<MoveTypes>(movetype))
    {
    case QUIET :
        movePiece(piece, from, to);
        break;
    case CAPTURE :
        removePiece(capturedPiece, to);
        movePiece(piece, from, to);
        break;
    case DOUBLE_PAWN_PUSH :
        movePiece(piece, from, to);
        if (bitboards[pieceIndex(!sideToMove, PAWN)] & PawnAttacks[sideToMove][(from + to) / 2])
        {
            enPassant = (from + to) / 2;
        }
        break;
    case KING_CASTLE :
        removePiece(piece, from);
        removePiece(pieceIndex(sideToMove, ROOK), castlingRooks[2 * sideToMove]);
        addPiece(piece, to);
        addPiece(pieceIndex(sideToMove, ROOK), to - 1);
        break;
    case QUEEN_CASTLE :
        removePiece(piece, from);
        removePiece(pieceIndex(sideToMove, ROOK), castlingRooks[2 * sideToMove + 1]);
        addPiece(piece, to);
        addPiece(pieceIndex(sideToMove, ROOK), to + 1);
        break;
    case EN_PASSANT :
        removePiece(capturedPiece, squareIndex(rankIndex(from), fileIndex(to)));
        movePiece(piece, from, to);
        break;
    case KNIGHT_PROMOTION_CAPTURE :
    case BISHOP_PROMOTION_CAPTURE :
    case ROOK_PROMOTION_CAPTURE :
    case QUEEN_PROMOTION_CAPTURE :
        removePiece(capturedPiece, to);
        removePiece(piece, from);
        addPiece(pieceIndex(sideToMove, KNIGHT + (movetype & 3)), to);
        break;
    case KNIGHT_PROMOTION :
    case BISHOP_PROMOTION :
    case ROOK_PROMOTION :
    case QUEEN_PROMOTION :
        removePiece(piece, from);
        addPiece(pieceIndex(sideToMove, KNIGHT + (movetype & 3)), to);
        break;
    default :
        break;
    }
    key ^= Zobrist::Instance()->EnPassantKeys[enPassant];
    key ^= Zobrist::Instance()->CastlingKeys[castlings];
    key ^= Zobrist::Instance()->SideToPlayKey;
    sideToMove = ~sideToMove;
    if (isCapture(move) || pieceType(piece) == PAWN)
        halfMove = 0;

    // Four stores that let the NNUE update this ply without ever looking at an
    // intermediate board: the pawn-pair delta needs the pawn sets on both sides
    // of the move, and the king squares decide whether a perspective can be
    // updated incrementally at all.
    if (updateNNUE)
    {
        auto&       acc  = nnueData.accumulator[nnueData.size];
        const auto& prev = nnueData.accumulator[nnueData.size - 1];

        acc.pawns[WHITE] = bitboards[WHITE_PAWN];
        acc.pawns[BLACK] = bitboards[BLACK_PAWN];
        acc.stateValid   = true;

        if (pieceType(piece) == KING || !prev.stateValid)
        {
            acc.kingSq[WHITE] = static_cast<uint8_t>(bitScanForward(bitboards[WHITE_KING]));
            acc.kingSq[BLACK] = static_cast<uint8_t>(bitScanForward(bitboards[BLACK_KING]));

            NNUE::Instance()->refreshOnBucketChange(*this, static_cast<Color>(pieceColor(piece)));
        }
        else
        {
            acc.kingSq[WHITE] = prev.kingSq[WHITE];
            acc.kingSq[BLACK] = prev.kingSq[BLACK];
        }
    }
}

void Board::unmakeMove(uint16_t move, bool updateNNUE) {
    int from     = moveFrom(move);
    int to       = moveTo(move);
    int movetype = moveType(move);

    BoardHistory info = boardHistory.back();
    boardHistory.pop_back();

    enPassant  = info.enPassant;
    halfMove   = info.halfMove;
    castlings  = info.castlings;
    sideToMove = ~sideToMove;
    fullMove -= sideToMove;

    int capturedPiece = info.capturedPiece;
    int piece         = this->pieceBoard[to];

    switch (movetype)
    {
    case QUIET :
    case DOUBLE_PAWN_PUSH :
        movePiece(piece, to, from, false);
        break;
    case CAPTURE :
        movePiece(piece, to, from, false);
        addPiece(capturedPiece, to, false);
        break;
    case KING_CASTLE :
        removePiece(piece, to, false);
        removePiece(pieceIndex(sideToMove, ROOK), to - 1, false);
        addPiece(piece, from, false);
        addPiece(pieceIndex(sideToMove, ROOK), castlingRooks[2 * sideToMove], false);
        break;
    case QUEEN_CASTLE :
        removePiece(piece, to, false);
        removePiece(pieceIndex(sideToMove, ROOK), to + 1, false);
        addPiece(piece, from, false);
        addPiece(pieceIndex(sideToMove, ROOK), castlingRooks[2 * sideToMove + 1], false);
        break;
    case EN_PASSANT :
        movePiece(piece, to, from, false);
        addPiece(capturedPiece, squareIndex(rankIndex(from), fileIndex(to)), false);
        break;
    case KNIGHT_PROMOTION_CAPTURE :
    case BISHOP_PROMOTION_CAPTURE :
    case ROOK_PROMOTION_CAPTURE :
    case QUEEN_PROMOTION_CAPTURE :
        removePiece(piece, to, false);
        addPiece(pieceIndex(sideToMove, PAWN), from, false);
        addPiece(capturedPiece, to, false);
        break;
    case KNIGHT_PROMOTION :
    case BISHOP_PROMOTION :
    case ROOK_PROMOTION :
    case QUEEN_PROMOTION :
        removePiece(piece, to, false);
        addPiece(pieceIndex(sideToMove, PAWN), from, false);
        break;
    default :
        break;
    }
    key = info.key;
    if (updateNNUE)
    {
        nnueData.accumulator[nnueData.size].clear();
        nnueData.size = std::max(0, nnueData.size - 1);
    }
}

int Board::eval() { return NNUE::Instance()->evaluate(*this); }

std::string Board::getFen() {
    std::stringstream ss;
    int               empty = 0;

    //1: pieces
    for (int i = 7; i >= 0; i--)
    {
        for (int j = 0; j < 8; j++)
        {
            auto sq = squareIndex(i, j);
            if (pieceBoard[sq] != EMPTY)
            {
                if (empty)
                    ss << empty;
                ss << pieceToChar(pieceBoard[sq]);
                empty = 0;
            }
            else
                empty++;
        }
        if (empty)
        {
            ss << empty;
            empty = 0;
        }
        if (i > 0)
            ss << '/';
    }
    ss << ' ';

    //2: side to move
    ss << ((sideToMove == BLACK) ? "b " : "w ");

    //3: castlings
    char castlingCharacters[] = "KQkq";
    for (int i = 0; i < 4; i++)
    {
        if (checkBit(castlings, i))
            ss << castlingCharacters[i];
    }
    if (castlings == 0)
        ss << "-";

    //4-5-6: en-passsant, halfmove , fullmove
    ss << " " << SQUARE_IDENTIFIER[enPassant];
    ss << " " << (int) halfMove << " " << fullMove;
    return ss.str();
}

void Board::makeNullMove() {
    boardHistory.emplace_back(enPassant, castlings, EMPTY, halfMove, key);
    sideToMove = ~sideToMove;
    key ^= Zobrist::Instance()->EnPassantKeys[enPassant];
    enPassant = NO_SQ;
    key ^= Zobrist::Instance()->EnPassantKeys[enPassant];
    key ^= Zobrist::Instance()->SideToPlayKey;
    TT::Instance()->ttPrefetch(key);
    halfMove = 0;
}

void Board::unmakeNullMove() {
    BoardHistory info = boardHistory.back();
    boardHistory.pop_back();
    enPassant  = info.enPassant;
    halfMove   = info.halfMove;
    key        = info.key;
    castlings  = info.castlings;
    sideToMove = ~sideToMove;
}

bool Board::hasNonPawnPieces() {
    return (bitboards[WHITE_KNIGHT] || bitboards[WHITE_BISHOP] || bitboards[WHITE_ROOK] || bitboards[WHITE_QUEEN])
        && (bitboards[BLACK_KNIGHT] || bitboards[BLACK_BISHOP] || bitboards[BLACK_ROOK] || bitboards[BLACK_QUEEN]);
}

bool Board::isMaterialDraw() {
    if (bitboards[WHITE_PAWN] || bitboards[BLACK_PAWN] || bitboards[WHITE_ROOK] || bitboards[BLACK_ROOK] || bitboards[WHITE_QUEEN] || bitboards[BLACK_QUEEN])
        return false;
    if (popcount64(occupied[0] | occupied[1]) < 4)
    {
        // here only left: K v K, K+B v K, K+N v K.
        return true;
    }
    if (popcount64(bitboards[WHITE_KNIGHT] | bitboards[BLACK_KNIGHT]) != 0)
    {
        return false;
    }
    if (popcount64(occupied[0] | occupied[1]) == 4)
    {
        constexpr uint64_t kWhiteSquares(0x55AA55AA55AA55AAULL);
        constexpr uint64_t kBlackSquares(0xAA55AA55AA55AA55ULL);

        if (bitboards[WHITE_BISHOP] && bitboards[BLACK_BISHOP])
        {
            return !(((bitboards[WHITE_BISHOP] | bitboards[BLACK_BISHOP]) & kWhiteSquares) && ((bitboards[WHITE_BISHOP] | bitboards[BLACK_BISHOP]) & kBlackSquares));
        }
    }
    return false;
}

bool Board::isRepetition() {

    const int historySize = static_cast<int>(boardHistory.size());
    for (int i = historySize - 1; i >= 0; i--)
    {
        if (key == boardHistory[i].key)
            return true;
        if ((historySize - i) > halfMove)
            return false;
    }
    return false;
}

bool Board::isDraw() {
    if (halfMove >= 100 || isRepetition() || isMaterialDraw())
        return true;
    return false;
}

bool Board::inCheck() {
    auto kingSq = bitScanForward(bitboards[pieceIndex(sideToMove, KING)]);
    return isSquareAttacked(*this, kingSq, ~sideToMove);
}

uint64_t Board::threat() {
    if (sideToMove == WHITE)
        return allAttackedSquares<BLACK>(*this, occupied[WHITE] | occupied[BLACK]);
    else
        return allAttackedSquares<WHITE>(*this, occupied[WHITE] | occupied[BLACK]);
}

bool Board::inCheck(uint64_t threat) {
    if (threat & bitboards[pieceIndex(sideToMove, KING)])
        return true;
    return false;
}

