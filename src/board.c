#include "board.h"
void push( Position* pos , uint8_t capture)
{
    pos->unmakeStack[pos->ply].castlings      = pos->castlings;
    pos->unmakeStack[pos->ply].en_passant     = pos->en_passant;
    pos->unmakeStack[pos->ply].captured_piece = capture;
    pos->unmakeStack[pos->ply].half_move      = pos->half_move;
    pos->unmakeStack[pos->ply].key            = pos->key;
}
void add_piece(Position* pos, uint8_t piece_type, uint8_t sq) 
{
    pos->board[sq] = piece_type;
    set_bit(&pos->bitboards[piece_type]  , sq);
    set_bit(&pos->occupied[piece_type/6] , sq);
}
void remove_piece(Position* pos,  uint8_t piece_type, uint8_t sq) 
{
    pos->board[sq] = EMPTY;
    clear_bit(&pos->bitboards[piece_type]  , sq);
    clear_bit(&pos->occupied[piece_type/6] , sq);
}
void move_piece(Position* pos, uint8_t piece_type, uint8_t from , uint8_t to )
{
    add_piece(pos, piece_type, to);
    remove_piece(pos, piece_type, from);
}
void fen_to_board ( Position* pos , const char* fen)
{
    pos->castlings = 0;
    pos->en_passant = -1; 
    pos->full_move = 1;
    pos ->half_move = 0;
    pos->bestmove= 0;
    pos->ply = 0;
    pos->killer[0]= 0;
    pos->piece_count = 64;
    pos->pos_history.index = 0;
    memset(pos->accumulator_cursor , 0, 2*MAX_DEPTH);
    memset(pos->bitboards, 0, 12*sizeof(uint64_t));
    memset(pos->occupied, 0, 2*sizeof(uint64_t));
    memset(pos->board, EMPTY, 64*sizeof(uint8_t));
    memset(pos->move_history , 0, sizeof(uint64_t)*2*MAX_DEPTH);
    int k=0;
    //1: pieces
    for(int i = 7 ; i>=0 ; i--)
    {
        for( int j = 0 ; j <8 ; j++ )
        {
            switch(fen[k])
            {
                case 'p' : add_piece(pos , BLACK_PAWN  , square_index(i, j) ); break;
                case 'P' : add_piece(pos , PAWN        , square_index(i, j) ); break;
                case 'n' : add_piece(pos , BLACK_KNIGHT, square_index(i, j) ); break;
                case 'N' : add_piece(pos , KNIGHT      , square_index(i, j) ); break;
                case 'b' : add_piece(pos , BLACK_BISHOP, square_index(i, j) ); break;
                case 'B' : add_piece(pos , BISHOP      , square_index(i, j) ); break;
                case 'r' : add_piece(pos , BLACK_ROOK  , square_index(i, j) ); break;
                case 'R' : add_piece(pos , ROOK        , square_index(i, j) ); break;
                case 'q' : add_piece(pos , BLACK_QUEEN , square_index(i, j) ); break;
                case 'Q' : add_piece(pos , QUEEN       , square_index(i, j) ); break;
                case 'k' : add_piece(pos , BLACK_KING  , square_index(i, j) ); break;
                case 'K' : add_piece(pos , KING        , square_index(i, j) ); break;
                case '/' : j--; break; 
                case '1' : case '2' :case '3' :case '4' :case '5' :case '6' :case '7' : case '8':
                pos->piece_count -= fen[k] - '0';
                j += fen[k] -'1';                           
                break;
                default: printf("%s",fen); printf("Error: Fen isn't proper!!! "); 
                assert(0); break;
            }
            k++;
        }
    }
    k++;
    // 2: side to move
    pos->side = fen[k] == 'w' ? WHITE : BLACK;
    k +=2;
    while(fen[k] != ' ') // 3: castlings
    {
        switch (fen[k])
        {
            case 'K' : pos->castlings |= WHITE_SHORT_CASTLE; break;
            case 'Q' : pos->castlings |= WHITE_LONG_CASTLE;  break;
            case 'k' : pos->castlings |= BLACK_SHORT_CASTLE; break;
            case 'q' : pos->castlings |= BLACK_LONG_CASTLE;  break;       
            default:          break;
        }
        k++;
    }
    k++;
    if(  fen[k] != '-') //4: en passant square
    {
        pos->en_passant =  fen[k++] - 'a' ;
        pos->en_passant += 8*(fen[k] - '1');
    }
    k +=2;
    // 5-6: half move and full move
    char *ptr = strdup(fen) + k;
    pos->half_move = atoi(strtok_r(NULL, " ", &ptr));
    pos->full_move = atoi(strtok_r(NULL, " ", &ptr));
}
void board_to_fen(Position* pos, char* fen)
{
    int counter=0;
    int empty=0;

    //1: pieces
     for(int i = 7 ; i >=0 ; i--)
    {
        for( int j = 0 ; j < 8 ; j++)
        {
            if(pos->board[square_index(i, j) ] != EMPTY)
            {
                if(empty)
                    fen[counter++] = empty +'0';
                fen[counter++] = piece_notations[pos->board[square_index(i, j) ]];
                empty=0;
            }
            else
                empty++;
        }
        if(empty )
        {
            fen[counter++] = empty +'0';
            empty=0;
        }
        if(i > 0)
            fen[counter++] = '/';
    }
    fen[counter++] = ' ';

    //2: side to move
    if(pos->side == BLACK)
        fen[counter++] = 'b';
    else
        fen[counter++] = 'w';

    fen[counter++] = ' ';

    //3: castlings
    uint8_t white_castlings = pos->castlings & (WHITE_CASTLE);
    uint8_t black_castlings = pos->castlings & (BLACK_CASTLE);
    switch (white_castlings)
    {
        case 3 : fen[counter++] = 'K';   
        case 2 : fen[counter++] = 'Q'; break;
        case 1 : fen[counter++] = 'K'; break; 
    }
    switch (black_castlings )
    {
        case 12 : fen[counter++] = 'k';   
        case 8 : fen[counter++] =  'q'; break;
        case 4 : fen[counter++] =  'k'; break; 
    }
    if(pos->castlings == 0)
        fen[counter++] = '-';

    //4-5-6: en-passsant, halfmove , fullmove
    fen[counter++] = ' ';
    sprintf(fen + counter, "%s %d %d", pos->en_passant != -1 ? square_to_string[pos->en_passant] : "-", pos->half_move, pos->full_move);
}
void print_Board(Position* pos )
{
    char fen[100];
    printf("fen:  ");
    board_to_fen(pos,fen);
    printf("%s\n", fen);
    for(int i=7 ; i>=0 ;i--)
    {
        printf("\n  |----|----|----|----|----|----|----|----|\n");
        for(int j = 0; j < 8 ;j++)
        {
            if( pos->board[ square_index(i, j) ] != EMPTY)
                printf("%5c" , piece_notations[pos->board[ square_index(i, j) ]]);
            else
                printf("%5c" , ' ');
        }
    }
    printf("\n  |----|----|----|----|----|----|----|----|\n");
    printf("\n%5c%5c%5c%5c%5c%5c%5c%5c\n", 'a','b','c', 'd', 'e','f', 'g' , 'h');
}

