#ifndef _NETWORK_H_
#define _NETWORK_H_
#include <stdalign.h>
#include "movegen.h"
#include "incbin/incbin.h"

#define INPUT_SIZE 768
#define L1 512
#define OUTPUT 1

#define SCALE 275 
#define SCALE_WEIGHT 512 
#define WEIGHT_FILE "devre.nnue"
INCBIN(EmbeddedWeights, WEIGHT_FILE);

int weight_indices[2][32];
int active_neurons[2][32];
int sz;
int8_t nn_indices[2][15] = {{ 0, 0 , 1 ,2 ,3 ,4 ,11 ,0 , 0 , 5 ,6, 7, 8 ,9 ,10 },{ 0, 5 ,6 ,7, 8 ,9, 10 , 0, 0 , 0, 1 ,2 ,3,4 , 11 }};

 alignas(64) int16_t  feature_weights[INPUT_SIZE*L1];
 alignas(64) int16_t  feature_biases  [L1];
 alignas(64) int16_t  layer1_weights  [2*L1];
 alignas(64) int32_t  layer1_bias;

 alignas(64) int16_t  layer_1[2*L1];
 alignas(64) float  output_layer[OUTPUT];

 #if defined(USE_AVX2)
    #include <immintrin.h>
    #define vector           __m256i
    #define vector_add       _mm256_add_epi16
    #define vector_sub       _mm256_sub_epi16
    #define vector_max       _mm256_max_epi16
    #define vector_set_zero  _mm256_setzero_si256
    #define vector_multipy   _mm256_madd_epi16
    #define vector_epi32_add _mm256_add_epi32
    #define vector_size      16
#elif defined(USE_SSE3)
    #include <tmmintrin.h>
    #define vector           __m128i
    #define vector_add       _mm_add_epi16
    #define vector_sub       _mm_sub_epi16
    #define vector_max       _mm_max_epi16
    #define vector_set_zero  _mm_setzero_si128
    #define vector_multipy   _mm_madd_epi16
    #define vector_epi32_add _mm_add_epi32
    #define vector_size      8
#endif

alignas(64) int16_t accumulator[2*MAX_DEPTH][2][L1]; 
void quan_relu(int16_t x[] , int dim)
{
    vector zero = vector_set_zero(); 
    vector* input = (vector*) &x[0];
    for (int i = 0; i < dim/vector_size; i++)
    {
      input[i] =  vector_max (zero, input[i ]);
    }
}
int set_weights()
{
    int16_t* data_int16;
    int32_t* data_int32;
    float* data_float = (float* )gEmbeddedWeightsData;
    data_int16 = (int16_t*)( data_float);

    for(int j=0; j<INPUT_SIZE*L1; j++)
        feature_weights[j] = (*data_int16++);
    for(int j=0; j<L1; j++)
        feature_biases[j] = (*data_int16++);

    for(int j=0; j<2*L1; j++)
        layer1_weights[j] = (*data_int16++);

    data_int32 = (int32_t*)(data_int16);
    for(int j=0; j<1; j++)
        layer1_bias = (*data_int32++);

    return 1;
}

