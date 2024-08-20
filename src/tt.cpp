#include <cassert>
#include "tt.h"
#include "move.h"
#include "zobrist.h"
#define MbToByte(x) (1024*1024*(x))

TT::TT() {
    ttMask = 0ull;
    table = nullptr;
    age = 0;
    ttAllocate();
}

TT::~TT() {
    ttFree();
}
void TT::ttSave(uint64_t key, int ply, int16_t score, int16_t staticEval, char bound, uint8_t depth, uint16_t move) {
    TTBucket* bucket = &table[key & ttMask];
    auto key32 = (key >> 32);


    auto * replace = &bucket->entries[0];
    for (int i = 0; i < numEntryPerBucket; i++) {
        auto * entry = &bucket->entries[i];

        if(entry->key == key32) {
            replace = entry;
            break;
        }

        auto replaceAgeDiff = (64 + this->age - getAge(replace)) & 63;
        auto entryAgeDiff   = (64 + this->age - getAge(entry)) & 63;

        // find an entry with lower depth & high age to save our TT info
        if( (replace->depth - replaceAgeDiff*8)
          > (entry->depth - entryAgeDiff*8))
        {
            replace = entry;
        }
    }

    if (move != NO_MOVE || key32 != replace->key)
        replace->move = move;

    if (replace->key != key32
        || bound == TT_EXACT
        || depth + 5 > replace->depth
        || getAge(replace) != age)
    {

        if (score > MIN_MATE_SCORE)
            score += ply;
        else if (-score > MIN_MATE_SCORE)
            score -= ply;

        replace->key = key32;
        replace->score = score;
        replace->depth = depth;
        replace->ageBound = bound | (age <<2);
        replace->staticEval = staticEval;
    }
}

bool TT::ttProbe(uint64_t key, int ply, int &ttDepth, int &ttScore, int &ttBound, int &ttStaticEval, uint16_t &ttMove) {

    TTBucket* bucket = &table[key & ttMask];
    auto key32 = (key >> 32);
    for (int i = 0; i < numEntryPerBucket; i++) {
        auto * entry = &bucket->entries[i];

        if (entry->key == key32) {

            ttScore = entry->score;
            if (entry->score > MIN_MATE_SCORE)
                ttScore -= ply;
            else if (-entry->score > MIN_MATE_SCORE)
                ttScore += ply;

            ttDepth = entry->depth;
            ttMove = entry->move;
            ttStaticEval = entry->staticEval;
            ttBound = entry->ageBound & 3;
            return true;
        }
    }

    return false;
}

void TT::ttAllocate(int megabyte) {
    int size = MbToByte(megabyte) / sizeof(TTBucket);
    int keySize = static_cast<int> ( log2(size));
    ttMask = (ONE << keySize) - ONE;
    ttFree();
    table = new TTBucket[(ONE << keySize)];
    ttClear();
}

void TT::ttClear() {
    age = 0;
    for(int i=0; i <= ttMask; i++)
        table[i] = {};
}

void TT::ttFree() {
    delete[] table;
}

void TT::ttPrefetch(uint64_t hash) {
    __builtin_prefetch(&table[hash & ttMask]);
}

TT *TT::Instance() {
    static TT instance = TT();
    return &instance;
}

void TT::updateAge() {
    //6 bit for age
    age = (age + 1) & 63;
}

uint8_t TT::getAge(TTentry * entry) {
    return (entry->ageBound >> 2);
}

int TT::getHashfull() {
    int hit = 0;
    for (int i = 0; i < 1000; i++) {
        TTBucket *bucket = &table[i];

        for (int j = 0; j < numEntryPerBucket; j++) {
            auto * entry = &bucket->entries[j];
            if (entry->key != 0u && age == getAge(entry))
                hit++;
        }
    }
    return hit / numEntryPerBucket;
}
