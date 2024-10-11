#ifndef DEVRE_UCI_H
#define DEVRE_UCI_H


#include "search.h"
#include "board.h"
#include "uciOptions.h"
#include "timeManager.h"

class Uci {
   private:
    Board*      board;
    Search      search;
    TimeManager timeManager;
    std::thread searchThread;

    static void printUci();

    void eval();

    void stop();

    void perft(std::vector<std::string>& commands);

    void setPosition(std::vector<std::string>& commands);

    void go(std::vector<std::string>& commands);

    void setoption(std::vector<std::string>& commands);

   public:
    Uci();

    virtual ~Uci();

    void UciLoop();
};


#endif  //DEVRE_UCI_H