//Todo: write this with incremental update inside make_move. It is very slow in this way
void calculate_indices()
{
    uint8_t b_king = mailbox[ PieceList[BLACK_KING][0] ] ;
    uint8_t w_king = mailbox [ PieceList[KING][0]  ];
    uint8_t sq;
    sz=0;
    uint64_t temp = black_pawns;
    while(temp)
    {
        sq = bitScanForward(temp);
        temp =  (temp>>(sq+1)) << (sq +1);
        active_neurons[0][sz] =  (5*64)+ sq ;
        active_neurons[1][sz] =   Mirror(sq) ;
        sz++;
    }
    temp = white_pawns;
    while(temp)
    {
        sq = bitScanForward(temp);
        temp =  (temp>>(sq+1)) << (sq +1);
        active_neurons[0][sz] =   sq ;
        active_neurons[1][sz] = 5*64+  Mirror(sq) ;
        sz++;
    }
    for(int i=2 ; i<= 13 ; i++)
    {
        int k=0;
        if(i == 6)
        {
            i = 10;
        }
        while(PieceList[i][k])
        {
            sq = mailbox[ PieceList[i][k] ];
            int index_w = nn_indices[0][i];
            int index_b =  nn_indices[1][i];
            active_neurons[0][sz] =  index_w*64+ sq ;
            active_neurons[1][sz] =  index_b*64+  Mirror(sq) ;
            sz++;
            k++;
        }
    }
    active_neurons[0][sz] = 10*64+ b_king;
    active_neurons[1][sz++] = 10*64+ Mirror(w_king);
    active_neurons[0][sz] = 11*64+ w_king;
    active_neurons[1][sz++] = 11*64+ Mirror(b_king);
    b_king = Mirror(b_king);
    for(int k=0 ; k<sz ; k++)
    {
        weight_indices[0][k] = L1*active_neurons[0][k];
        weight_indices[1][k] = L1*active_neurons[1][k];
    }
}
void calculate_input_layer_incrementally(Position* pos,Stack* stack, uint16_t move)
{
    int activated_inputs[2]={-1,-1};
    int deactivated_inputs[2][2] = {{-1 , -1} , {-1, -1}};
    int move_type = move_type(move) , captured_piece = stack->array[stack->top].captured_piece , piece_type = pos->board[move_to(move)];
    uint8_t from = mailbox[move_from(move)];
    uint8_t to = mailbox[ move_to(move) ];
    switch(move_type)
    {
        case 4:
            deactivated_inputs[0][1] = nn_indices[0][captured_piece]*64 + to;
            deactivated_inputs[1][1] = nn_indices[1][captured_piece]*64 +  Mirror(to);
        case 0: case 1:
            activated_inputs[0] =  nn_indices[0][piece_type]*64 + to;
            activated_inputs[1] =  nn_indices[1][piece_type]*64 + Mirror(to);
            deactivated_inputs[0][0] = nn_indices[0][piece_type]*64 + from;
            deactivated_inputs[1][0] = nn_indices[1][piece_type]*64 + Mirror(from);
        break;
    }
    pos->accumulator_cursor[pos->ply ]= 1;
     memcpy(accumulator[pos->ply][0] , accumulator[pos->ply -1][0] , 2*L1);
     memcpy(accumulator[pos->ply][1] , accumulator[pos->ply -1][1] , 2*L1);
    vector *outputs1 = (vector *) &accumulator[pos->ply][0];   //white
    vector *outputs2 = (vector *) &accumulator[pos->ply][1];  //black
    vector *weights1 = (vector *) &feature_weights[L1*activated_inputs[0]];
    vector *weights2 = (vector *) &feature_weights[L1*activated_inputs[1]];
    for(int i=0; i< L1/vector_size; i++)
    {
        outputs1[ i] = vector_add(outputs1[ i] , weights1[ i]);
        outputs2[ i] = vector_add(outputs2[ i] , weights2[ i]);
    }

    for(int i=0 ; i<2 ; i++)
    {
        if(deactivated_inputs[0][i] != -1)
        {
            weights1 = (vector *) &feature_weights[L1*deactivated_inputs[0][i]];
            weights2 = (vector *) &feature_weights[L1*deactivated_inputs[1][i]];
            for(int i=0; i< L1/vector_size; i++)
            {
                outputs1[ i] = vector_sub(outputs1[ i] , weights1[ i]);
                outputs2[ i] = vector_sub(outputs2[ i] , weights2[ i]);
            }
        }
    }

    memcpy(layer_1 + pos->side_to_move*L1 , accumulator[pos->ply][0] , 2*L1);
    memcpy(layer_1 + (!pos->side_to_move)*L1 , accumulator[pos->ply][1] , 2*L1);
    quan_relu(layer_1 , 2*L1);//activation func.
}
void calculate_input_layer(Position* pos)
{
    vector *biases =   (vector *) &feature_biases[0];
    vector *outputs1 = (vector *) &layer_1[0];  //side_to_move
    vector *outputs2 = (vector *) &layer_1[L1]; //for enemy

    for(int i=0; i< L1/vector_size; i++)
    {
        outputs2[i] = biases[i];
        outputs1[i] = biases[i];
    }
    for(int k=0 ; k < sz; k++)
    {
        vector *weights1 = (vector *) &feature_weights [weight_indices[pos->side_to_move][k]];
        vector *weights2 = (vector *) &feature_weights[weight_indices[!pos->side_to_move][k]];
        for(int i=0; i< L1/vector_size; i++)
        {
            outputs1[ i] = vector_add(outputs1[ i] , weights1[ i]);
            outputs2[ i] = vector_add(outputs2[ i] , weights2[ i]);
        }
    }
    pos->accumulator_cursor[pos->ply]= 1;
    memcpy( accumulator[pos->ply][0] , layer_1 + pos->side_to_move*L1 ,     2*L1);
    memcpy( accumulator[pos->ply][1] , layer_1 + ( 1-pos->side_to_move)*L1 , 2*L1); 
    quan_relu(layer_1 , 2*L1);//activation func.
}
int32_t quan_matrix_multp(int16_t weight_matrix[] ,int32_t bias, int16_t input[], int input_dim)
{
    vector* input_vector=(vector *)&input[0];
    vector* weights = (vector *)&weight_matrix[0];
    alignas(64) int32_t result[4];
    vector out0= vector_multipy(input_vector[0 ] , weights[0 ]);
    vector out1= vector_multipy(input_vector[1 ] , weights[1 ]);
    vector out2= vector_multipy(input_vector[2 ] , weights[2 ]);
    vector out3= vector_multipy(input_vector[3 ] , weights[3 ]);
    vector out4= vector_multipy(input_vector[4 ] , weights[4 ]);
    vector out5= vector_multipy(input_vector[5 ] , weights[5 ]);
    vector out6= vector_multipy(input_vector[6 ] , weights[6 ]);
    vector out7= vector_multipy(input_vector[7 ] , weights[7 ]);
    for(int i=8; i<input_dim/(vector_size) ; i+=8)
    {
        out0= vector_epi32_add(out0 , vector_multipy(input_vector[i + 0 ] , weights[i + 0 ]));
        out1= vector_epi32_add(out1 , vector_multipy(input_vector[i + 1 ] , weights[i + 1 ]));
        out2= vector_epi32_add(out2 , vector_multipy(input_vector[i + 2 ] , weights[i + 2 ]));
        out3= vector_epi32_add(out3 , vector_multipy(input_vector[i + 3 ] , weights[i + 3 ]));
        out4= vector_epi32_add(out4 , vector_multipy(input_vector[i + 4 ] , weights[i + 4 ]));
        out5= vector_epi32_add(out5 , vector_multipy(input_vector[i + 5 ] , weights[i + 5 ]));
        out6= vector_epi32_add(out6 , vector_multipy(input_vector[i + 6 ] , weights[i + 6 ]));
        out7= vector_epi32_add(out7 , vector_multipy(input_vector[i + 7 ] , weights[i + 7 ]));
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
    return (bias + result[0] + result[1] +result[2] +result[3])* ((SCALE) / (SCALE_WEIGHT*SCALE_WEIGHT*1.0)); 
}
int16_t evaluate_network(Position* pos ,Stack* stack  , uint16_t move)
{
    if(pos->half_move >= 100)
        return 0;
    if(black_pawns == 0 && white_pawns == 0 
     && PieceList[QUEEN][0] == 0 && PieceList[BLACK_QUEEN][0] == 0
     && PieceList[ROOK][0] == 0 && PieceList[BLACK_ROOK][0] == 0
     && PieceList[BLACK_BISHOP][1] == 0 && PieceList[BISHOP][1] == 0 
     && PieceList[BLACK_KNIGHT][1] == 0 && PieceList[KNIGHT][1] == 0  )//insufficent material detection 
    {       
            int k=0;
            k += (PieceList[BISHOP][0] > 0 );
            k += (PieceList[BLACK_BISHOP][0] > 0);
            k += (PieceList[KNIGHT][0] > 0);
            k += (PieceList[BLACK_KNIGHT][0] > 0);
            if(k<= 1)
            {
                return 0;
            }
    }
    if(pos->ply && pos->accumulator_cursor[pos->ply-1 ] == 1 && move != 0 && move != NULL_MOVE && ( move_type(move) < 2 || move_type(move) == CAPTURE ) )
    {
        calculate_input_layer_incrementally(pos ,stack , move);
    }
    else
    {
        calculate_indices();
        calculate_input_layer(pos);
    }
    int eval = quan_matrix_multp(layer1_weights ,layer1_bias , layer_1, 2*L1);
    return eval * ((100.0 -pos->half_move)/100.0);
}
#endif
