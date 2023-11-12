#include "tt.h"
#include "move.h"
#include "zobrist.h"
#define MbToByte(x) (1024*1024*(x))

TT::TT() {
    ttMask = 0ull;
    table = nullptr;
    ttAllocate();
}

TT::~TT() {
    ttFree();
}
void TT::ttSave(uint64_t key, int ply, int16_t score, int16_t staticEval, char bound, uint8_t depth, uint16_t move) {
    TTentry *entry = &table[key & ttMask];

    if (entry->key != key
        || entry->depth  < depth + 4
        || bound == TT_EXACT) {

        if (score > MIN_MATE_SCORE)
            score += ply;
        else if (-score > MIN_MATE_SCORE)
            score -= ply;
        entry->key = key;
        entry->score = score;
        entry->depth = depth;
        entry->bound = bound;
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
        ttBound = entry.bound;
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
