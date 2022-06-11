#include "move.h"
#include "tt.h"

const int move_type_scores[16] = {Mil, Mil, 3*Mil, 2*Mil, 10*Mil, 10*Mil, 0, 0,-10*Mil,-9*Mil,-8*Mil, 20*Mil,-7*Mil,-6*Mil,-5*Mil, 25*Mil};
const int see_values[12]   = { 100 , 325 , 325 , 500 ,1000 ,10000 , 100 , 325 , 325 , 500 ,1000 ,10000};
void print_move(uint16_t move)
{
    
    uint8_t from = move_from(move);
    uint8_t to = move_to(move);
    printf("%s%s" ,square_to_string[from] , square_to_string[to]);
    switch(move_type(move))
    {
        case KNIGHT_PROMOTION: case KNIGHT_PROMOTION_CAPTURE: printf("n "); break;
        case BISHOP_PROMOTION: case BISHOP_PROMOTION_CAPTURE: printf("b "); break;
        case ROOK_PROMOTION:   case ROOK_PROMOTION_CAPTURE:   printf("r "); break;
        case QUEEN_PROMOTION:  case QUEEN_PROMOTION_CAPTURE:  printf("q "); break;
        default: printf(" ");  break;
    }
}
uint16_t string_to_move(Position* pos ,char* str)
{
    MoveList move_list;
    move_list.num_of_moves = generate_moves(pos, move_list.moves, ALL_MOVES);
    uint8_t from = (str[0] -'a') + 8*(str[1] - '1');
    uint8_t to =   (str[2] -'a') + 8*(str[3] - '1');

    for (int i=0; i< move_list.num_of_moves  ; i++ )
    {
        if ( from == move_from(move_list.moves[i]) && to == move_to(move_list.moves[i]))
        {
            if(is_promotion(move_list.moves[i]))//promotion
            {
                switch(str[4])
                {
                    case 'n': case 'N': return move_list.moves[i];
                    case 'b': case 'B': return move_list.moves[i+1];
                    case 'r': case 'R': return move_list.moves[i+2];
                    case 'q': case 'Q': return move_list.moves[i+3];
                    default: break;
                }
            }
            return move_list.moves[i];
        }
    }
    return NULL_MOVE;
}
uint16_t pick_move(MoveList* move_list, int index)
{
    int max_index=0;
    for(int i=1 ; i < move_list->num_of_moves -index ; i++)
    {
        if(move_list->move_scores[i] > move_list->move_scores[max_index])
        {
            max_index = i;
        }
    }
    move_list->score_of_move = move_list->move_scores[max_index];
    uint16_t move = move_list->moves[max_index];

    move_list->move_scores[max_index] =  move_list->move_scores[move_list->num_of_moves - index -1];
    move_list->moves[max_index] =  move_list->moves[move_list->num_of_moves - index -1];
    
    return move;
}
void make_move(Position* pos, uint16_t move)
{
    if(move == NULL_MOVE)//do null move
    {
        push(pos, EMPTY);
        pos->ply++;
        pos->side = !pos->side;
        pos->key ^= SideToPlayKey;
        pos->en_passant = -1;
        pos->half_move = 0;
        pos->move_history[pos->ply] = NULL_MOVE;
        return ;
    }
    uint8_t from = move_from(move);
    uint8_t to   = move_to(move);
    uint8_t move_type = move_type(move);
    uint8_t piece = pos->board[from]; // moving piece
    uint8_t captured_piece = pos->board[to]; // Captured piece type 
    memset(&pos->nnue_changes, 0, sizeof(NNUE_Changes));
    if(move_type == EN_PASSANT)
    {
       captured_piece = piece_index(!pos->side , PAWN);
    }
    push( pos, captured_piece);

    pos->en_passant = -1;
    pos->half_move++; 
    pos->full_move += pos->side;
    pos->ply++;
    pos->move_history[pos->ply] = move;

    if(is_capture(move))
        pos->piece_count--;

    if(pos->castlings)
    {
        if(from == H1 || to == H1)
            pos->castlings &= 14 ; 
        else if( from == A1 || to == A1)
            pos->castlings &= 13; 
        else if(from == H8 || to == H8)
            pos->castlings &= 11; 
        else if( from == A8 || to == A8)
            pos->castlings &= 7; 
        else if( pos-> board[from] == KING)
            pos->castlings &= BLACK_CASTLE ; 
        else if(pos->board[from] == BLACK_KING)
            pos->castlings &= WHITE_CASTLE ; 
    }
    if(pos->unmakeStack[pos->ply -1].en_passant != -1) //remove the last en passant key 
    {
        pos->key ^= EnPassantKeys[ pos->unmakeStack[pos->ply -1].en_passant] ;
    }
    if(pos->unmakeStack[pos->ply -1].castlings != pos->castlings) //xor if there is a change in castling rights
    {
        pos->key ^= CastlingKeys[ pos->unmakeStack[pos->ply -1].castlings] ^ CastlingKeys[pos->castlings];
    }
    pos->key ^= SideToPlayKey;

    switch(move_type)
    {
        case QUIET:
            move_piece(pos, piece , from,to);
            pos->key ^= PieceKeys[piece][from] ^ PieceKeys[piece][to];
            break;
        case CAPTURE:
            remove_piece(pos, captured_piece , to);
            move_piece(pos, piece , from,to);
            pos->key ^= PieceKeys[piece][from] ^ PieceKeys[piece][to] ^ PieceKeys[captured_piece][to];
            break;
        case DOUBLE_PAWN_PUSH:
            move_piece(pos, piece , from,to);
            pos->key ^= PieceKeys[piece][from] ^ PieceKeys[piece][to];
            uint64_t ep= pos->bitboards[piece_index(!pos->side , PAWN)] & PawnAttacks[pos->side][(from + to)/2]; 
            if(ep)
                pos->en_passant = (from + to)/2;
            break;
        case KING_CASTLE:
            move_piece(pos, piece, from, to);
            move_piece(pos, piece_index(pos->side,ROOK), to +1, from + 1);
            pos->key ^= PieceKeys[piece][from] ^ PieceKeys[piece][to] ^ PieceKeys[piece_index(pos->side,ROOK)][to + 1 ] ^PieceKeys[piece_index(pos->side,ROOK)][from +1];
            break;
        case QUEEN_CASTLE:
            move_piece(pos, piece, from, to);
            move_piece(pos, piece_index(pos->side,ROOK), to - 2, from - 1);  
            pos->key ^= PieceKeys[piece][from] ^ PieceKeys[piece][to] ^ PieceKeys[piece_index(pos->side,ROOK)][to - 2 ] ^PieceKeys[piece_index(pos->side,ROOK)][from - 1];    
            break;
        case EN_PASSANT:
            remove_piece(pos, captured_piece, square_index(rank_index(from) , file_index(to))); 
            move_piece(pos, piece, from,to);
            pos->key ^= PieceKeys[piece][from] ^ PieceKeys[piece][to] ^ PieceKeys[captured_piece][square_index(rank_index(from) , file_index(to))];
            break;
        case KNIGHT_PROMOTION_CAPTURE: case BISHOP_PROMOTION_CAPTURE: case ROOK_PROMOTION_CAPTURE: case QUEEN_PROMOTION_CAPTURE:
            remove_piece(pos, captured_piece, to);
             pos->key ^= PieceKeys[captured_piece][to];
        case KNIGHT_PROMOTION: case BISHOP_PROMOTION: case ROOK_PROMOTION: case QUEEN_PROMOTION:
            remove_piece(pos, piece , from);
            add_piece(pos, piece_index(pos->side, KNIGHT + ( move_type & 3)), to);
              pos->key  ^= PieceKeys[piece][from] ^ PieceKeys[ piece_index(pos->side, KNIGHT + ( move_type & 3)) ][to];
        break;
    }  
    pos->side = !pos->side;
    if(is_capture(move) || piece_type(piece) == PAWN )
    {
        pos->half_move = 0;
    }
    pos->pos_history.keys[pos->pos_history.index++] = pos->key;
}
void unmake_move(Position* pos,  uint16_t move)
{
    uint16_t from = move_from(move);
    uint16_t to   = move_to(move);
    uint16_t move_type = move_type(move);
    
    //  Unmake info
    UnMake_Info unmake_info = pos->unmakeStack[pos->ply -1];
    pos->castlings          = unmake_info.castlings;
    pos->en_passant         = unmake_info.en_passant;
    pos->half_move          = unmake_info.half_move;
    pos-> side              = !pos->side;
    pos-> key               = unmake_info.key;  
    pos->accumulator_cursor[pos->ply ] = 0;
    pos->ply--;
    if(move == NULL_MOVE)
    {
        return ;
    }    
    if(is_capture(move))
        pos->piece_count++;

    uint8_t captured_piece = unmake_info.captured_piece;  
    pos->pos_history.index--;

    uint8_t piece= pos->board[to];
    if(pos->side == BLACK)
        pos->full_move--;
    switch(move_type)
    {
        case QUIET: case DOUBLE_PAWN_PUSH:
            move_piece(pos, piece , to ,from);
            break;
        case CAPTURE:
            move_piece(pos, piece , to ,from);
            add_piece(pos, captured_piece , to);
            break;
        case KING_CASTLE:
            move_piece(pos, piece, to, from);
            move_piece(pos, piece_index(pos->side,ROOK), from + 1 , to +1);
            break;
        case QUEEN_CASTLE:
            move_piece(pos, piece, to, from);
            move_piece(pos, piece_index(pos->side,ROOK), from - 1 , to - 2);;      
            break;
        case EN_PASSANT:
            move_piece(pos, piece, to , from);
            add_piece(pos, captured_piece, square_index(rank_index(from) , file_index(to))); 
            break;
        case KNIGHT_PROMOTION_CAPTURE: case BISHOP_PROMOTION_CAPTURE: case ROOK_PROMOTION_CAPTURE: case QUEEN_PROMOTION_CAPTURE:
            remove_piece(pos, piece, to);
            add_piece(pos, piece_index(pos->side ,PAWN) , from);
            add_piece(pos, captured_piece, to);
            break;
        case KNIGHT_PROMOTION: case BISHOP_PROMOTION: case ROOK_PROMOTION: case QUEEN_PROMOTION:
            remove_piece(pos, piece, to);
            add_piece(pos, piece_index(pos->side ,PAWN) , from);
            break;
    }
}
int calculate_SEE(Position* pos, uint16_t move)
{
  // Do not calculate SEE for castlings, ep, promotions
  if (move_type(move) >= KING_CASTLE && move_type(move) != CAPTURE){
       return 1000;
    }
  int gain[32] = {0};
  int d = 0;
  int from  = move_from(move);
  int to    =  move_to(move);
  int side  = pos->side;
  
  uint64_t attackers = square_attacked_by(pos , to);
  uint64_t diagonalX  = pos->bitboards[BISHOP]  |  pos->bitboards[BLACK_BISHOP] |  pos->bitboards[QUEEN] | pos->bitboards[BLACK_QUEEN];
  uint64_t horizontalX= pos->bitboards[ROOK]    |  pos->bitboards[BLACK_ROOK]   |  pos->bitboards[QUEEN] | pos->bitboards[BLACK_QUEEN];
  uint64_t occ = pos->occupied[0] | pos->occupied[1];
  uint8_t piece = piece_type(pos->board[to]);
  gain[0] = is_capture(move) ? see_values[piece] : 0;
  do
  {
    d++;
    piece = piece_type(pos->board[from]);
    gain[d]  = see_values[piece] - gain[d-1];
    clear_bit(&occ, from);
    clear_bit(&attackers, from);
    if ( max(-gain[d-1], gain[d]) < 0){
        break;
    }
    if (piece == PAWN || piece == BISHOP || piece == QUEEN) 
            attackers |= bishop_attacks(occ, to) & diagonalX   & occ;
    if (piece == ROOK || piece == QUEEN) 
            attackers |= rook_attacks(occ, to)   & horizontalX & occ; 
    side = !side;
    from = getLeastValuablePiece(pos, attackers , side);
  } while (from != NO_SQ);

  while (--d)
    gain[d-1] = -max(-gain[d-1], gain[d]);

  return gain[0];
}
void score_moves(Position* pos, MoveList* move_list, uint16_t hash_move, int flag)
{
    uint16_t move;
    int* scores = move_list->move_scores;
    uint16_t killer = pos->killer[pos->ply];
    uint16_t counter= pos->counter_moves[pos->side][move_from(pos->move_history[pos->ply])][move_to(pos->move_history[pos->ply])];
    for(int i=0; i < move_list->num_of_moves ; i++)
    {
        move = move_list->moves[i];
        if(move == hash_move)
        {
            scores[i] = 30*Mil;
        }
        else
        {
            scores[i] = move_type_scores[move_type(move)];
            if(killer == move)
            {
                scores[i] = 9*Mil;
            }
            else if(counter == move)
            {
                scores[i] = 8*Mil;
            }
            else if(move_type(move)< 2) //history heuristic
            {
                scores[i] += pos->history[pos->side][move_from(move)][move_to(move)];
            }
            else if(move_type(move) == CAPTURE )
            {
                uint8_t from = move_from(move);
                uint8_t to   = move_to(move);
                uint8_t piece = pos->board[from];
                uint8_t captured_piece= pos->board[to];
                if(flag != QSEARCH && calculate_SEE(pos, move) < 0 )
                    scores[i]  =  Mil + piece_values[captured_piece]*100 - piece_values[piece];
                else
                    scores[i]  += piece_values[captured_piece]*100 - piece_values[piece];
            }
        }
    }
}