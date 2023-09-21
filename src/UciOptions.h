#ifndef DEVRE_UCIOPTIONS_H
#define DEVRE_UCIOPTIONS_H

#include <map>
#include <string>
#include <utility>
#include "types.h"

class Option {
public:
    int min, max;
    std::string currentValue, type, defaultValue;

    Option(int min, int max, std::string currentValue, std::string type, std::string defaultValue)
            :
            min(min), max(max),
            currentValue(std::move(currentValue)), type(std::move(type)), defaultValue(std::move(defaultValue)) {}

    void printOption(const std::string &name) const {
        std::cout << "option name " + name + " type " + type + " default " + defaultValue;
        if (type == "spin") {
            std::cout << " min " << min << " max " << max;
        }
        std::cout << std::endl;
    }

    void updateOption(const std::string &value) {
        if (value.empty())
            return;

        if (type == "spin") {
            try {
                int x = std::stoi(value);
                if (x <= max && x >= min) {
                    currentValue = value;
                }
            } catch (int e) {

            }
        } else if (type == "check") {
            if (value == "true" || value == "false")
                currentValue = value;
        } else {
            currentValue = value;
        }
    }
};

using OptionsMap = std::unordered_map<std::string, Option>;
static OptionsMap Options({
                                  {"Threads",       Option(1, 256, "1", "spin", "1")},
                                  {"Hash",          Option(1, 131072, "16", "spin", "16")},
                                  {"UCI_Chess960",  Option(0, 1, "false", "check", "false")},
                                  {"Move Overhead", Option(0, 5000, "50", "spin", "50")}
                          });

#endif //DEVRE_UCIOPTIONS_H
