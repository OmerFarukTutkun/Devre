#ifndef DEVRE_NNUE_H
#define DEVRE_NNUE_H

#include "board.h"
#include "types.h"
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

    int l1Clip = 64;
    std::vector<int8_t, AlignedAllocator<int8_t, 64>> l1Weights;
    alignas(64) int16_t l1Biases[NNUE_L1_MAX]{};
    std::vector<int16_t, AlignedAllocator<int16_t, 64>> l2Weights;
    alignas(64) int32_t l2Biases[NNUE_L2_MAX]{};
    alignas(64) uint32_t l2ClipByRow[NNUE_L2_MAX]{};
    alignas(64) float l3Weights[NNUE_L2_MAX]{};
    float l3Bias = 0.0f;

    static int baseIndex(int piece, int sq, Color perspective);
    uint32_t featureIndex(int i, int j) const;
    const int8_t* getPairWeights(uint32_t feature) const;

    int evaluateNet(Board& board);
    int head(const int16_t* us, const int16_t* them) const;
    void updateInputLayer(Board& board, int idx, bool fromScratch = false);
    void recalculateInputLayer(Board& board, Color perspective, int idx);
    void incrementalUpdateInputLayer(Board& board, Color perspective, int idx);
    bool loadNetFromBuffer(const uint8_t* data, size_t size, const std::string& sourceLabel);
    bool loadNet(const std::string& filePath);

   public:
    static float materialScale(Board& board);

    static float halfMoveScale(Board& board);

    void calculateInputLayer(Board& board, int idx, bool fromScratch = false);

    int evaluate(Board& board);

    static NNUE* Instance();

    bool loadNetwork(const std::string& filePath);
};

#endif  //DEVRE_NNUE_H

