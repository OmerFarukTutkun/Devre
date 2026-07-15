#include "datagen.h"

#include "board.h"
#include "movegen.h"
#include "move.h"
#include "search.h"
#include "nnue.h"
#include "tt.h"
#include "util.h"
#include "types.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// On-disk format (little-endian, x86).
//
// A file is a concatenation of self-delimiting game records. Each record is:
//
//   [ 32-byte header ][ ScoredMove ]* [ ScoredMove{0,0} terminator ]
//
// Header (marlinformat-compatible packed board):
//   u64 occupancy        bitboard of occupied squares (square 0 = a1, LSB)
//   u8  pieces[16]        4 bits per occupied square, in ascending-square order
//                         (low nibble first). Nibble = color<<3 | pieceType,
//                         i.e. 0..5 white PNBRQK, 8..13 black pnbrqk. A rook
//                         that still holds a castling right is encoded 6 (white)
//                         / 14 (black) so castling & FRC are reconstructable.
//   u8  stmEpSquare       bit7 = side to move (0 white, 1 black);
//                         bits0..6 = en-passant square, or 64 if none.
//   u8  halfmoveClock     50-move counter at the first scored position.
//   u16 fullmoveNumber
//   i16 eval              white-relative raw eval of the first scored position.
//   u8  wdl               GAME RESULT, white POV: 0 = white loss (black win),
//                         1 = draw, 2 = white win.
//   u8  extra             reserved, 0.
//
// ScoredMove (one per scored position, in play order from the header position):
//   u16 move              the best move found at that position, in Devre's
//                         native encoding: from<<10 | to<<4 | type (see move.h).
//                         Castling is king-to-king-destination; promotions and
//                         en-passant are distinguished by `type`.
//   i16 eval              WHITE-RELATIVE search score in Devre's RAW internal
//                         units (= 2 x centipawns; UCI cp = eval / 2), negated
//                         when black moves.
//
// Perspective summary: every eval and the wdl live in white's frame, so the
// score->WDL sigmoid fit is P(white win) = sigmoid(eval / scale). A trainer that
// wants a side-to-move target negates eval when the side to move is black.
// ---------------------------------------------------------------------------

namespace {

// ---- result codes (white POV) ----
constexpr uint8_t WDL_BLACK_WIN = 0;
constexpr uint8_t WDL_DRAW      = 1;
constexpr uint8_t WDL_WHITE_WIN = 2;

#pragma pack(push, 1)
struct MarlinPackedBoard {
    uint64_t occupancy;
    uint8_t  pieces[16];
    uint8_t  stmEpSquare;
    uint8_t  halfmoveClock;
    uint16_t fullmoveNumber;
    int16_t  eval;
    uint8_t  wdl;
    uint8_t  extra;
};
struct ScoredMove {
    uint16_t move;
    int16_t  eval;
};
#pragma pack(pop)
static_assert(sizeof(MarlinPackedBoard) == 32, "packed board must be 32 bytes");
static_assert(sizeof(ScoredMove) == 4, "scored move must be 4 bytes");

uint16_t toViriMove(uint16_t devreMove) {
    int from = moveFrom(devreMove);
    int to = moveTo(devreMove);
    int type = moveType(devreMove);

    int viriTo = to;
    int viriType = 0;
    int viriPromo = 0;

    if (type == EN_PASSANT) {
        viriType = 1;
    } else if (type == KING_CASTLE) {
        viriType = 2;
        viriTo = from + 3;
    } else if (type == QUEEN_CASTLE) {
        viriType = 2;
        viriTo = from - 4;
    } else if (type >= KNIGHT_PROMOTION) {
        viriType = 3;
        viriPromo = type & 3; // Knight=0, Bishop=1, Rook=2, Queen=3
    } else {
        viriType = 0;
    }

    return from | (viriTo << 6) | (viriPromo << 12) | (viriType << 14);
}

// ---- generation parameters ----
// Score thresholds are in Devre's RAW internal eval units (= 2 x centipawns),
// matching what datagenSearch returns; the UCI-cp equivalent is half the value.
constexpr int     RANDOM_PLIES_MIN = 8;
constexpr int64_t VERIFY_NODES     = 5000;
constexpr int     OPENING_MAX_CP   = 600;    // discard openings past ~300 UCI cp (white-rel)
constexpr int     WIN_ADJ_CP       = 2000;   // win-adjudicate at ~1000 UCI cp
constexpr int     WIN_ADJ_PLIES    = 4;
constexpr int     DRAW_ADJ_CP      = 16;     // draw-adjudicate at ~8 UCI cp
constexpr int     DRAW_ADJ_PLIES   = 10;
constexpr int     DRAW_ADJ_MIN_PLY = 40;
constexpr int     MAX_GAME_PLIES   = 400;
constexpr int     MATE_CP          = MIN_MATE_SCORE;  // |eval| at/above this means a mate was found

// Each completed game is written and flushed to the OS immediately, so a hard
// kill (console-window close, crash, taskkill) can lose at most the single
// in-flight game per worker. Records are self-delimiting and the analyzer skips
// a truncated tail, so a partially written final game stays readable. Games are
// long enough (~100+ plies) that a flush per game is negligible I/O.

// ---- shared run state ----
std::atomic<bool>     g_stop{false};
std::atomic<uint64_t> g_positions{0};
std::atomic<uint64_t> g_games{0};
std::atomic<uint64_t> g_white{0};
std::atomic<uint64_t> g_draw{0};
std::atomic<uint64_t> g_black{0};

void onSignal(int) { g_stop.store(true); }

// splitmix64 - small, fast, adequate for uniform opening-move selection.
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) :
        s(seed) {}
    uint64_t next() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z          = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z          = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    int nextInt(int n) { return static_cast<int>(next() % static_cast<uint64_t>(n)); }
};

