#ifndef DEVRE_NNUE_H
#define DEVRE_NNUE_H

#include "board.h"
#include "types.h"
#include <array>
#include <cstdlib>
#include <limits>
#include <malloc.h>
#include <new>
#include <vector>

template<typename T, std::size_t Alignment>
class AlignedAllocator {
   public:
    using value_type = T;

    AlignedAllocator() noexcept = default;

    template<typename U>
    constexpr AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_alloc();

        const std::size_t bytes = n * sizeof(T);
        const std::size_t alignedBytes = ((bytes + Alignment - 1) / Alignment) * Alignment;
        void* ptr = nullptr;
#if defined(_WIN32)
        ptr = _aligned_malloc(std::max(alignedBytes, Alignment), Alignment);
#else
        ptr = std::aligned_alloc(Alignment, std::max(alignedBytes, Alignment));
#endif
        if (!ptr)
            throw std::bad_alloc();

        return static_cast<T*>(ptr);
    }

    void deallocate(T* ptr, std::size_t) noexcept {
#if defined(_WIN32)
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }

    template<typename U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };
};

template<typename T, typename U, std::size_t Alignment>
constexpr bool operator==(const AlignedAllocator<T, Alignment>&, const AlignedAllocator<U, Alignment>&) noexcept {
    return true;
}

template<typename T, typename U, std::size_t Alignment>
constexpr bool operator!=(const AlignedAllocator<T, Alignment>&, const AlignedAllocator<U, Alignment>&) noexcept {
    return false;
}

class NNUE {
   private:
    NNUE();

    float relationL1Scale = 64.0f;
    float relationL2Scale = 4096.0f;
    float relationL3Scale = 1048576.0f;
    float relationInvL1Squared = 1.0f / (64.0f * 64.0f);
    float relationInvL2Scale = 1.0f / 4096.0f;
    uint32_t relationL1Rows = 0;
    uint32_t relationL1Cols = 0;
    uint32_t relationL2Rows = 0;
    int relationL1Clip = 64;
    std::vector<int8_t, AlignedAllocator<int8_t, 64>> relationL1Weights;
    alignas(64) int16_t relationL1Biases[RELATION_L1_MAX]{};
    std::vector<float> relationL1ActivationLut;
    std::vector<float, AlignedAllocator<float, 64>> relationL2WeightsByInput;
    const float* relationL2WeightsThem = nullptr;
    alignas(64) float relationL2Biases[RELATION_L2_MAX]{};
    alignas(64) float relationL3Weights[RELATION_L2_MAX]{};
    float relationL3Bias = 0.0f;
    std::array<uint32_t, RELATION_BASE_FEATURES * RELATION_BASE_FEATURES> relationLut{};

    static int relationBaseIndex(int piece, int sq, Color perspective);
    uint32_t relationFeatureIndex(int i, int j) const;
    const int8_t* relationRow(uint32_t relationIndex) const;

    int evaluateRelationNet(Board& board);
    int relationHead(const int16_t* us, const int16_t* them) const;
    void applyRelationDeltaAdd(int16_t* accumulator, uint32_t relationIndex) const;
    void applyRelationDeltaBatch(int16_t* accumulator, const int8_t* const* addRows, int addCount, const int8_t* const* subRows, int subCount) const;
    void updateInputLayer(Board& board, int idx, bool fromScratch = false);
    void recalculateInputLayer(Board& board, Color perspective, int idx);
    void incrementalUpdateInputLayer(Board& board, Color perspective, int idx);
    bool loadRelationNetworkFromBuffer(const uint8_t* data, size_t size, const std::string& sourceLabel);
    bool loadRelationNetwork(const std::string& filePath);

   public:
    static float materialScale(Board& board);

    static float halfMoveScale(Board& board);

    void calculateInputLayer(Board& board, int idx, bool fromScratch = false);

    int evaluate(Board& board);

    static NNUE* Instance();

    bool loadNetwork(const std::string& filePath);
};

#endif  //DEVRE_NNUE_H
