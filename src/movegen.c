#include "attack.h"
#include "movegen.h"
int build_moves(uint64_t attacks, uint8_t from, uint8_t move_type, uint16_t* moves)
{
    int n=0;
    uint16_t to;
    while(attacks)
    {
        to = poplsb(&attacks);
        moves[n++] = create_move(from, to, move_type);
    }
    return n;
}
int build_pawn_moves(uint64_t attacks, int8_t dir, uint8_t move_type, uint16_t* moves)
{
    int n=0;
    uint16_t to;
    while(attacks)
    {
        to = poplsb(&attacks);
        moves[n++] = create_move(to + dir, to, move_type);
    }
    return n;
}
int build_promotion_moves(uint64_t attacks, int8_t dir, uint8_t move_type, uint16_t* moves, int flag)
{
    int n=0;
    uint16_t to;
    while(attacks)
    {
        to = poplsb(&attacks);
        //do not generate knight,bishop,rook promotions in qsearch
        if(flag == ALL_MOVES)
        {
            moves[n++] = create_move(to + dir, to, move_type);
            moves[n++] = create_move(to + dir, to, move_type + 1);
            moves[n++] = create_move(to + dir, to, move_type + 2);
        }
        moves[n++] = create_move(to + dir, to, move_type + 3);
    }
    return n;
}
int generate_non_pawn_moves(Position* pos, uint8_t piece_type, uint16_t *moves, int flag)
{
    uint64_t our_pieces = pos->bitboards[ piece_index( pos->side, piece_type) ];
    uint64_t occ = pos->occupied[0] | pos->occupied[1];
    uint64_t attacks = 0ull, quiets= 0ull, captures= 0ull;
    uint8_t sq;
    int n=0;
    while(our_pieces)
    {
        sq= poplsb( &our_pieces );
        attacks = get_attacks(occ, sq, piece_type);
        if(flag == ALL_MOVES)
        {
            quiets = attacks & ~occ;
            n += build_moves(quiets, sq, QUIET, &moves[n]);
        }
        captures = attacks & pos->occupied[!pos->side];
        n += build_moves(captures, sq, CAPTURE, &moves[n]);
    }
    return n;
}
int generate_castles(Position* pos, uint16_t* moves)
{
    if(!pos->castlings)
        return 0;
    uint8_t short_castle, long_castle;
    int n=0;
    if(pos->side == BLACK)
    {
        short_castle = (pos->castlings >> 2 ) & 1u;
        long_castle =  (pos->castlings >> 3 ) & 1u;
        if(long_castle) 
        {
            int king = bitScanForward( pos->bitboards[BLACK_KING] );
            int rook = pos->castling_rooks[3];

            uint64_t between = betweenMask(king, C8) | betweenMask(rook,D8) | (ONE << C8) | (ONE << D8);
            uint64_t mask = pos->occupied[WHITE] | pos->occupied[BLACK];
            mask &= ~( ( ONE << king) |  (ONE << rook));
            uint64_t squares_king_passes = betweenMask(king, C8) | (ONE << C8) | (ONE << king);

            if(!(( mask & between) || (squares_king_passes & pos->threat)) )
                moves[n++] = create_move(king,C8, QUEEN_CASTLE);
        }

        if(short_castle) 
        {
            int king = bitScanForward( pos->bitboards[BLACK_KING] );
            int rook = pos->castling_rooks[2];

            uint64_t between = betweenMask(king, G8) | betweenMask(rook,F8) | (ONE << G8) | (ONE << F8);
            uint64_t mask = pos->occupied[WHITE] | pos->occupied[BLACK];
            mask &= ~( ( ONE << king) |  (ONE << rook));
            uint64_t squares_king_passes = betweenMask(king, G8) | (ONE << G8) | (ONE << king);
            
            if(!(( mask & between) || (squares_king_passes & pos->threat)) )
                moves[n++] = create_move(king, G8, KING_CASTLE);
        }
        
    }
    else
    {
        short_castle = pos->castlings  & ONE;
        long_castle =  (pos->castlings >> 1) & ONE;
       if(long_castle) 
        {
            int king = bitScanForward( pos->bitboards[KING] );
            int rook = pos->castling_rooks[1];
            
            uint64_t between = betweenMask(king, C1) | betweenMask(rook,D1) | (ONE << C1) | (ONE << D1);
            uint64_t mask = pos->occupied[WHITE] | pos->occupied[BLACK];
            mask &= ~( ( ONE << king) |  (ONE << rook));
            uint64_t squares_king_passes = betweenMask(king, C1) | (ONE << C1) | (ONE << king);

            if(!(( mask & between) || (squares_king_passes & pos->threat)) )
                moves[n++] = create_move(king,C1, QUEEN_CASTLE);
        }

        if(short_castle) 
        {
            int king = bitScanForward( pos->bitboards[KING] );
            int rook = pos->castling_rooks[0];

            uint64_t between = betweenMask(king, G1) | betweenMask(rook,F1) | (ONE << G1) | (ONE << F1);
            uint64_t mask = pos->occupied[WHITE] | pos->occupied[BLACK];
            mask &= ~( ( ONE << king) |  (ONE << rook));
            uint64_t squares_king_passes = betweenMask(king, G1) | (ONE << G1) | (ONE << king);
            
            if(!(( mask & between) || (squares_king_passes & pos->threat)) )
                moves[n++] = create_move(king, G1, KING_CASTLE);
        }
    }
    return n;
}
int generate_pawn_moves(Position* pos, uint16_t* moves, int flag)
{
    uint64_t pawns = pos->bitboards[ piece_index( pos->side, PAWN) ];
    uint64_t occ = pos->occupied[0] | pos->occupied[1];
    uint64_t captures = 0ull, promotions = 0ull, promotion_captures= 0ull ;
    uint64_t forward = 0ull, left= 0ull, right= 0ull;
    int n=0;
    if(pos->side == BLACK)
    {
        forward =  ( pawns >> 8) & ~occ;

        if(flag == ALL_MOVES)
        {
            // 1-2: Pawn pushes
            uint64_t quiets = forward & ~RANK_1;
            uint64_t double_push = ((forward & RANK_6) >> 8) & ~occ;

            n += build_pawn_moves(quiets, 8, QUIET, &moves[n]);
            n += build_pawn_moves(double_push, 16, DOUBLE_PAWN_PUSH, &moves[n]);
        }
        //3-Promotions
        promotions = forward & RANK_1;
        n +=build_promotion_moves(promotions, 8, KNIGHT_PROMOTION, &moves[n], flag);

        // 4-5: captures and promotion captures to the left
        left = ( ( pawns & ~FILE_H) >> 7) & pos->occupied[!pos->side]; 
        promotion_captures = left & RANK_1;
        captures = left & ~RANK_1;
        n += build_promotion_moves(promotion_captures, 7, KNIGHT_PROMOTION_CAPTURE, &moves[n], flag);
        n += build_pawn_moves(captures, 7, CAPTURE, &moves[n]);

        // 6-7: captures and promotion captures to the right
        right = (( pawns &  ~FILE_A) >> 9 ) & pos->occupied[!pos->side]; ;
        promotion_captures = right & RANK_1;
        captures = right & ~RANK_1;
        n += build_promotion_moves(promotion_captures, 9, KNIGHT_PROMOTION_CAPTURE, &moves[n], flag);
        n += build_pawn_moves(captures, 9, CAPTURE, &moves[n]);

        //8: En passant
        if(pos->en_passant != -1)
        {
            uint64_t ep = PawnAttacks[WHITE][pos->en_passant] & pawns;
            while(ep)
            {
                moves[n++] = create_move(poplsb(&ep), pos->en_passant, EN_PASSANT);
            }
        }
    }
    else
    {
        forward =  ( pawns << 8) & ~occ;

        if(flag == ALL_MOVES)
        {
            // 1-2: Pawn pushes
            uint64_t quiets = forward & ~RANK_8;
            uint64_t double_push = ((forward & RANK_3) << 8) & ~occ;

            n+= build_pawn_moves(quiets, -8, QUIET, &moves[n]);
            n+= build_pawn_moves(double_push, -16, DOUBLE_PAWN_PUSH, &moves[n]);
        }
        //3-Promotions
        promotions = forward & RANK_8;
        n += build_promotion_moves(promotions, -8, KNIGHT_PROMOTION, &moves[n], flag);

        // 4-5: captures and promotion captures to the right
        right = ( ( pawns & ~FILE_H) <<9 ) & pos->occupied[!pos->side]; 
        promotion_captures = right & RANK_8;
        captures = right & ~RANK_8;
        n += build_promotion_moves(promotion_captures, -9, KNIGHT_PROMOTION_CAPTURE, &moves[n], flag);
        n += build_pawn_moves(captures, -9, CAPTURE, &moves[n]);

        // 6-7: captures and promotion captures to the left
        left = (( pawns &  ~FILE_A) << 7 ) & pos->occupied[!pos->side]; ;
        promotion_captures = left & RANK_8;
        captures = left & ~RANK_8;
        n += build_promotion_moves(promotion_captures, -7, KNIGHT_PROMOTION_CAPTURE, &moves[n], flag);
        n += build_pawn_moves(captures, -7, CAPTURE, &moves[n]);

        //8: En passant
        if(pos->en_passant != -1)
        {
            uint64_t ep = PawnAttacks[BLACK][pos->en_passant] & pawns;
            while(ep)
            {
                moves[n++] = create_move(poplsb(&ep), pos->en_passant, EN_PASSANT);
            }
        }
    }
    return n;
}
int generate_moves(Position* pos, uint16_t* moves, int flag)
{
    int n=0;
    for(int i= KNIGHT; i<=KING; i++)
        n += generate_non_pawn_moves(pos, i, &moves[n] , flag);

    n += generate_pawn_moves(pos, &moves[n], flag);

    if(flag == ALL_MOVES)
        n += generate_castles(pos, &moves[n]);
    return n;
}
