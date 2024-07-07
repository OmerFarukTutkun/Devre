#ifndef DEVRE_TT_H
#define DEVRE_TT_H

#include "types.h"

class TT {
    private:
        TT();
        ~TT();
        uint64_t ttMask;
        TTentry* table;
        void ttFree();
    public:
        void ttSave(uint64_t key, int ply, int16_t score, int16_t staticEval, char bound, uint8_t depth, uint16_t move);
        bool ttProbe(uint64_t key,int ply,int& ttDepth,int& ttScore,int& ttBound,int& ttStaticEval,uint16_t& ttMove);
        void ttAllocate(int megabyte=16);
        void ttClear();
        void ttPrefetch(uint64_t hash);
        void updateAge();
        int getHashfull();
        static TT* Instance();
};

#endif //DEVRE_TT_H
