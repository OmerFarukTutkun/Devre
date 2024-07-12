#include <sstream>
#include "board.h"
#include "util.h"
#include "attack.h"
#include "move.h"
#include "nnue.h"
#include "zobrist.h"
#include "tt.h"

Board::Board(const std::string &fen) {
    key = 0;
    enPassant = NO_SQ;
    halfMove = 0;
    fullMove = 1;
    sideToMove = Color::WHITE;
    boardHistory.reserve(1024);
    boardHistory.clear();
    castlings = 0;
    nnueData = NNUEData();

    std::fill(bitboards, bitboards + N_PIECES, 0ull);
    std::fill(occupied, occupied + N_COLORS, 0ull);
    std::fill(pieceBoard, pieceBoard + N_SQUARES, EMPTY);
    std::fill(castlingRooks, castlingRooks + 4, NO_SQ);

    auto split = splitString(fen);
    int k = 0;

    //1: pieces
    for (int i = 7; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
            char c = split[0][k];
            if (CHAR_TO_PIECE.find(c) != CHAR_TO_PIECE.end()) {
                int piece = CHAR_TO_PIECE[c];
                addPiece(piece, squareIndex(i, j));
            } else if (c == '/')
                j--;
            else if (isdigit(c)) {
                j += c - '1';
            } else {
                std::cout << fen << "\n" << "Error: Fen isn't proper!!! " << "\n";
            }
            k++;
        }
    }

    if (split[1][0] == 'b') {
        sideToMove = Color::BLACK;
    }

    if (split.size() >= 3) {
        int wking = bitScanForward(bitboards[WHITE_KING]);
        int bking = bitScanForward(bitboards[BLACK_KING]);
        for (char c: split[2]) {
            if (c == 'K') {
                castlings |= WHITE_SHORT_CASTLE;
                castlingRooks[0] = H1;
            } else if (c == 'Q') {
                castlings |= WHITE_LONG_CASTLE;
                castlingRooks[1] = A1;
            } else if (c == 'k') {
                castlings |= BLACK_SHORT_CASTLE;
                castlingRooks[2] = H8;
            } else if (c == 'q') {
                castlings |= BLACK_LONG_CASTLE;
                castlingRooks[3] = A8;
            }
            //FRC castling rights
            else if (c <= 'H' && c >= 'A') {
                int queenSide = (c - 'A') < fileIndex(wking);
                if (queenSide)
                    castlings |= WHITE_LONG_CASTLE;
                else
                    castlings |= WHITE_SHORT_CASTLE;
                castlingRooks[queenSide] = squareIndex(0, c - 'A');
            } else if (c <= 'h' && c >= 'a') {
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
    if (split.size() >= 4) {
        if (split[3][0] != '-')
            enPassant = squareIndex(split[3]);
    }
    if (split.size() >= 5) {
        halfMove = std::stoi(split[4]);
    }
    if (split.size() >= 6) {
        fullMove = std::stoi(split[5]);
    }

    NNUE::Instance()->calculateInputLayer(*this, true);

    key ^= Zobrist::Instance()->EnPassantKeys[enPassant];
    key ^= Zobrist::Instance()->CastlingKeys[castlings];
    if (sideToMove == BLACK)
        key ^= Zobrist::Instance()->SideToPlayKey;

}

void Board::addPiece(int piece, int sq) {
    pieceBoard[sq] = piece;
    setBit(bitboards[piece], sq);
    setBit(occupied[pieceColor(piece)], sq);
    nnueData.nnueChanges.emplace_back(piece, sq, 1);
    key ^= Zobrist::Instance()->PieceKeys[piece][sq];
}

void Board::removePiece(int piece, int sq) {
    pieceBoard[sq] = EMPTY;
    clearBit(bitboards[piece], sq);
    clearBit(occupied[pieceColor(piece)], sq);
    nnueData.nnueChanges.emplace_back(piece, sq, -1);
    key ^= Zobrist::Instance()->PieceKeys[piece][sq];
}

void Board::movePiece(int piece, int from, int to) {
    addPiece(piece, to);
    removePiece(piece, from);
}

void Board::print() {
    std::string fen = getFen();
    std::cout << "fen : " + fen << std::endl;
    std::cout << "eval: " << eval() << std::endl;
    std::cout << "key : " << key << std::endl;
    // 7409793769312533240
    for (int i = 7; i >= 0; i--) {
        printf("\n  |----|----|----|----|----|----|----|----|\n");
        for (int j = 0; j < 8; j++) {
            if (pieceBoard[squareIndex(i, j)] != EMPTY)
                printf("%5c", PIECE_TO_CHAR[pieceBoard[squareIndex(i, j)]]);
            else
                printf("%5c", ' ');
        }
    }
    printf("\n  |----|----|----|----|----|----|----|----|\n");
    printf("\n%5c%5c%5c%5c%5c%5c%5c%5c\n", 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h');
    std::cout << std::flush;
}

void Board::makeMove(uint16_t move, bool updateNNUE) {
    auto from = moveFrom(move);
    auto to = moveTo(move);
    auto movetype = moveType(move);
    auto piece = this->pieceBoard[from];
    auto capturedPiece = this->pieceBoard[to];
    if (movetype == EN_PASSANT) {
        capturedPiece = pieceIndex(~sideToMove, PAWN);
    }

    boardHistory.emplace_back(enPassant, castlings, capturedPiece, halfMove, key);
    nnueData.nnueChanges.clear();


    //remove enPassant and Castling keys
    key ^= Zobrist::Instance()->EnPassantKeys[enPassant];
    key ^= Zobrist::Instance()->CastlingKeys[castlings];

    enPassant = NO_SQ;
    halfMove++;
    fullMove += sideToMove;

    if (castlings) {
        if (piece == WHITE_KING || from == castlingRooks[0] || to == castlingRooks[0])
            castlings &= 0b1110;
        if (piece == WHITE_KING || from == castlingRooks[1] || to == castlingRooks[1])
            castlings &= 0b1101;
        if (piece == BLACK_KING || from == castlingRooks[2] || to == castlingRooks[2])
            castlings &= 0b1011;
        if (piece == BLACK_KING || from == castlingRooks[3] || to == castlingRooks[3])
            castlings &= 0b0111;
    }

    switch (static_cast<MoveTypes>(movetype)) {
        case QUIET:
            movePiece(piece, from, to);
            break;
        case CAPTURE:
            removePiece(capturedPiece, to);
            movePiece(piece, from, to);
            break;
        case DOUBLE_PAWN_PUSH:
            movePiece(piece, from, to);
            if (bitboards[pieceIndex(!sideToMove, PAWN)] & PawnAttacks[sideToMove][(from + to) / 2]) {
                enPassant = (from + to) / 2;
            }
            break;
        case KING_CASTLE:
            removePiece(piece, from);
            removePiece(pieceIndex(sideToMove, ROOK), castlingRooks[2 * sideToMove]);
            addPiece(piece, to);
            addPiece(pieceIndex(sideToMove, ROOK), to - 1);
            break;
        case QUEEN_CASTLE:
            removePiece(piece, from);
            removePiece(pieceIndex(sideToMove, ROOK), castlingRooks[2 * sideToMove + 1]);
            addPiece(piece, to);
            addPiece(pieceIndex(sideToMove, ROOK), to + 1);
            break;
        case EN_PASSANT:
            removePiece(capturedPiece, squareIndex(rankIndex(from), fileIndex(to)));
            movePiece(piece, from, to);
            break;
        case KNIGHT_PROMOTION_CAPTURE:
        case BISHOP_PROMOTION_CAPTURE:
        case ROOK_PROMOTION_CAPTURE:
        case QUEEN_PROMOTION_CAPTURE:
            removePiece(capturedPiece, to);
        case KNIGHT_PROMOTION:
        case BISHOP_PROMOTION:
        case ROOK_PROMOTION:
        case QUEEN_PROMOTION:
            removePiece(piece, from);
            addPiece(pieceIndex(sideToMove, KNIGHT + (movetype & 3)), to);
            break;
        default:
            break;
    }
    key ^= Zobrist::Instance()->EnPassantKeys[enPassant];
    key ^= Zobrist::Instance()->CastlingKeys[castlings];
    key ^= Zobrist::Instance()->SideToPlayKey;
    TT::Instance()->ttPrefetch(key);
    sideToMove = ~sideToMove;
    if (isCapture(move) || pieceType(piece) == PAWN)
        halfMove = 0;

    if (updateNNUE) {
        nnueData.move = move;
        NNUE::Instance()->calculateInputLayer(*this);
        nnueData.nnueChanges.clear();
    } else {
        nnueData.clear();
    }
}

void Board::unmakeMove(uint16_t move, bool updateNNUE) {
    int from = moveFrom(move);
    int to = moveTo(move);
    int movetype = moveType(move);

    BoardHistory info = boardHistory.back();
    boardHistory.pop_back();

    enPassant = info.enPassant;
    halfMove = info.halfMove;
    castlings = info.castlings;
    sideToMove = ~sideToMove;
    fullMove -= sideToMove;

    int capturedPiece = info.capturedPiece;
    int piece = this->pieceBoard[to];

    if (updateNNUE)
        nnueData.popAccumulator();

    switch (movetype) {
        case QUIET:
        case DOUBLE_PAWN_PUSH:
            movePiece(piece, to, from);
            break;
        case CAPTURE:
            movePiece(piece, to, from);
            addPiece(capturedPiece, to);
            break;
        case KING_CASTLE:
            removePiece(piece, to);
            removePiece(pieceIndex(sideToMove, ROOK), to - 1);
            addPiece(piece, from);
            addPiece(pieceIndex(sideToMove, ROOK), castlingRooks[2 * sideToMove]);

            break;
        case QUEEN_CASTLE:
            removePiece(piece, to);
            removePiece(pieceIndex(sideToMove, ROOK), to + 1);
            addPiece(piece, from);
            addPiece(pieceIndex(sideToMove, ROOK), castlingRooks[2 * sideToMove + 1]);

            break;
        case EN_PASSANT:
            movePiece(piece, to, from);
            addPiece(capturedPiece, squareIndex(rankIndex(from), fileIndex(to)));
            break;
        case KNIGHT_PROMOTION_CAPTURE:
        case BISHOP_PROMOTION_CAPTURE:
        case ROOK_PROMOTION_CAPTURE:
        case QUEEN_PROMOTION_CAPTURE:
            removePiece(piece, to);
            addPiece(pieceIndex(sideToMove, PAWN), from);
            addPiece(capturedPiece, to);
            break;
        case KNIGHT_PROMOTION:
        case BISHOP_PROMOTION:
        case ROOK_PROMOTION:
        case QUEEN_PROMOTION:
            removePiece(piece, to);
            addPiece(pieceIndex(sideToMove, PAWN), from);
            break;
        default:
            break;
    }
    key = info.key;
}

int Board::eval() {
    return NNUE::Instance()->evaluate(*this);
}

std::string Board::getFen() {
    std::stringstream ss;
    int empty = 0;

    //1: pieces
    for (int i = 7; i >= 0; i--) {
        for (int j = 0; j < 8; j++) {
            auto sq = squareIndex(i, j);
            if (pieceBoard[sq] != EMPTY) {
                if (empty)
                    ss << empty;
                ss << PIECE_TO_CHAR[pieceBoard[sq]];
                empty = 0;
            } else
                empty++;
        }
        if (empty) {
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
    for (int i = 0; i < 4; i++) {
        if (checkBit(castlings, i))
            ss << castlingCharacters[i];
    }
    if (castlings == 0)
        ss << "-";

    //4-5-6: en-passsant, halfmove , fullmove
    ss << " " << SQUARE_IDENTIFIER[enPassant];
    ss << " " << (int)halfMove << " " << fullMove;
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
    enPassant = info.enPassant;
    halfMove = info.halfMove;
    key = info.key;
    castlings = info.castlings;
    sideToMove = ~sideToMove;
}

bool Board::hasNonPawnPieces() {
    return (bitboards[WHITE_KNIGHT] || bitboards[WHITE_BISHOP] || bitboards[WHITE_ROOK] || bitboards[WHITE_QUEEN])
           && (bitboards[BLACK_KNIGHT] || bitboards[BLACK_BISHOP] || bitboards[BLACK_ROOK] || bitboards[BLACK_QUEEN]);
}


bool Board::isMaterialDraw()
{
    if (bitboards[WHITE_PAWN] || bitboards[BLACK_PAWN] || bitboards[WHITE_ROOK] ||
        bitboards[BLACK_ROOK] || bitboards[WHITE_QUEEN] || bitboards[BLACK_QUEEN])
        return false;
    if (popcount64(occupied[0] | occupied[1]) < 4) {
    // here only left: K v K, K+B v K, K+N v K.
        return true;
    }
    if (popcount64(bitboards[WHITE_KNIGHT] | bitboards[BLACK_KNIGHT]) != 0) {
        return false;
    }
    if (popcount64(occupied[0] | occupied[1]) == 4) {
        constexpr uint64_t kWhiteSquares(0x55AA55AA55AA55AAULL);
        constexpr uint64_t kBlackSquares(0xAA55AA55AA55AA55ULL);

        if (bitboards[WHITE_BISHOP] && bitboards[BLACK_BISHOP]) {
            return !(((bitboards[WHITE_BISHOP] | bitboards[BLACK_BISHOP]) & kWhiteSquares) &&
                     ((bitboards[WHITE_BISHOP] | bitboards[BLACK_BISHOP]) & kBlackSquares));
        }
    }
    return false;
}

bool Board::isRepetition() {

    for (int i = boardHistory.size() - 1; i >= 0; i--) {
        if (key == boardHistory.at(i).key)
            return true;
        if ((boardHistory.size() - i) > halfMove)
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
