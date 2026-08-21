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
    static constexpr int KING_BUCKETS   = 12;
    static constexpr int PSQ_FEATURES   = 768 * KING_BUCKETS;            // 9216
    static constexpr int PAWN_IDS       = 96;                            // 48 friendly + 48 enemy
    static constexpr int PAWN_PAIRS     = PAWN_IDS * (PAWN_IDS - 1) / 2; // 4560
    static constexpr int PW             = NNUE_FT_OUT / 2;
    static constexpr int L1_SIZE        = 16;
    static constexpr int L2_SIZE        = 32;
    static constexpr int HEAD_SIZE      = L2_SIZE + 2 * L1_SIZE;         // 64, [L2 act | L1 act]
    static constexpr int OUTPUT_BUCKETS = 8;

    static constexpr int QA          = 127;  // FT / accumulator scale
    static constexpr int INPUT_SHIFT = 7;    // pairwise right shift
    static constexpr int L1_QUANT    = 64;   // int8 L1 weight scale

    static_assert(NNUE_FT_IN == PSQ_FEATURES + PAWN_PAIRS, "input space must be psq + pawn pairs");
    static_assert(NNUE_FT_OUT % 2 == 0, "the pairwise stage halves the accumulator");

   private:
    struct NetworkData {
        alignas(64) int8_t  ftWeights[static_cast<size_t>(NNUE_FT_IN) * NNUE_FT_OUT];
        alignas(64) int16_t ftBiases[NNUE_FT_OUT];
        alignas(64) int8_t  l1Weights[OUTPUT_BUCKETS * L1_SIZE][2 * PW];
        alignas(64) float   l1Norm[OUTPUT_BUCKETS * L1_SIZE];
        alignas(64) float   l1Biases[OUTPUT_BUCKETS * L1_SIZE];
        alignas(64) float   l2Weights[OUTPUT_BUCKETS][2 * L1_SIZE][L2_SIZE];
        alignas(64) float   l2Biases[OUTPUT_BUCKETS * L2_SIZE];
        alignas(64) float   l3Weights[OUTPUT_BUCKETS][HEAD_SIZE];
        alignas(64) float   l3Biases[OUTPUT_BUCKETS];
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

    void refresh(const Board& board, Color perspective, int16_t* accumulator) const;
    void refreshFromCache(Board& board, Color perspective, PerspectiveKey key, int16_t* accumulator) const;
    void updatePerspective(Board& board, Color perspective) const;
    void applyPly(Board& board, Color perspective, int idx, PerspectiveKey key) const;

    void pawnPairDelta(const uint64_t prevPawns[N_COLORS], const uint64_t curPawns[N_COLORS], Color perspective,
                       PerspectiveKey key, const int8_t** addRows, int& nAdd, const int8_t** subRows,
                       int& nSub) const;

    static void computePairwise(const int16_t* stm, const int16_t* nstm, uint8_t* out);
    void        l1Dots(const uint8_t* pairwise, int bucket, int32_t* dots) const;
    int         finishHead(const int32_t* dots, int bucket) const;
    int         runHead(const int16_t* stm, const int16_t* nstm, int bucket) const;

    bool loadFromBuffer(const uint8_t* data, size_t size, const std::string& sourceLabel);

   public:

    static int psqFeature(int piece, int square, Color perspective, PerspectiveKey key);
    static int pawnId(int square, Color pawnColor, Color perspective, int flip);
    static int pawnPairIndex(int idA, int idB);
    static int outputBucket(const Board& board);

    static float materialScale(Board& board);
    static float halfMoveScale(Board& board);

    void calculateInputLayer(Board& board, int idx, bool fromScratch = false);
    int evaluate(Board& board);
    bool isLoaded() const { return loaded; }

    static NNUE  instance;
    static NNUE* Instance() { return &instance; }

    bool loadNetwork(const std::string& filePath);
};

#endif  // DEVRE_NNUE_H