inline void appendBytes(std::vector<uint8_t>& v, const void* p, size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    v.insert(v.end(), b, b + n);
}

inline int whiteRelative(int cp, Color stm) { return stm == WHITE ? cp : -cp; }

// Raw evals reach ~32000 for mate scores; clamp just inside the int16 range so
// those survive while any stray VALUE_INFINITE sentinel is bounded.
inline int16_t clampEval(int cp) { return static_cast<int16_t>(std::clamp(cp, -32000, 32000)); }

void packBoard(const Board& b, MarlinPackedBoard& out) {
    uint64_t occ  = b.occupied[WHITE] | b.occupied[BLACK];
    out.occupancy = occ;
    std::memset(out.pieces, 0, sizeof(out.pieces));

    int      idx = 0;
    uint64_t bb  = occ;
    while (bb)
    {
        int sq    = poplsb(bb);
        int piece = b.pieceBoard[sq];  // 0..5 white, 8..13 black == marlin base nibble
        int nib   = piece;

        if (piece == WHITE_ROOK)
        {
            if ((sq == b.castlingRooks[0] && (b.castlings & WHITE_SHORT_CASTLE)) || (sq == b.castlingRooks[1] && (b.castlings & WHITE_LONG_CASTLE)))
                nib = 6;
        }
        else if (piece == BLACK_ROOK)
        {
            if ((sq == b.castlingRooks[2] && (b.castlings & BLACK_SHORT_CASTLE)) || (sq == b.castlingRooks[3] && (b.castlings & BLACK_LONG_CASTLE)))
                nib = 14;
        }

        out.pieces[idx >> 1] |= (idx & 1) ? static_cast<uint8_t>(nib << 4) : static_cast<uint8_t>(nib);
        idx++;
    }

    int ep             = (b.enPassant == NO_SQ) ? 64 : b.enPassant;
    out.stmEpSquare    = static_cast<uint8_t>(((b.sideToMove == BLACK) ? 0x80 : 0) | ep);
    out.halfmoveClock  = b.halfMove;
    out.fullmoveNumber = b.fullMove;
    out.eval           = 0;
    out.wdl            = WDL_DRAW;
    out.extra          = 0;
}

// Rebuild the accumulator from scratch for a fresh search on `board` (mirrors
// what Uci::go does before starting a search).
inline void prepareEval(Board& board) {
    board.nnueData.size = 0;
    board.nnueData.accumulator[0].clear();
    NNUE::Instance()->calculateInputLayer(board, 0, true);
}

