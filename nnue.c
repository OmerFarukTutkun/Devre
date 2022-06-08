#include "nnue.h"
#include "board.h"
int weight_indices[2][32];
int sz;
int8_t nn_indices[2][12] = {{0 , 1 ,2 ,3 ,4 ,5  ,6, 7, 8 ,9 ,10, 11 },{ 6 ,7, 8 ,9, 10 , 11, 0, 1 ,2 ,3,4 , 5 }};

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

void calculate_indices(Position* pos)
{
    sz= 0;
    for(int k=0 ; k<64 ; k++)
    {
        if(pos->board[k] != EMPTY)
        {        
            weight_indices[WHITE][sz]   = L1*(nn_indices[WHITE][pos->board[k]]*64 + k);
            weight_indices[BLACK][sz++] = L1*(nn_indices[BLACK][pos->board[k]]*64 + mirror_vertically( k) );
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
    int activated_inputs[2][2]   = {{-1 , -1} , {-1, -1}};
    int deactivated_inputs[2][2] = {{-1 , -1} , {-1, -1}};
    uint8_t from = move_from(move);
    uint8_t to  = move_to(move);
    int move_type = move_type(move) , captured_piece = pos->unmakeStack[pos->ply -1].captured_piece , piece= pos->board[to];

    activated_inputs[WHITE][0] =  nn_indices[WHITE][piece]*64 + to;
    activated_inputs[BLACK][0] =  nn_indices[BLACK][piece]*64 + mirror_vertically(to);
    if(is_promotion(move))
    {
        deactivated_inputs[WHITE][0] = nn_indices[WHITE][piece_index(!pos->side, PAWN)]*64 + from;
        deactivated_inputs[BLACK][0] = nn_indices[BLACK][piece_index(!pos->side, PAWN)]*64 + mirror_vertically(from);
    }
    else
    {
        deactivated_inputs[WHITE][0] = nn_indices[WHITE][piece]*64 + from;
        deactivated_inputs[BLACK][0] = nn_indices[BLACK][piece]*64 + mirror_vertically(from);
    }
    switch(move_type)
    {
        case CAPTURE: case KNIGHT_PROMOTION_CAPTURE: case BISHOP_PROMOTION_CAPTURE: case ROOK_PROMOTION_CAPTURE: case QUEEN_PROMOTION_CAPTURE:
            deactivated_inputs[WHITE][1] = nn_indices[WHITE][captured_piece]*64 + to;
            deactivated_inputs[BLACK][1] = nn_indices[BLACK][captured_piece]*64 +  mirror_vertically(to);
        break;
        case EN_PASSANT:
            deactivated_inputs[WHITE][1] = nn_indices[WHITE][captured_piece]*64 + square_index(rank_index(from), file_index(to));
            deactivated_inputs[BLACK][1] = nn_indices[BLACK][captured_piece]*64 + mirror_vertically( square_index(rank_index(from), file_index(to)) );
        break;
        case KING_CASTLE:
            activated_inputs[WHITE][1]  =  nn_indices[WHITE][piece_index(!pos->side, ROOK)]*64 + to - 1;
            activated_inputs[BLACK][1]  =  nn_indices[BLACK][piece_index(!pos->side, ROOK)]*64 + mirror_vertically(to - 1);
            deactivated_inputs[WHITE][1] = nn_indices[WHITE][piece_index(!pos->side, ROOK)]*64 + to + 1;
            deactivated_inputs[BLACK][1] = nn_indices[BLACK][piece_index(!pos->side, ROOK)]*64 + mirror_vertically(to + 1);
        break;
        case QUEEN_CASTLE:
            activated_inputs[WHITE][1]  =  nn_indices[WHITE][piece_index(!pos->side, ROOK)]*64 + to + 1;
            activated_inputs[BLACK][1]  =  nn_indices[BLACK][piece_index(!pos->side, ROOK)]*64 + mirror_vertically(to + 1);
            deactivated_inputs[WHITE][1] = nn_indices[WHITE][piece_index(!pos->side, ROOK)]*64 + to - 2;
            deactivated_inputs[BLACK][1] = nn_indices[BLACK][piece_index(!pos->side, ROOK)]*64 + mirror_vertically(to - 2);
        break;
    }
    pos->accumulator_cursor[pos->ply ]= 1;

    vector *white_acc = (vector *) &accumulator[pos->ply - 1][WHITE];   
    vector *black_acc = (vector *) &accumulator[pos->ply - 1][BLACK];
    vector *outputs1 = (vector *) &accumulator[pos->ply][WHITE];   
    vector *outputs2 = (vector *) &accumulator[pos->ply][BLACK];

    vector *weights1 = (vector *) &feature_weights[L1*activated_inputs[WHITE][0]];
    vector *weights2 = (vector *) &feature_weights[L1*activated_inputs[BLACK][0]];
    
    for(int i=0; i< L1/vector_size; i++)
    {
        outputs1[ i] = vector_add(white_acc[ i] , weights1[ i]);
        outputs2[ i] = vector_add(black_acc[ i] , weights2[ i]);
    }

    if(activated_inputs[0][1] != -1)
    {
        weights1 = (vector *) &feature_weights[L1*activated_inputs[WHITE][1]];
        weights2 = (vector *) &feature_weights[L1*activated_inputs[BLACK][1]];
        for(int i=0; i< L1/vector_size; i++)
        {
            outputs1[ i] = vector_add(outputs1[ i] , weights1[ i]);
            outputs2[ i] = vector_add(outputs2[ i] , weights2[ i]);
        }
    }
    for(int i=0 ; i<2 ; i++)
    {
        if(deactivated_inputs[0][i] != -1)
        {
            weights1 = (vector *) &feature_weights[L1*deactivated_inputs[WHITE][i]];
            weights2 = (vector *) &feature_weights[L1*deactivated_inputs[BLACK][i]];
            for(int i=0; i< L1/vector_size; i++)
            {
                outputs1[ i] = vector_sub(outputs1[ i] , weights1[ i]);
                outputs2[ i] = vector_sub(outputs2[ i] , weights2[ i]);
            }
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
    return (layer1_bias+ result[0] + result[1] +result[2] +result[3])* ((SCALE) / (SCALE_WEIGHT*SCALE_WEIGHT*1.0)); 
}
int16_t evaluate_nnue(Position* pos)
{
    if(pos->ply && pos->accumulator_cursor[pos->ply-1 ] == 1)
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