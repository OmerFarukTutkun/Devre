#include "timeManager.h"
#include "uciOptions.h"
#include "util.h"
#include "tuning.h"

DEFINE_PARAM_B(hardTimePercentage, 40, 0, 100);
DEFINE_PARAM_B(softTimePercentage, 5, 0, 100);

bool TimeManager::checkLimits(uint64_t totalNodes) {

    if (--calls > 0)
        return false;

    calls = period;

    auto elapsed = currentTime() - startTime;
    if (elapsed >= hardTime)
        return true;
    if (fixedMoveTime != -1)
    {
        auto moveOverhead = Options.at("MoveOverhead");
        if (elapsed + std::stoi(moveOverhead.currentValue) >= fixedMoveTime)
            return true;
    }
    if (nodeLimit != -1)
    {
        if (totalNodes >= nodeLimit)
            return true;
    }
    return false;
}

TimeManager::TimeManager() {
    depthLimit    = MAX_PLY;
    nodeLimit     = -1;
    movesToGo     = 20;
    fixedMoveTime = -1;
    remainingTime = 1e9;
    inc           = 0;
    startTime     = 0;
    softTime      = 0;
    period        = 1000;
    calls         = period;
    hardTime      = 0;
}

void TimeManager::start() {
    startTime         = currentTime();
    auto moveOverhead = Options.at("MoveOverhead");
    hardTime =
      remainingTime * hardTimePercentage / 100 + inc - std::stoi(moveOverhead.currentValue);
hardTime = std::min(hardTime, 80*remainingTime/100);
    softTime =
      remainingTime * softTimePercentage / 100 + inc - std::stoi(moveOverhead.currentValue);
}