// One worker: plays independent games on its own Search/Board and streams them.
void worker(int id, std::string outDir, int64_t softNodes, int64_t hardNodes, uint64_t runStamp, uint64_t seed, int temperaturePct) {
    Search search;
    search.setThread(1);
    Board& board = search.threads[0]->board;

    Stack* ss = new Stack[MAX_PLY + 10];
    Rng    rng(seed);

    std::string   path = outDir + "/devre_" + std::to_string(runStamp) + "_" + std::to_string(id) + ".bin";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        std::cerr << "datagen: worker " << id << " could not open " << path << std::endl;
        delete[] ss;
        return;
    }

    std::vector<uint8_t> rec;
    rec.reserve(4096);

    uint64_t localGames = 0;

    while (!g_stop.load(std::memory_order_relaxed))
    {
        // ---- random opening ----
        int randomPlies = RANDOM_PLIES_MIN + static_cast<int>(localGames & 1);  // alternate 8/9

        board       = Board(START_FEN);
        bool aborted = false;
        for (int i = 0; i < randomPlies; i++)
        {
            MoveList ml;
            legalmoves<ALL_MOVES>(board, ml);
            if (ml.numMove == 0)
            {
                aborted = true;
                break;
            }
            board.makeMove(ml.moves[rng.nextInt(ml.numMove)], false);
        }
        if (aborted)
            continue;

        {
            MoveList ml;
            legalmoves<ALL_MOVES>(board, ml);
            if (ml.numMove == 0 || board.isDraw())
                continue;
        }

        // ---- verify the opening is not already decided ----
        prepareEval(board);
        SearchResult vr = search.datagenSearch(ss, VERIFY_NODES, hardNodes);
        if (vr.move == NO_MOVE)
            continue;
        int verifyCp = whiteRelative(vr.cp, board.sideToMove);
        if (std::abs(verifyCp) > OPENING_MAX_CP)
            continue;

        // ---- play out the game ----
        rec.clear();
        MarlinPackedBoard hdr;
        packBoard(board, hdr);
        hdr.eval        = clampEval(verifyCp);
        size_t hdrOff   = rec.size();
        appendBytes(rec, &hdr, sizeof(hdr));

        uint8_t  result      = WDL_DRAW;
        int      ply         = 0;
        int      winStreak   = 0;
        int      winSign     = 0;
        int      drawStreak  = 0;
        uint64_t storedMoves = 0;

        while (true)
        {
            if (board.isDraw())
            {
                result = WDL_DRAW;
                break;
            }
            {
                MoveList ml;
                legalmoves<ALL_MOVES>(board, ml);
                if (ml.numMove == 0)
                {
                    if (board.inCheck())
                        result = (board.sideToMove == WHITE) ? WDL_BLACK_WIN : WDL_WHITE_WIN;
                    else
                        result = WDL_DRAW;
                    break;
                }
            }
            if (ply >= MAX_GAME_PLIES)
            {
                result = WDL_DRAW;
                break;
            }

            prepareEval(board);
            SearchResult r  = search.datagenSearch(ss, softNodes, hardNodes);
            uint16_t     bm = r.move;
            int          wcp = whiteRelative(r.cp, board.sideToMove);

            // Move selection noise: with TEMPERATURE_PCT probability, re-search
            // with the best move excluded to find a reasonable second-best move
            // and play that instead. This makes games branch differently without
            // playing outright blunders — the search guarantees the alternative
            // is the next-best move by its own metric.
            if (!g_stop && r.move != NO_MOVE && temperaturePct > 0 && rng.nextInt(100) < temperaturePct)
            {
                SearchResult r2 = search.datagenSearch(ss, softNodes, hardNodes, r.move);
                if (r2.move != NO_MOVE && r2.move != r.move)
                {
                    bm  = r2.move;
                    wcp = whiteRelative(r2.cp, board.sideToMove);
                }
            }

            if (bm == NO_MOVE)
            {
                MoveList ml;
                legalmoves<ALL_MOVES>(board, ml);
                bm = ml.moves[0];
            }

            ScoredMove sm{toViriMove(bm), clampEval(wcp)};
            appendBytes(rec, &sm, sizeof(sm));
            storedMoves++;

            // adjudication on the white-relative score of this position
            int mag = std::abs(wcp);
            if (mag >= MATE_CP)
            {
                result = (wcp > 0) ? WDL_WHITE_WIN : WDL_BLACK_WIN;
                break;
            }
            int sign = (wcp > 0) - (wcp < 0);
            if (mag >= WIN_ADJ_CP)
            {
                if (sign == winSign)
                    winStreak++;
                else
                {
                    winSign   = sign;
                    winStreak = 1;
                }
            }
            else
            {
                winSign   = 0;
                winStreak = 0;
            }
            if (winStreak >= WIN_ADJ_PLIES)
            {
                result = (winSign > 0) ? WDL_WHITE_WIN : WDL_BLACK_WIN;
                break;
            }

            if (ply >= DRAW_ADJ_MIN_PLY && mag <= DRAW_ADJ_CP)
                drawStreak++;
            else
                drawStreak = 0;
            if (drawStreak >= DRAW_ADJ_PLIES)
            {
                result = WDL_DRAW;
                break;
            }

            board.makeMove(bm, false);
            ply++;
        }

        ScoredMove terminator{0, 0};
        appendBytes(rec, &terminator, sizeof(terminator));
        rec[hdrOff + offsetof(MarlinPackedBoard, wdl)] = result;

        // Write the whole game record and push it to the OS right away so an
        // unexpected kill costs at most this single game.
        out.write(reinterpret_cast<const char*>(rec.data()), static_cast<std::streamsize>(rec.size()));
        out.flush();
        localGames++;

        g_games.fetch_add(1, std::memory_order_relaxed);
        g_positions.fetch_add(storedMoves, std::memory_order_relaxed);
        if (result == WDL_WHITE_WIN)
            g_white.fetch_add(1, std::memory_order_relaxed);
        else if (result == WDL_DRAW)
            g_draw.fetch_add(1, std::memory_order_relaxed);
        else
            g_black.fetch_add(1, std::memory_order_relaxed);
    }

    out.flush();
    delete[] ss;
}

