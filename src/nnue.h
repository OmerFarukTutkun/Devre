#ifndef DEVRE_NNUE_H
#define DEVRE_NNUE_H

#include "board.h"
#include "types.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class NNUE {
   public:
    // Input space: two disjoint blocks feeding one shared accumulator.
    static constexpr int KING_BUCKETS     = 12;
    static constexpr int PSQ_FEATURES     = 768 * KING_BUCKETS;              // 9216
    static constexpr int PAWN_IDS         = 96;                              // 48 friendly + 48 enemy
    static constexpr int PAWN_PAIRS       = PAWN_IDS * (PAWN_IDS - 1) / 2;   // 4560
    static constexpr int NUM_THREATS      = 59808;
    static constexpr int NUM_TAC_FEATURES = PAWN_PAIRS + NUM_THREATS;        // 64368
    static constexpr int FT_IN            = PSQ_FEATURES + NUM_TAC_FEATURES; // 73584

    // Head: FT -> pairwise -> L1 -> L2 -> scalar.
    static constexpr int PW             = NNUE_FT_OUT / 2;         // 384 pairwise outputs
    static constexpr int L1_SIZE        = 16;
    static constexpr int L2_SIZE        = 32;
    static constexpr int HEAD_SIZE      = L2_SIZE + 2 * L1_SIZE;   // 64, [L2 act | L1 act]
    // The L1 input is consumed as 4-byte groups, the unit one dpbusd lane eats.
    static constexpr int L1_GROUPS      = 2 * PW / 4;              // 192
    static constexpr int OUTPUT_BUCKETS = 1;
    static constexpr int EVAL_SCALE     = 427;
    static constexpr int QA          = 127;  // FT / accumulator scale (int8 weights)
    static constexpr int INPUT_SHIFT = 7;    // pairwise right shift (127*127 >> 7 = 126)

    static constexpr int MAX_PSQ_ACTIVE    = 32;
    static constexpr int MAX_PAIR_ACTIVE   = 96;
    static constexpr int MAX_THREAT_ACTIVE = 128;
    static constexpr int MAX_ACTIVE        = MAX_PSQ_ACTIVE + MAX_PAIR_ACTIVE + MAX_THREAT_ACTIVE;
    static_assert(QA + MAX_ACTIVE * 127 <= 32767, "int16 FT accumulator can overflow");
    static_assert((QA * QA >> INPUT_SHIFT) <= 127, "pairwise output must fit int8");

    static_assert(NNUE_FT_OUT % 2 == 0, "the pairwise stage halves the accumulator");
    static_assert(2 * PW % 4 == 0, "the L1 input must split into whole 4-byte groups");

   private:
    struct NetworkData {
        alignas(64) int8_t  ftPsqWeights[static_cast<size_t>(PSQ_FEATURES) * NNUE_FT_OUT];
        alignas(64) int8_t  ftTacWeights[static_cast<size_t>(NUM_TAC_FEATURES) * NNUE_FT_OUT];
        alignas(64) int16_t ftTacBiases[NNUE_FT_OUT];
        alignas(64) int8_t  l1Weights[L1_GROUPS][4 * L1_SIZE];
        alignas(64) float   l1Norm[L1_SIZE];
        alignas(64) float   l1Biases[L1_SIZE];
        alignas(64) float   l2Weights[L2_SIZE][2 * L1_SIZE];
        alignas(64) float   l2Biases[L2_SIZE];
        alignas(64) float   l3Weights[HEAD_SIZE];
        alignas(64) float   l3Biases;
    };

    struct PerspectiveKey {
        uint8_t bucket;
        uint8_t flip;

        bool operator==(const PerspectiveKey& o) const { return bucket == o.bucket && flip == o.flip; }
        bool operator!=(const PerspectiveKey& o) const { return !(*this == o); }
    };

    NNUE();

    std::unique_ptr<NetworkData> network;
    bool                         loaded = false;

    static PerspectiveKey perspectiveKey(int kingSquare, Color perspective);

    void refresh(const Board& board, Color perspective, PerspectiveKey key, int16_t* psqOut,
                 int16_t* tacOut) const;
    void refreshPsq(const Board& board, Color perspective, PerspectiveKey key, int16_t* out) const;
    void refreshTac(const Board& board, Color perspective, PerspectiveKey key, int16_t* out) const;

    void updatePerspective(Board& board, Color perspective) const;
    void applyPly(Board& board, Color perspective, int idx, PerspectiveKey key, bool doPsq, bool doTac) const;

    void pawnPairDelta(const uint64_t prevPawns[N_COLORS], const uint64_t curPawns[N_COLORS], Color perspective,
                       PerspectiveKey key, const int8_t** addRows, int& nAdd, const int8_t** subRows,
                       int& nSub) const;

    static void computePairwise(const int16_t* stmPsq, const int16_t* stmTac, const int16_t* ntmPsq,
                                const int16_t* ntmTac, uint8_t* out);
    void        l1Dots(const uint8_t* pairwise, int32_t* dots) const;
    int         finishHead(const int32_t* dots) const;
    int         runHead(const int16_t* stmPsq, const int16_t* stmTac, const int16_t* ntmPsq,
                        const int16_t* ntmTac) const;

    bool loadFromBuffer(const uint8_t* data, size_t size, const std::string& sourceLabel);

   public:

    static int psqFeature(int piece, int square, Color perspective, PerspectiveKey key);
    static int pawnId(int square, Color pawnColor, Color perspective, int flip);
    static int pawnPairIndex(int idA, int idB);

    static float materialScale(Board& board);
    static float halfMoveScale(Board& board);

    // Called from makeMove, the last point where the node's board state exists,
    // so the whole subtree below can delta from here instead of each leaf
    // rebuilding from the bias.
    void refreshOnBucketChange(Board& board, Color moved) const;

    void calculateInputLayer(Board& board, int idx, bool fromScratch = false);
    int evaluate(Board& board);
    bool isLoaded() const { return loaded; }

    static NNUE  instance;
    static NNUE* Instance() { return &instance; }

    bool loadNetwork(const std::string& filePath);
};

#endif  // DEVRE_NNUE_H
