#ifndef _ATTACK_H_
#define _ATTACK_H_
#include "board.h"
#include "PieceList.h"
int offset[6][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0},
    {-21, -19, -12, -8, 8, 12, 19, 21}, // KNIGHT
    {-11, -9, 9, 11, 0, 0, 0, 0},       // BISHOP
    {-10, -1, 1, 10, 0, 0, 0, 0},       // ROOK
    {-11, -10, -9, -1, 1, 9, 10, 11},   // QUEEN
    {-11, -10, -9, -1, 1, 9, 10, 11}    // KING
};

// turn vector to unit vector
int8_t vector_to_unit_vector[155] =
    {
        -11, 0, 0, 0, 0, -9, 0, -10, 0, 0, 0, -11, 0, 0, -9, 0, 0, -10, 0, 0, 0, 0, -11, -9, 0, 0, 0, -10, 0, 0,
        0, 0, -9, -11, 0, 0, 0, -10, 0, 0, 0, -9, 0, 0, -11, 0, 0, -10, 0, 0, -9, 0, 0, 0, 0, -11, 0, -10, 0, -9,
        0, 0, 0, 0, 0, 0, -11, -10, -9, 0, -1, -1, -1, -1, -1, -1, -1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 9, 10, 11, 0,
        0, 0, 0, 0, 0, 9, 0, 10, 0, 11, 0, 0, 0, 0, 9, 0, 0, 10, 0, 0, 11, 0, 0, 9, 0, 0, 0, 10, 0, 0,
        0, 11, 9, 0, 0, 0, 0, 10, 0, 0, 0, 9, 11, 0, 0, 0, 0, 10, 0, 0, 9, 0, 0, 11, 0, 0, 0, 10, 0, 9,
        0, 0, 0, 0, 11};
int ortogonal_or_diagonal[43] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, -1, 1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// is_move_possible[KNIGHT-1][vector + 77] will tell us if vector is knight move or not
//  it is used to fasten is_square_attacked.
uint8_t is_move_possible[4][155] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,
     0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1,
     0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
     0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1,
     0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0,
     0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0,
     0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1,
     0, 0, 0, 0, 1},
    {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
     0, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0,
     0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1,
     0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0,
     0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0,
     0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1,
     0, 0, 0, 0, 1}};
void add_to_attackers(uint64_t *attackers, int piece, int sq)
{
    attackers[piece] = attackers[piece] | (ONE << mailbox[sq]);
}
int get_smallest_attacker(uint64_t *attackers, int *square, int side) // return least valuable attacker piece type
{
    for (int i = 1 + 8 * side; i <= 6 + 8 * side; i++)
    {
        if (attackers[i])
        {
            *square = mailbox64[bitScanForward(attackers[i])];
            return i;
        }
    }
    return 0;
}
int is_square_attacked(Position *pos, uint8_t square, int side)
{
    uint8_t enemy;
    int8_t unit_vector;
    uint8_t enemy_king = BLACK_KING, enemy_knight = BLACK_KNIGHT;
    int i = 0;
    if (!side)
    {
        if (pos->board[square - 9] == PAWN || pos->board[square - 11] == PAWN) // if enemy pawn can capture our piece return TRUE
            return TRUE;
        enemy_king = KING;
        enemy_knight = KNIGHT;
    }
    else
    {
        if (pos->board[square + 9] == BLACK_PAWN || pos->board[square + 11] == BLACK_PAWN)
            return TRUE;
    }
    switch (square - PieceList[enemy_king][0]) // if enemy king can capture our piece return TRUE
    {
    case 1:  case 9: case 10:  case 11:
    case -1: case -9:case -11: case -10:
        return TRUE;
    default:
        break;
    }
    for (int j = enemy_knight; j <= enemy_knight + 3; j++) // enemy knights, bishops, rooks, queens
    {
        for (i = 0; i < MAX_PIECE_NUMBER; i++)
        {
            enemy = PieceList[j][i];
            if (enemy) // is there a piece
            {
                if (is_move_possible[j - enemy_knight][enemy - square + 77])
                {
                    if (j == enemy_knight)
                        return TRUE;
                    unit_vector = vector_to_unit_vector[square - enemy + 77]; // unit vector from enemy piece to our square
                    while (TRUE)
                    {
                        enemy += unit_vector;
                        if (enemy == square)
                            return TRUE;
                        if (pos->board[enemy] != EMPTY)
                        {
                            break;
                        }
                    }
                }
            }
            else
                break;
        }
    }
    return FALSE;
}
void square_attacked_by(Position *pos, uint8_t square, uint64_t attackers[])
{
    if (pos->board[square - 9] == PAWN)
    {
        add_to_attackers(&attackers[0], PAWN, square - 9);
    }
    if (pos->board[square - 11] == PAWN) // if enemy pawn can capture our piece return TRUE
    {
        add_to_attackers(&attackers[0], PAWN, square - 11);
    }
    if (pos->board[square + 9] == BLACK_PAWN)
    {
        add_to_attackers(&attackers[0], BLACK_PAWN, square + 9);
    }
    if (pos->board[square + 11] == BLACK_PAWN) // if enemy pawn can capture our piece return TRUE
    {
        add_to_attackers(&attackers[0], BLACK_PAWN, square + 11);
    }
    switch (square - PieceList[KING][0]) // if enemy king can capture our piece return TRUE
    {
    case 1: case 9: case 10: case 11:
    case -1: case -9:  case -11: case -10:
        add_to_attackers(&attackers[0], KING, PieceList[KING][0]);
    default:
        break;
    }
    switch (square - PieceList[BLACK_KING][0]) // if enemy king can capture our piece return TRUE
    {
    case 1: case 9: case 10: case 11:
    case -1:  case -9: case -11: case -10:
        add_to_attackers(&attackers[0], BLACK_KING, PieceList[BLACK_KING][0]);
    default:
        break;
    }
    int piece, sq;
    if (PieceList[KNIGHT][0] || PieceList[BLACK_KNIGHT][0])
    {
        for (int i = 0; i < 8; i++) // knight
        {
            sq = offset[1][i] + square;
            piece = pos->board[sq];
            if ((piece & 7) == KNIGHT)
            {
                add_to_attackers(&attackers[0], piece, sq);
            }
        }
    }
    if (PieceList[BISHOP][0] || PieceList[BLACK_BISHOP][0] || PieceList[QUEEN][0] || PieceList[BLACK_QUEEN][0])
    {
        for (int i = 0; i < 4; i++) // bishop
        {
            piece = EMPTY;
            sq = square;
            while (piece == EMPTY)
            {
                sq += offset[2][i];
                piece = pos->board[sq];
            }
            if ((piece & 7) == QUEEN || (piece & 7) == BISHOP)
            {
                add_to_attackers(&attackers[0], piece, sq);
            }
        }
    }
    if (PieceList[ROOK][0] || PieceList[BLACK_ROOK][0] || PieceList[QUEEN][0] || PieceList[BLACK_QUEEN][0])
    {
        for (int i = 0; i < 4; i++) // rook
        {
            piece = EMPTY;
            sq = square;
            while (piece == EMPTY)
            {
                sq += offset[3][i];
                piece = pos->board[sq];
            }
            if ((piece & 7) == QUEEN || (piece & 7) == ROOK)
            {
                add_to_attackers(&attackers[0], piece, sq);
            }
        }
    }
}
#endif