void printStats(uint64_t elapsedMs) {
    uint64_t games = g_games.load();
    uint64_t pos   = g_positions.load();
    uint64_t w     = g_white.load();
    uint64_t d     = g_draw.load();
    uint64_t b     = g_black.load();
    double   secs  = elapsedMs / 1000.0;
    double   pps   = secs > 0 ? pos / secs : 0.0;
    double   gps   = secs > 0 ? games / secs : 0.0;
    double   avgLen = games > 0 ? static_cast<double>(pos) / games : 0.0;
    double   total  = games > 0 ? static_cast<double>(games) : 1.0;

    std::cout << "games " << games << " pos " << pos << " | " << static_cast<uint64_t>(pps) << " pos/s " << static_cast<uint64_t>(gps) << " games/s"
              << " | avglen " << avgLen << " | W/D/L " << static_cast<int>(100.0 * w / total) << "/" << static_cast<int>(100.0 * d / total) << "/"
              << static_cast<int>(100.0 * b / total) << "%" << std::endl;
}

}  // namespace

void runDatagen(int argc, char** argv) {
    int         threads        = (argc > 2) ? std::stoi(argv[2]) : static_cast<int>(std::thread::hardware_concurrency());
    std::string outDir         = (argc > 3) ? std::string(argv[3]) : "./data";
    uint64_t    target         = (argc > 4) ? std::stoull(argv[4]) : 0ull;
    int64_t     softNodes      = (argc > 5) ? std::stoll(argv[5]) : 5000;
    int         temperaturePct = (argc > 6) ? std::stoi(argv[6]) : 0;
    int64_t     hardNodes = std::max<int64_t>(softNodes * 40, 200000);

    if (threads < 1)
        threads = 1;

    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);

    // Initialise shared singletons on the main thread before spawning workers.
    TT::Instance()->ttAllocate(std::min(4096, 16 * threads));
    Board warm(START_FEN);
    prepareEval(warm);
    (void) warm.eval();

    std::signal(SIGINT, onSignal);

    std::cout << "Devre " << VERSION << " datagen" << std::endl;
    std::cout << "threads=" << threads << " outDir=" << outDir << " softNodes=" << softNodes << " hardNodes=" << hardNodes
              << " target=" << (target ? std::to_string(target) : std::string("infinite"))
              << " temperature=" << temperaturePct << "%" << std::endl;
    std::cout << "press Ctrl-C to stop" << std::endl;

    uint64_t                 runStamp = currentTime();
    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (int i = 0; i < threads; i++)
    {
        uint64_t seed = runStamp * 0x9E3779B97F4A7C15ull + static_cast<uint64_t>(i) * 0x632BE59BD9B4E019ULL + 1;
        pool.emplace_back(worker, i, outDir, softNodes, hardNodes, runStamp, seed, temperaturePct);
    }

    uint64_t t0        = currentTime();
    uint64_t nextPrint = t0 + 3000;
    while (!g_stop.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        uint64_t now = currentTime();
        if (now >= nextPrint)
        {
            printStats(now - t0);
            nextPrint = now + 3000;
        }
        if (target && g_positions.load() >= target)
        {
            g_stop.store(true);
            break;
        }
    }

    std::cout << "stopping, flushing workers..." << std::endl;
    for (auto& th : pool)
        th.join();

    std::cout << "done. ";
    printStats(currentTime() - t0);
}
