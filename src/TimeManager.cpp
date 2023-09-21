#include "TimeManager.h"
#include "UciOptions.h"
#include "util.h"
bool TimeManager::checkLimits() {

    if(--calls > 0)
        return false;

    calls = period;

    auto elapsed = currentTime() - startTime;
    if(elapsed >= optimalTime)
        return true;
    if(fixedMoveTime != -1)
    {

        auto moveOverhead = Options.at("Move Overhead");
        if(elapsed + std::stoi(moveOverhead.currentValue) >= fixedMoveTime)
            return true;
    }
    return false;
}

TimeManager::TimeManager() {
    depthLimit = MAX_PLY;
    nodeLimit = -1;
    movesToGo = 20;
    fixedMoveTime = -1;
    remainingTime = 1e9;
    inc = 0;
    startTime = 0;
    optimalTime = 0;
    period = 1000;
    calls = period;

}

void TimeManager::start() {
    startTime = currentTime();
    auto moveOverhead = Options.at("Move Overhead");
    optimalTime = remainingTime/8 + inc - std::stoi(moveOverhead.currentValue);
}