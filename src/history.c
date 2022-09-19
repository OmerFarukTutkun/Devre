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
    uint32_t last = pos->move_history[pos->ply];
    uint32_t penultimate = pos->move_history[pos->ply - 1];
    if(last > NULL_MOVE)
        pos->counter_moves[pos->side][move_from(last)][move_to(last)] = moves[length -1 ];//update counter move heuristic
    if(pos->killers[pos->ply][0] != moves[length-1] )
    {
        pos->killers[pos->ply][1] =  pos->killers[pos->ply][0]; 
        pos->killers[pos->ply][0] = moves[length - 1 ];
    }
   
    for(int i=0 ; i< length ; i++)
    {
        if(move_type(moves[i]) < 2)
        {
            uint8_t from = move_from(moves[i]);
            uint8_t to = move_to(moves[i]);
            int16_t* current = &pos->history[check_bit(pos->threat, from)][check_bit(pos->threat, to) ][pos->side][from][to];
            update_history(current, depth, i == length - 1);
            if(last > NULL_MOVE)
            {
                current = &pos->conthist[pos->side][ piece_type(piece(last)) ] [move_to(last) ] [piece_type( pos->board[from])][ to];
                update_history(current, depth, i == length - 1);
                if(penultimate > NULL_MOVE && pos->ply)
                {
                    current = &pos->followuphist[pos->side][ piece_type(piece(penultimate)) ] [move_to(penultimate) ] [piece_type( pos->board[from])][ to];
                    update_history(current, depth, i == length - 1);
                }
            }
        }
    }
}

void update_capture_histories(Position* pos, int depth, uint16_t* moves, int length)
{
    for(int i=0 ; i< length ; i++)
    {
        if(move_type(moves[i]) == CAPTURE)
        {
            uint8_t from = move_from(moves[i]);
            uint8_t to = move_to(moves[i]);
            int16_t* current = &pos->capturehist[pos->side][ piece_type(pos->board[from])][to][piece_type(pos->board[to])];
            update_history(current, depth, i == length - 1);
        }
    }
}

int32_t get_capture_history(Position* pos, uint16_t move)
{
    uint8_t from = move_from(move);
    uint8_t to = move_to(move);
    return pos->capturehist[pos->side][ piece_type(pos->board[from])][to][piece_type(pos->board[to])];
}
int32_t get_history(Position* pos, uint16_t move)
{
    uint8_t from = move_from(move);
    uint8_t to = move_to(move);
    uint32_t last = pos->move_history[pos->ply];
    uint32_t penultimate = pos->move_history[pos->ply - 1];
    int32_t score = pos->history[check_bit(pos->threat, from)][check_bit(pos->threat, to) ][pos->side][from][to];
    if(last > NULL_MOVE)
    {
        score += pos->conthist[pos->side][ piece_type(piece(last)) ] [move_to(last) ] [piece_type( pos->board[from])] [ to];
        if(penultimate > NULL_MOVE && pos->ply)
            score += pos->followuphist[pos->side][ piece_type(piece(penultimate)) ] [move_to(penultimate) ] [piece_type( pos->board[from])][ to];
    }
    return score;        
}