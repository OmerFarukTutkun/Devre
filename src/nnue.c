#include "nnue.h"
#include "board.h"
int king_indices[64] = {
    0, 1, 2, 3, 3, 2, 1, 0,
    4, 5, 6, 7, 7, 6, 5, 4,
    8, 9, 10, 11, 11, 10, 9, 8,
    12, 13, 14, 15, 15, 14, 13, 12,
    16, 17, 18, 19, 19, 18, 17, 16,
    20, 21, 22, 23, 23, 22, 21, 20,
    24, 25, 26, 27, 27, 26, 25, 24,
    28, 29, 30, 31, 31, 30, 29, 28};

int weight_indices[2][32];
int sz;
const int8_t nn_indices[2][12] = {{0 , 1 ,2 ,3 ,4 ,5  ,6, 7, 8 ,9 ,10, 11 },{ 6 ,7, 8 ,9, 10 , 11, 0, 1 ,2 ,3,4 , 5 }};

 alignas(64) int16_t  feature_weights[INPUT_SIZE*L1];
 alignas(64) int16_t  feature_biases  [L1];
 alignas(64) int16_t  layer1_weights  [2*L1];
 alignas(64) int32_t  layer1_bias;

 alignas(64) int16_t accumulator[2*MAX_DEPTH][2][L1]; 

INCBIN(EmbeddedWeights, WEIGHT_FILE);
void set_weights()
{
    int16_t* data_int16;
    data_int16 = (int16_t*)( gEmbeddedWeightsData);

    for(int j=0; j<INPUT_SIZE*L1; j++)
        feature_weights[j] = (*data_int16++);
    for(int j=0; j<L1; j++)
        feature_biases[j] = (*data_int16++);

    for(int j=0; j<2*L1; j++)
        layer1_weights[j] = (*data_int16++);
    
    layer1_bias = *(int32_t*)(data_int16);
}

int nn_index(int king, int piece, int sq, int side)
{
    if (side == BLACK)
    {
        sq = mirror_vertically(sq);
        king = mirror_vertically(king);
    }
    if (king % 8 < 4)
        return king_indices[king] * 768 + nn_indices[side][piece] * 64 + mirror_horizontally(sq);
    else
        return king_indices[king] * 768 + nn_indices[side][piece] * 64 + sq;
}

void calculate_indices(Position* pos)
{
    int wking =bitScanForward( pos->bitboards[KING] );
    int bking =bitScanForward( pos->bitboards[BLACK_KING] );
    sz= 0;
    for(int k=0 ; k<64 ; k++)
    {
        if(pos->board[k] != EMPTY)
        {        
            weight_indices[WHITE][sz]   = L1*nn_index(wking, pos->board[k], k , WHITE);
            weight_indices[BLACK][sz++] = L1*nn_index(bking, pos->board[k], k , BLACK);
        }
    }
}

