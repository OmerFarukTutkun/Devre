#ifndef DEVRE_UCIOPTIONS_H
#define DEVRE_UCIOPTIONS_H

#include <map>
#include <string>
#include <utility>
#include "types.h"

class Option {
   public:
    int         min{}, max{};
    std::string currentValue, type, defaultValue;

    Option(int min, int max, const std::string& currentValue, std::string type) :
        min(min),
        max(max),
        currentValue(currentValue),
        type(std::move(type)),
        defaultValue(currentValue) {}

    Option(const std::string& currentValue, std::string type) :
        currentValue(currentValue),
        type(std::move(type)),
        defaultValue(currentValue) {}

    void printOption(const std::string& name) const {
        std::cout << "option name " + name + " type " + type + " default " + defaultValue;
        if (type == "spin")
        {
            std::cout << " min " << min << " max " << max;
        }
        std::cout << std::endl;
    }

    void updateOption(const std::string& value) {
        if (value.empty())
            return;

        if (type == "spin")
        {
            try
            {
                int x = std::stoi(value);
                if (x <= max && x >= min)
                {
                    currentValue = value;
                }
            } catch (int e)
            {}
        }
        else if (type == "check")
        {
            if (value == "true" || value == "false")
                currentValue = value;
        }
        else
        {
            currentValue = value;
        }
    }
};

using OptionsMap = std::unordered_map<std::string, Option>;
extern OptionsMap Options;

#endif  //DEVRE_UCIOPTIONS_H
