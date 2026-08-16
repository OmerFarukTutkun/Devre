#ifndef DEVRE_MOVEPICK_H
#define DEVRE_MOVEPICK_H

#include "move.h"
#include "movegen.h"
#include "threadData.h"


bool isPseudoLegal(const Board& board, uint16_t move);

bool isLegal(const Board& board, uint16_t move);


constexpr int GOOD_TACTICAL_THRESHOLD = 9500000;

enum PickMode : uint8_t {
    PICK_MAIN,           // alphaBeta
    PICK_QSEARCH,        // qsearch, not in check: tacticals only
    PICK_QSEARCH_CHECK,  // qsearch in check: every evasion, same stages as alphaBeta
};

enum PickStage : uint8_t {
    STAGE_TT,
    STAGE_GEN_TACTICAL,
    STAGE_GOOD_TACTICAL,
    STAGE_REFUTATION,
    STAGE_GEN_QUIET,
    STAGE_REST,
    STAGE_DONE,
};

// Staged move picker. Nothing is generated in the constructor, so a node that
// cuts off early never pays for quiet generation. next() reads the board, so it
// must not be called between makeMove() and unmakeMove().
class MovePicker {
   public:
    MovePicker(ThreadData& thread, Stack* ss, uint16_t ttMove, PickMode mode);

    uint16_t next();

    void skipQuiets();

   private:
    int  bestIndex() const;
    void removeAt(int index);
    void generateTacticals();
    void generateQuiets();
    bool refutationOk(uint16_t move, int index) const;

    ThreadData& m_thread;
    Stack*      m_ss;
    Board*      m_board;
    MoveList    m_list;
    MoveGenInfo m_info;
    uint16_t    m_ttMove;
    uint16_t    m_refutations[3];
    int         m_refutationIndex;
    bool        m_skipQuiets;
    PickMode    m_mode;
    PickStage   m_stage;
};

#endif  //DEVRE_MOVEPICK_H
