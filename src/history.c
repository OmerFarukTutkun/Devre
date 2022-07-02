#include "history.h"
//Most things are taken from Ethereal
const int HistoryDivisor = 16384;
int stat_bonus(int depth) {

    return depth > 13 ? 32 : 16 * depth * depth + 128 *(depth -1 );
}
void update_history(int16_t *current, int depth, bool good) {

    const int delta = good ? stat_bonus(depth) : -stat_bonus(depth);
    *current += delta - *current * abs(delta) / HistoryDivisor;
}

void update_histories(Position* pos, int depth, uint16_t* moves, int length)
{
    if ((length == 1 && depth <= 3))
        return;
    uint16_t prv_move = pos->move_history[pos->ply];
    if(prv_move > NULL_MOVE)
        pos->counter_moves[pos->side][move_from(prv_move)][move_to(prv_move)] = moves[length -1 ];//update counter move heuristic
    
    pos->killer[pos->ply] = moves[length - 1 ];
    for(int i=0 ; i< length ; i++)
    {
        if(move_type(moves[i]) < 2)
        {
            uint8_t from = move_from(moves[i]);
            uint8_t to = move_to(moves[i]);
            int16_t* current = &pos->history[check_bit(pos->threat, from)][check_bit(pos->threat, to) ][pos->side][from][to];
            update_history(current, depth, i == length - 1);
            if(prv_move > NULL_MOVE)
            {
                current = &pos->conthist[pos->side][ piece_type(pos->board[move_to(prv_move)]) ] [move_to(prv_move) ] [piece_type( pos->board[from])][ to];
                update_history(current, depth, i == length - 1);
            }
        }
    }
}
int16_t get_history(Position* pos, uint16_t move)
{
    uint8_t from = move_from(move);
    uint8_t to = move_to(move);
    uint16_t prv_move = pos->move_history[pos->ply];
    int16_t score = pos->history[check_bit(pos->threat, from)][check_bit(pos->threat, to) ][pos->side][from][to];
    if(prv_move > NULL_MOVE)
        score += pos->conthist[pos->side][ piece_type(pos->board[move_to(prv_move)]) ] [move_to(prv_move) ] [piece_type( pos->board[from])] [ to];
    return score;        
}