#include "tt.h"
#include "move.h"
#include "zobrist.h"
#define MbToByte(x) (1024*1024*(x))

TT::TT() {
    age = 0;
    ttMask = 0ull;
    table = nullptr;
    ttAllocate();
}

TT::~TT() {
    ttFree();
}
void TT::ttSave(uint64_t key, int ply, int16_t score, int16_t staticEval, char bound, uint8_t depth, uint16_t move) {
    TTentry *entry = &table[key & ttMask];
    uint8_t entryBound = entry->ageAndBound & 0b00000011;
    uint8_t entryAge = entry->ageAndBound & 0b11111100;

    if (entry->key == 0ull
        || entry->depth - (age - entryAge) <= depth
        || (bound == TT_EXACT && entryAge < age)) {

        if (score > MIN_MATE_SCORE)
            score += ply;
        else if (-score > MIN_MATE_SCORE)
            score -= ply;
        entry->key = key;
        entry->score = score;
        entry->depth = depth;
        entry->ageAndBound = age + bound;
        entry->staticEval = staticEval;

        if (move != NO_MOVE)
            entry->move = move;
    }
}

bool TT::ttProbe(uint64_t key, int ply, int &ttDepth, int &ttScore, int &ttBound, int &ttStaticEval, uint16_t &ttMove) {
    TTentry entry = table[key & ttMask];
    if (entry.key == key) {
        ttScore = entry.score;
        if (entry.score > MIN_MATE_SCORE)
            ttScore -= ply;
        else if (-entry.score > MIN_MATE_SCORE)
            ttScore += ply;
        ttDepth = entry.depth;
        ttMove = entry.move;
        ttStaticEval = entry.staticEval;
        ttBound = entry.ageAndBound & 0b00000011;
        return true;
    } else
        return false;
}

void TT::ttAllocate(int megabyte) {
    int size = MbToByte(megabyte) / sizeof(TTentry);
    int keySize = static_cast<int> ( log2(size));
    ttMask = (ONE << keySize) - ONE;
    ttFree();
    table = new TTentry[(ONE << keySize)];
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

void TT::updateAge() {
    age += 4;
    if (age > 255)
        age = 0;
}

TT *TT::Instance() {
    static TT instance = TT();
    return &instance;
}
