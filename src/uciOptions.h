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
                                  {"Move Overhead", Option(0, 5000, "50", "spin", "50")},

                                  // spsa
                                  {"IIRDepth",       Option(2, 6, "3", "spin", "3")},
                                  {"rfpDepth",       Option(3, 8, "5", "spin", "5")},
                                  {"rfpMargin",       Option(75, 175, "125", "spin", "125")},
                                  {"razoringDepth",       Option(3, 7, "5", "spin", "5")},
                                  {"razoringMargin",       Option(100, 600, "350", "spin", "350")},
                                  {"nmpBase",       Option(2, 8, "4", "spin", "4")},
                                  {"nmpDivisor",       Option(2, 8, "6", "spin", "6")},
                                  {"nmpEvalDivisor",       Option(100, 300, "200", "spin", "200")},
                                  {"lmpDepth",       Option(3, 10, "5", "spin", "5")},
                                  {"lmpBase",       Option(0, 20, "6", "spin", "6")},
                                  {"fpDepth",       Option(4, 12, "8", "spin", "8")},
                                  {"fpMargin",       Option(0, 160, "80", "spin", "80")},
                                  {"conthistDepth",       Option(1, 6, "3", "spin", "3")},
                                  {"conthistMargin",       Option(-5000, 0, "-3000", "spin", "-3000")},
                                  {"lmrHistoryDivisor",       Option(1000, 10000, "5000", "spin", "5000")},
                                  {"lmrCaptureHistoryDivisor",       Option(1000, 10000, "5000", "spin", "5000")},
                          });

#endif //DEVRE_UCIOPTIONS_H
