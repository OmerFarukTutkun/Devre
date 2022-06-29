#include "tt.h"

uint64_t PieceKeys[12][64];
uint64_t SideToPlayKey;
uint64_t CastlingKeys[16];
uint64_t EnPassantKeys[64];

#define MB 1024*1024
uint64_t  TT_MASK;

TTentry* TranspositionTable;
uint8_t age;

void tt_init(int megabyte)
{
    megabyte = MIN(4096 , MAX(1, megabyte));
    int size = (MB*megabyte )/sizeof(TTentry);
    int keySize = log2( size );
    TT_MASK = (ONE << keySize) - ONE;
    tt_free();
    TranspositionTable =(TTentry*) malloc( (ONE << keySize )*sizeof(TTentry));	
}
void tt_clear()
{
    age = 0;
    memset(TranspositionTable , 0, (TT_MASK + ONE)*sizeof(TTentry));
}
void tt_free()
{
    free(TranspositionTable);
}
uint64_t rand_u64()
{
    static uint64_t seed= 1442695040888963407; 
    const uint64_t a = 6364136223846793005; 
    seed *= a;
    return seed;
}
void init_keys()
{
    for(int i=0 ; i<12 ; i++)
    {
        for(int j=0 ; j<64;j++)
        {
            PieceKeys[i][j] = rand_u64();
        }
    }
    SideToPlayKey = rand_u64();
    for(int i = 0; i<16 ;i++)
        CastlingKeys[i] = rand_u64();

    for(int i = 0; i<64 ;i++)
        EnPassantKeys[i] = rand_u64();
}
void initZobristKey(Position* pos)
{
    pos->key = 0ull;
    for(int i=0 ; i< 64; i++ )
    {
        if(pos->board[i] != EMPTY)
        {
            pos->key ^= PieceKeys[ pos->board[i] ][i];
        }
    }
    if(pos ->side == BLACK)
       pos->key ^= SideToPlayKey;
    if(pos-> en_passant != -1)
       pos->key ^= EnPassantKeys[pos-> en_passant];

    pos->key ^= CastlingKeys[ pos->castlings];
}
void tt_save(Position* pos , int score ,char flag,uint8_t depth,uint16_t move)
{
    TTentry* entry = &TranspositionTable[pos->key & TT_MASK];
    if( entry->key == 0ull  
    ||   entry->depth - 2*(age - entry->age)  + (flag != TT_EXACT && entry->flag  == TT_EXACT) <= depth
    || (flag == TT_EXACT && entry->age < age))
    {
        if( score >  MATE - MAX_DEPTH)
            score += pos->ply;
        else if( -score > MATE - MAX_DEPTH)
            score -= pos->ply;

        entry->key   = pos->key;
        entry->score = score;
        entry->depth = depth;
        entry->flag  = flag;
        entry->move  = move;
        entry->age   = age;
    }
}
TTentry* tt_probe(uint64_t key)
{
    TTentry* entry =  &TranspositionTable[key & TT_MASK];
    if(entry->key == key)
        return entry;
    else
        return NULL;
}
void update_age()
{
    age +=1;
}
