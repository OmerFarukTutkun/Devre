#include "timeManager.h"
#include "uciOptions.h"
#include "util.h"
#include "tuning.h"

DEFINE_PARAM_B(hardTimePercentage, 40, 0, 100);
DEFINE_PARAM_B(softTimePercentage, 5, 0, 100);

bool TimeManager::checkLimits(uint64_t totalNodes) {

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
    startTime              = currentTime();
    auto          option   = Options.at("MoveOverhead");
    const int64_t overhead = std::stoi(option.currentValue);

    hardTime = remainingTime * hardTimePercentage / 100 + inc - overhead;
    hardTime = std::min(hardTime, 80 * remainingTime / 100);

    // The move still has to reach the gui before the flag falls, and with a
    // nearly empty clock the formula above can even go negative. Answering late
    // loses the game outright, so cap on what is really left and keep the limit
    // positive: a search that is out of time must still return a legal move.
    hardTime = std::min(hardTime, remainingTime - overhead);
    hardTime = std::max<int64_t>(hardTime, 1);

    softTime = remainingTime * softTimePercentage / 100 + inc;

    //starting an iteration we cannot finish only burns the little time we have
    softTime = std::min(softTime, hardTime);
}