void calculate_input_layer_incrementally(Position* pos)
{
    uint16_t move = pos->move_history[pos->ply];
    if(move == NULL_MOVE)
    {
        pos->accumulator_cursor[pos->ply ]= 1;
	    memcpy(accumulator[pos->ply][WHITE] , accumulator[pos->ply -1][WHITE] , 2*L1);
    	memcpy(accumulator[pos->ply][BLACK] , accumulator[pos->ply -1][BLACK] , 2*L1);
	    return;
    }
    int wking =bitScanForward( pos->bitboards[KING] );
    int bking =bitScanForward( pos->bitboards[BLACK_KING] );

    int nnue_changes[2][3];
    int k=0;
    uint8_t from = move_from(move);
    uint8_t to  = move_to(move);
    int move_type = move_type(move) , captured_piece = pos->unmakeStack[pos->ply -1].captured_piece , piece= pos->board[to];

    nnue_changes[WHITE][k]   =  nn_index(wking, piece , to , WHITE);
    nnue_changes[BLACK][k++] =  nn_index(bking, piece , to , BLACK);
    if(is_promotion(move))
    {
        nnue_changes[WHITE][k]   =  nn_index(wking, piece_index(!pos->side, PAWN) , from , WHITE);
        nnue_changes[BLACK][k++] =  nn_index(bking, piece_index(!pos->side, PAWN) , from , BLACK);
    }
    else
    {
        nnue_changes[WHITE][k]    = nn_index(wking, piece , from , WHITE);
        nnue_changes[BLACK][k++]  = nn_index(bking, piece , from , BLACK);
    }
    if(is_capture(move))
    {
        int sq ;
        if( move_type != EN_PASSANT)
            sq = to;
        else
            sq = square_index(rank_index(from), file_index(to));
        nnue_changes[WHITE][k]    = nn_index(wking, captured_piece , sq , WHITE);
        nnue_changes[BLACK][k++]  = nn_index(bking, captured_piece , sq , BLACK);
    }
    pos->accumulator_cursor[pos->ply ]= 1;

    vector *white_acc = (vector *) &accumulator[pos->ply - 1][WHITE];   
    vector *black_acc = (vector *) &accumulator[pos->ply - 1][BLACK];
    vector *outputs1 = (vector *) &accumulator[pos->ply][WHITE];   
    vector *outputs2 = (vector *) &accumulator[pos->ply][BLACK];

    vector *weights1 = (vector *) &feature_weights[L1*nnue_changes[WHITE][0]];
    vector *weights2 = (vector *) &feature_weights[L1*nnue_changes[BLACK][0]];
    
    for(int i=0; i< L1/vector_size; i++)
    {
        outputs1[ i] = vector_add(white_acc[ i] , weights1[ i]);
        outputs2[ i] = vector_add(black_acc[ i] , weights2[ i]);
    }

    for(int j=1 ; j<k ; j++)
    {
        weights1 = (vector *) &feature_weights[L1*nnue_changes[WHITE][j]];
        weights2 = (vector *) &feature_weights[L1*nnue_changes[BLACK][j]];
        for(int i=0; i< L1/vector_size; i++)
        {
            outputs1[ i] = vector_sub(outputs1[ i] , weights1[ i]);
            outputs2[ i] = vector_sub(outputs2[ i] , weights2[ i]);
        }
    }
}
void calculate_input_layer(Position* pos)
{
    vector *biases =   (vector *) &feature_biases[0];
    vector *outputs1 = (vector *) &accumulator[pos->ply][WHITE][0];  //white
    vector *outputs2 = (vector *) &accumulator[pos->ply][BLACK][0];  //black
    for(int i=0; i< L1/vector_size; i++)
    {
        outputs2[i] = biases[i];
        outputs1[i] = biases[i];
    }
    for(int k=0 ; k < sz; k++)
    {
        vector *weights1 = (vector *) &feature_weights [weight_indices[WHITE][k]];
        vector *weights2 = (vector *) &feature_weights[weight_indices[BLACK][k]];
        for(int i=0; i< L1/vector_size; i++)
        {
            outputs1[ i] = vector_add(outputs1[ i] , weights1[ i]);
            outputs2[ i] = vector_add(outputs2[ i] , weights2[ i]);
        }
    }
    pos->accumulator_cursor[pos->ply]= 1;
}
int32_t quan_matrix_multp(int ply, int side)
{
    vector* acc_us=   (vector *)&accumulator[ply][side][0];
    vector* acc_enemy=(vector *)&accumulator[ply][!side][0];
    vector* weights = (vector *)&layer1_weights[0];
    vector zero = vector_set_zero(); 
    alignas(64) int32_t result[4];
    vector* acc = acc_us;

    vector out0= vector_multipy(  vector_max(acc[0], zero) , weights[0 ]);
    vector out1= vector_multipy(  vector_max(acc[1], zero) , weights[1 ]);
    vector out2= vector_multipy(  vector_max(acc[2], zero) , weights[2 ]);
    vector out3= vector_multipy(  vector_max(acc[3], zero) , weights[3 ]);
    vector out4= vector_multipy(  vector_max(acc[4], zero) , weights[4 ]);
    vector out5= vector_multipy(  vector_max(acc[5], zero) , weights[5 ]);
    vector out6= vector_multipy(  vector_max(acc[6], zero) , weights[6 ]);
    vector out7= vector_multipy(  vector_max(acc[7], zero) , weights[7 ]);
    int j=8;
    for(int i=8; i< 2*L1/(vector_size) ; i+=8)
    {
        if(i*vector_size == L1)
        {
            j=0;
            acc = acc_enemy;
        }
        out0= vector_epi32_add(out0 , vector_multipy( vector_max( acc[j + 0 ] , zero) , weights[i + 0 ]));
        out1= vector_epi32_add(out1 , vector_multipy( vector_max( acc[j + 1 ] , zero) , weights[i + 1 ]));
        out2= vector_epi32_add(out2 , vector_multipy( vector_max( acc[j + 2 ] , zero) , weights[i + 2 ]));
        out3= vector_epi32_add(out3 , vector_multipy( vector_max( acc[j + 3 ] , zero) , weights[i + 3 ]));
        out4= vector_epi32_add(out4 , vector_multipy( vector_max( acc[j + 4 ] , zero) , weights[i + 4 ]));
        out5= vector_epi32_add(out5 , vector_multipy( vector_max( acc[j + 5 ] , zero) , weights[i + 5 ]));
        out6= vector_epi32_add(out6 , vector_multipy( vector_max( acc[j + 6 ] , zero) , weights[i + 6 ]));
        out7= vector_epi32_add(out7 , vector_multipy( vector_max( acc[j + 7 ] , zero) , weights[i + 7 ]));
        j +=8;
    }
    vector out01 = vector_epi32_add(out0 ,out1);
    vector out23 = vector_epi32_add(out2 ,out3);
    vector out45 = vector_epi32_add(out4, out5);
    vector out67 = vector_epi32_add(out6 , out7);
    vector out0123=vector_epi32_add(out01, out23);
    vector out4567=vector_epi32_add(out45 , out67);
    vector sum=    vector_epi32_add(out0123, out4567);
    #if defined(USE_AVX2)
        __m128i high_sum = _mm256_extractf128_si256(sum , 1);
        __m128i low_sum = _mm256_castsi256_si128(sum);
        *(__m128i*) &result[0] = _mm_add_epi32(high_sum , low_sum);
    #elif defined(USE_SSE3)
        *(__m128i*) &result[0] = sum;
    #endif
    return (layer1_bias+ result[0] + result[1] +result[2] +result[3]) / OUTPUT_DIVISOR; 
}
int16_t evaluate_nnue(Position* pos)
{
    int piece = piece(pos->move_history[pos->ply]);
    if(pos->ply && piece_type(piece) != KING &&  pos->accumulator_cursor[pos->ply-1 ] == 1)
    {
        calculate_input_layer_incrementally(pos );
    }
    else
    {
        calculate_indices(pos);
        calculate_input_layer(pos);
    }
    int eval = quan_matrix_multp(pos->ply, pos->side);
    return eval * ((100.0 - pos->half_move)/100.0);
}