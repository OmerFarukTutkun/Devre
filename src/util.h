#ifndef DEVRE_UTIL_H
#define DEVRE_UTIL_H

#include <vector>
#include <string>
#include <sstream>
#include <iterator>
#include <chrono>

inline std::string popFront(std::vector<std::string>& vec) {
    if (vec.empty())
        return "";
    auto x = vec.at(0);
    vec.erase(vec.begin());
    return x;
}

inline std::vector<std::string> splitString(const std::string& s) {
    std::stringstream        fen_stream(s);
    std::vector<std::string> seglist;
    std::copy(std::istream_iterator<std::string>(fen_stream), std::istream_iterator<std::string>(),
              std::back_inserter(seglist));
    return seglist;
}

inline uint64_t currentTime() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

#endif  //DEVRE_UTIL_H
