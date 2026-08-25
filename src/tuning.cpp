#include "tuning.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace {

std::vector<EngineParam*>& tuningParams() {
    static std::vector<EngineParam*> params;
    return params;
}

}  // namespace

void registerParam(EngineParam* param) { tuningParams().push_back(param); }

EngineParam* findParam(std::string name) {
    for (auto* param : tuningParams())
    {
        if (param->name == name)
            return param;
    }
    return nullptr;
}

std::string paramsToUci() {
    std::ostringstream ss;

    for (auto* param : tuningParams())
    {
        ss << "option name " << param->name << " type spin default " << param->value << " min " << param->min << " max " << param->max << "\n";
    }

    return ss.str();
}

std::string paramsToSpsaInput() {
    std::ostringstream ss;

    for (auto* param : tuningParams())
    {
        ss << param->name << ", "
           << "int"
           << ", " << double(param->value) << ", " << double(param->min) << ", " << double(param->max) << ", " << std::max(0.5, double(param->max - param->min) / 20.0) << ", " << 0.002 << "\n";
    }

    return ss.str();
}
