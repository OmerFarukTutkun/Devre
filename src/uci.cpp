#include "uci.h"
#include "perft.h"
#include "util.h"
#include "move.h"
#include "tt.h"
#include "nnue.h"
#include "bench.h"
#include "tuning.h"

void Uci::UciLoop() {

    std::cout << "Devre  " << VERSION << " by Omer Faruk Tutkun" << std::endl;
    std::string line;

    bool quit = false;
    while (!quit && std::getline(std::cin, line)) {
        auto commands = splitString(line);
        auto cmd = popFront(commands);

        if (cmd == "stop" || cmd == "quit") {
            this->stop();
            quit = (cmd == "quit");
        } else if (cmd == "uci")
            printUci();
        else if (cmd == "perft")
            perft(commands);
        else if (cmd == "isready")
            std::cout << "readyok" << std::endl;
        else if (cmd == "position")
            setPosition(commands);
        else if (cmd == "print")
            board->print();
        else if (cmd == "go")
            go(commands);
        else if (cmd == "bench")
            bench(2, nullptr);
        else if (cmd == "eval")
            eval();
        else if (cmd == "ucinewgame") {

            TT::Instance()->ttClear();

            auto option = Options.at("Threads");
            search.setThread(stoi(option.currentValue));

        } else if (cmd == "setoption")
            setoption(commands);
        else if (cmd == "tune")       std::cout << paramsToSpsaInput();
    }
}

void Uci::printUci() {
    std::cout << "id name Devre " << VERSION << std::endl;
    std::cout << "id author Omer Faruk Tutkun" << std::endl;

    for (auto const &x: Options) {
        Option option = x.second;
        option.printOption(x.first);
    }
    std::cout << paramsToUci() << "uciok" << std::endl;
}

void Uci::perft(std::vector<std::string> &commands) {
    if (!commands.empty()) {
        int depth = std::stoi(commands.at(0));
        perftTest(*board, depth, false);
    }
}

void Uci::setPosition(std::vector<std::string> &commands) {
    auto cmd = popFront(commands);
    std::string fen = START_FEN;
    if (cmd == "fen") {
        fen = "";
        for (const auto &x: commands)
            fen += " " + x;
    }
    delete board;
    board = new Board(fen);
    while (!commands.empty()) {
        cmd = popFront(commands);
        if (cmd == "moves") {
            while (!commands.empty()) {
                auto uciMove = popFront(commands);
                auto move = moveFromUci(*board, uciMove);
                if (move)
                    board->makeMove(move, false);
            }
            break;
        }
    }
    board->nnueData.clear();
    NNUE::Instance()->calculateInputLayer(*board, true);
}

void Uci::eval() {
    int score = board->eval();
    for (int i = 7; i >= 0; i--) {
        std::cout << "\n  |-------|-------|-------|-------|-------|-------|-------|-------|\n";
        for (int j = 0; j < 8; j++) {
            int piece = board->pieceBoard[8 * i + j];
            std::cout << "       " << PIECE_TO_CHAR.at(piece);
        }
        std::cout << "\n" << i + 1;
        for (int j = 0; j < 8; j++) {
            int sq = 8 * i + j;
            int piece = board->pieceBoard[sq];
            if (piece != EMPTY && piece != BLACK_KING && piece != WHITE_KING) {
                board->removePiece(piece, sq);
                NNUE::Instance()->calculateInputLayer(*board, true);
                printf("%8.2f", (score - board->eval()) / 100.0);
                board->addPiece(piece, sq);
                board->nnueData.popAccumulator();
            } else {
                std::cout << "        ";
            }
        }
    }
    board->nnueData.nnueChanges.clear();
    printf("\n  |-------|-------|-------|-------|-------|-------|-------|-------|\n");
    printf("\n%8c%8c%8c%8c%8c%8c%8c%8c", 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h');
    printf("\n\nScore: %.2f (this score is multiplied by 0.5 when printing uci info)\n", score / 100.0);
}

void Uci::go(std::vector<std::string> &commands) {
    this->stop();
    timeManager = TimeManager();

    auto cmd = popFront(commands);
    auto cmdTime = (board->sideToMove == WHITE) ? "wtime" : "btime";
    auto cmdInc = (board->sideToMove == WHITE) ? "winc" : "binc";
    while (!cmd.empty()) {
        if (cmd == cmdTime)
            timeManager.remainingTime = std::stoi(popFront(commands));
        else if (cmd == cmdInc)
            timeManager.inc = std::stoi(popFront(commands));
        else if (cmd == "movestogo")
            timeManager.movesToGo = std::stoi(popFront(commands));
        else if (cmd == "depth")
            timeManager.depthLimit = std::stoi(popFront(commands));
        else if (cmd == "nodes")
            timeManager.nodeLimit = std::stoi(popFront(commands));
        else if (cmd == "movetime")
            timeManager.fixedMoveTime = std::stoi(popFront(commands));

        cmd = popFront(commands);
    }
    timeManager.start();
    searchThread = std::thread(&Search::start, &search, board, &timeManager, 0);
}

void Uci::setoption(std::vector<std::string> &commands) {
    popFront(commands);
    auto name = popFront(commands);
    auto it = Options.find(name);
    if (it != Options.end()) {
        popFront(commands);
        auto value = popFront(commands);
        it->second.updateOption(value);
        if (name == "Hash")
            TT::Instance()->ttAllocate(stoi(it->second.currentValue));
        if (name == "Threads")
            search.setThread(stoi(it->second.currentValue));

        return;
    }
    else if constexpr (doTuning) {
        EngineParam* param = findParam(name);
        popFront(commands);
        auto value = popFront(commands);
        if (param) {
            param->value = std::stoi(value);
            search.initSearchParameters();
            return;
        }
    }

    std::cout << "undefined option" << std::endl;
}

Uci::Uci() {
    board = new Board();
    board->nnueData.clear();
    NNUE::Instance()->calculateInputLayer(*board, true);

    search = Search();
    auto option = Options.at("Threads");
    search.setThread(stoi(option.defaultValue));
    timeManager = TimeManager();
}

void Uci::stop() {
    search.stop();
    if (searchThread.joinable()) {
        searchThread.join();
    }
}

Uci::~Uci() {
    delete board;
}
