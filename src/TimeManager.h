#ifndef DEVRE_TIMEMANAGER_H
#define DEVRE_TIMEMANAGER_H

#include "types.h"
#include "util.h"
class TimeManager {

public:
    int depthLimit;
    int64_t nodeLimit;
    int movesToGo;
    int fixedMoveTime;
    int remainingTime;
    int inc;

    uint64_t startTime;
    int optimalTime;

    int calls;
    int period;

    bool checkLimits();
    //set start time and calculate optimal time to start
    void start();

    TimeManager();

};


#endif //DEVRE_TIMEMANAGER_H