long long int timeInMilliseconds(void) {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000) + (tv.tv_usec/1000);
}
bool has_non_pawn_pieces(Position* pos)
{
    return (pos->bitboards[KNIGHT] || pos->bitboards[BISHOP] || pos->bitboards[ROOK] || pos->bitboards[QUEEN] )
       &&  (pos->bitboards[BLACK_KNIGHT] || pos->bitboards[BLACK_BISHOP] || pos->bitboards[BLACK_ROOK] || pos->bitboards[BLACK_QUEEN] );
}
bool is_repetition(Position * pos)
{
    for(int i = pos->pos_history.index - 2 ; i>= 0; i--) 
    {
        if(pos->key == pos->pos_history.keys[i])
        {
            return TRUE;
        }
        if( pos->pos_history.index  - i > pos->half_move)
        {
            return FALSE;
        }
    }
    return FALSE;
}
bool is_material_draw(Position* pos)
{
    if(pos->bitboards[PAWN] || pos->bitboards[BLACK_PAWN] || pos->bitboards[ROOK] || pos->bitboards[BLACK_ROOK] || pos->bitboards[QUEEN] || pos->bitboards[BLACK_QUEEN])
        return FALSE;
        
    int bishop_counts[2] ={ popcount64(pos->bitboards[BISHOP]) ,  popcount64(pos->bitboards[BLACK_BISHOP]) }; 
    int knight_counts[2] ={ popcount64(pos->bitboards[KNIGHT]) ,  popcount64(pos->bitboards[BLACK_KNIGHT]) };

    int total = bishop_counts[0] + bishop_counts[1] + knight_counts[0] + knight_counts[1] ;
    if(total <= 1)
        return TRUE;
    if(total == 2 && (bishop_counts[0] != 2 && bishop_counts[1] != 2) )
        return TRUE; 
    return FALSE;
}
bool is_draw(Position* pos)
{
    if(pos->ply && ( pos->half_move >= 100 || is_repetition(pos) || is_material_draw(pos)) )
        return TRUE;
    return FALSE;
}