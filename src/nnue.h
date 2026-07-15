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

struct NNUEDeltaBatch;

class NNUE {
   private:
    NNUE();

    static NNUE instance;

    alignas(64) int16_t ftWeights[NNUE_FT_IN * NNUE_FT_OUT]{}; // row per feature, 1536 cols
    alignas(64) int16_t ftBiases[NNUE_FT_OUT]{};
    alignas(64) int16_t headWeights[2 * NNUE_FT_OUT]{};        // [us | them] order
    int16_t headBias  = 0;
    bool    loaded    = false;

    static int baseIndex(int piece, int sq, Color perspective);

    int evaluateNet(Board& board);
    int head(const int16_t* us, const int16_t* them) const;
    void updateInputLayer(Board& board, int idx, bool fromScratch = false);
    void recalculateInputLayer(Board& board, Color perspective, int idx);
    bool loadNetFromBuffer(const uint8_t* data, size_t size, const std::string& sourceLabel);

   public:
    static float materialScale(Board& board);

    static float halfMoveScale(Board& board);

    void calculateInputLayer(Board& board, int idx, bool fromScratch = false);

    int evaluate(Board& board);

    static NNUE* Instance() { return &instance; }

    bool loadNetwork(const std::string& filePath);
};

#endif  //DEVRE_NNUE_H

