#ifndef DEVRE_TIMEMANAGER_H
#define DEVRE_TIMEMANAGER_H

#include "types.h"
#include "util.h"

class TimeManager {

   public:
    int     depthLimit;
    int64_t nodeLimit;
    int     movesToGo;
    int64_t fixedMoveTime;
    int64_t remainingTime;
    int64_t inc;

    uint64_t startTime;
    int64_t  softTime;
    int64_t  hardTime;

    int calls;
    int period;

    bool checkLimits(uint64_t totalNodes);

    //set start time and calculate optimal time to start
    void start();

    TimeManager();
};


#endif  //DEVRE_TIMEMANAGER_H
