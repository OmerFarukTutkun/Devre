#ifndef _NETWORK_H_
#define _NETWORK_H_
#include <stdalign.h>
#include "board.h"
#define INPUT 32*704
#define L1 128
#define L2 8
#define L3 32
#define OUTPUT 1

#define SCALE 260 // SCALING FACTOR to turn centipawn to win probability,    wdl = sigmoid(cp/SCALE)
// sigmoid(output[0]) = sigmoid(cp/SCALE) -> cp = output[0] * SCALE at the end of neural network
#define SCALE_WEIGHT 8192 // SCALE weights for float32 -> int16 quantization
#define WEIGHT_FILE "network_128x2_8.10.2021.bin"

int weight_indices[2][32];
int active_neurons[2][32];
int sz;
int game_phase;

int8_t nn_indices[2][15] = {{ 0, 0 , 1 ,2 ,3 ,4 ,-1 ,0 , 0 , 5 ,6, 7, 8 ,9 ,10 },{ 0, 5 ,6 ,7, 8 ,9, 10 ,0 , 0 , 0, 1 ,2 ,3,4 , -1 }};
int piece_values[15]={ 0, 10 , 32,35 ,53 ,98 ,100 ,0 , 0, 10 , 32,35 ,53 ,98 ,100 };

 alignas(64) int16_t  feature_weights[2] [INPUT*L1];
 alignas(64) int16_t  feature_biases [2] [L1];
 alignas(64) int16_t  layer1_weights  [2][2*L1*L2];
 alignas(64) int32_t  layer1_biases [2][L2];
 
 alignas(64) float  layer2_weights[2][L2*L3];
 alignas(64) float  layer2_biases[2][L3];
 alignas(64) float  layer3_weights[2][1*L3];
 alignas(64) float  layer3_biases[2][1];

 alignas(64) int16_t  layer_1[2*L1];
 alignas(64) float  layer_2[L2];
 alignas(64) float  layer_3[L3];
 alignas(64) float  output[OUTPUT];


int Vertical_Mirror(int sq)
{ 
    return((sq/8)*8 + 7-sq%8);
}
void Vertical_mirror_all_pieces(int side)
{
    for(int i=0; i<sz ; i++)
    {
        active_neurons[side][i]  = Vertical_Mirror( active_neurons[side][i] );
    }
}
int handle_king(uint8_t* king )  // does mirroring for all pieces is needed or not
{
    if(king[0]%8 < 4)
    {
        king[0] =(king[0]/8)*4 + king[0]%8 ;
        return 0;
    }
    else    //mirroring is needed since king is on a-d files
    {
        king[0] =(king[0]/8)*4 + 7 - king[0]%8 ;
        return 1;
    }    
}
void quan_clipped_relu(int16_t x[] , int dim)
{
    __m256i zero = _mm256_setzero_si256(); 
    __m256i max =  _mm256_set1_epi16(SCALE_WEIGHT);
    __m256i* input = (__m256i*) &x[0];
    for (int i = 0; i < dim/16; i+=4)
    {
      input[i+0] =  _mm256_max_epi16(zero, input[i + 0]);
      input[i+0] =  _mm256_min_epi16(max, input[i + 0]);

      input[i+1] =  _mm256_max_epi16(zero, input[i + 1]);
      input[i+1] =  _mm256_min_epi16(max, input[i + 1]);

      input[i+2] =  _mm256_max_epi16(zero, input[i + 2]);
      input[i+2] =  _mm256_min_epi16(max, input[i + 2]);

      input[i+3] =  _mm256_max_epi16(zero, input[i + 3]);
      input[i+3] =  _mm256_min_epi16(max, input[i + 3]);
    }
}
void clipped_relu(float x[] , int dim)
{
    __m256 zero = _mm256_setzero_ps(); 
    __m256 max = _mm256_set1_ps(1.0);
    __m256* input = (__m256*) &x[0];
    for (int i = 0; i < dim/8; i+=2)
    {
      input[i+0] =  _mm256_max_ps(zero, input[i + 0]);
      input[i+0] =  _mm256_min_ps(max, input[i + 0]);

      input[i+1] =  _mm256_max_ps(zero, input[i + 1]);
      input[i+1] =  _mm256_min_ps(max, input[i + 1]);
    }
}
int transpose_weights(int16_t* weights, int row, int column)
{
    int16_t* matrix=(int16_t*) malloc(sizeof(int16_t)* row*column);
    for(int i=0; i<column ; i++)
    {
        for(int j=0; j<row ; j++)
        {
            matrix[i*row + j]= weights[j*column + i];
        }
    }
    memcpy(weights , matrix, sizeof(int16_t)*row*column);
    free(matrix);
}
int set_weights()
{
    //Todo: Embed the weights in to the exe
    FILE* file = fopen(WEIGHT_FILE , "rb");
    if(file == NULL)
    {
        printf("Network file could not open\n");
    }
    for(int i=0 ; i<2 ; i++)
    {
        fread(feature_weights[i] , sizeof(int16_t) , INPUT*L1 ,file);
        fread(feature_biases[i] , sizeof(int16_t) , L1 ,file); 
        fread(layer1_weights[i], sizeof (int16_t) , 2*L1*L2 ,file); 
        fread(layer1_biases[i] , sizeof(int32_t) , L2 ,file); 
        fread(layer2_weights[i] , sizeof(float) , L2*L3 ,file); 
        fread(layer2_biases[i] , sizeof(float) , L3 ,file);
        fread(layer3_weights[i] , sizeof(float) , L3 ,file); 
        fread(layer3_biases[i] , sizeof(float) , 1 ,file); 
        transpose_weights(layer1_weights[i] , 2*L1 , L2);
    }
    fclose(file);
    return 1;
}

//Todo: write this with incremental update inside make_move. It is very slow in this way
int calculate_indices(Position* pos)
{
    uint8_t b_king = mailbox[ PieceList[B_KING][0] ] ;
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
            uint8_t sq = mailbox[ PieceList[i][k] ];
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
    b_king = Mirror(b_king);

    if(handle_king(&w_king))
    {
        Vertical_mirror_all_pieces(0);
    }
    if(handle_king(&b_king))
    {
        Vertical_mirror_all_pieces(1);
    }
        for(int k=0 ; k<sz ; k++)
    {
        weight_indices[0][k] = L1*(w_king*704 + active_neurons[0][k]);
        weight_indices[1][k] = L1*(b_king*704 + active_neurons[1][k]);
    }
    if(sz >= 20)
        game_phase=0;
    else
        game_phase=1;
}
int calculate_input_layer(int side)
{
    __m256i *biases = (__m256i *) &feature_biases[game_phase][0];
    __m256i *outputs1 = (__m256i*) &layer_1[0];//side_to_move
    __m256i *outputs2 = (__m256i*) &layer_1[L1];//for enemy
    if(L1 == 128)
    {
        outputs1[0]= biases[0];
        outputs1[1]= biases[1];
        outputs1[2]= biases[2];
        outputs1[3]= biases[3];
        outputs1[4]= biases[4];
        outputs1[5]= biases[5];
        outputs1[6]= biases[6];
        outputs1[7]= biases[7];
        
        outputs2[0]= biases[0];
        outputs2[1]= biases[1];
        outputs2[2]= biases[2];
        outputs2[3]= biases[3];
        outputs2[4]= biases[4];
        outputs2[5]= biases[5];
        outputs2[6]= biases[6];
        outputs2[7]= biases[7];
        for(int k=0 ; k < sz; k++)
        {
            __m256i *weights1 = (__m256i*) &feature_weights[game_phase][weight_indices[side][k]];
            __m256i *weights2 = (__m256i*) &feature_weights[game_phase][weight_indices[!side][k]];
            outputs1[ 0]= _mm256_add_epi16(outputs1[ 0] , weights1[ 0]);
            outputs1[ 1]= _mm256_add_epi16(outputs1[ 1] , weights1[ 1]);
            outputs1[ 2]= _mm256_add_epi16(outputs1[ 2] , weights1[ 2]);
            outputs1[ 3]= _mm256_add_epi16(outputs1[ 3] , weights1[ 3]);
            outputs1[ 4]= _mm256_add_epi16(outputs1[ 4] , weights1[ 4]);
            outputs1[ 5]= _mm256_add_epi16(outputs1[ 5] , weights1[ 5]);
            outputs1[ 6]= _mm256_add_epi16(outputs1[ 6] , weights1[ 6]);
            outputs1[ 7]= _mm256_add_epi16(outputs1[ 7] , weights1[ 7]);

            outputs2[ 0]= _mm256_add_epi16(outputs2[ 0] , weights2[ 0]);
            outputs2[ 1]= _mm256_add_epi16(outputs2[ 1] , weights2[ 1]);
            outputs2[ 2]= _mm256_add_epi16(outputs2[ 2] , weights2[ 2]);
            outputs2[ 3]= _mm256_add_epi16(outputs2[ 3] , weights2[ 3]);
            outputs2[ 4]= _mm256_add_epi16(outputs2[ 4] , weights2[ 4]);
            outputs2[ 5]= _mm256_add_epi16(outputs2[ 5] , weights2[ 5]);
            outputs2[ 6]= _mm256_add_epi16(outputs2[ 6] , weights2[ 6]);
            outputs2[ 7]= _mm256_add_epi16(outputs2[ 7] , weights2[ 7]);
        }
        quan_clipped_relu(layer_1 , 2*L1);//activation func.
    }
}
int quan_matrix_multp(int16_t weight_matrix[] ,int32_t biases[], int16_t input[] , float output[] , int input_dim , int output_dim )
{
    if(output_dim == 8)
    {
        __m256i* input_vector=(__m256i*)&input[0];
        for(int j=0 ; j<output_dim ; j++)
        {
            __m256i* weights = (__m256i*)&weight_matrix[j*input_dim];
            alignas(64) int32_t result[4];
            __m256i out0= _mm256_madd_epi16(input_vector[0 ] , weights[0 ]);
            __m256i out1= _mm256_madd_epi16(input_vector[1 ] , weights[1 ]);
            __m256i out2= _mm256_madd_epi16(input_vector[2 ] , weights[2 ]);
            __m256i out3= _mm256_madd_epi16(input_vector[3 ] , weights[3 ]);
            __m256i out4= _mm256_madd_epi16(input_vector[4 ] , weights[4 ]);
            __m256i out5= _mm256_madd_epi16(input_vector[5 ] , weights[5 ]);
            __m256i out6= _mm256_madd_epi16(input_vector[6 ] , weights[6 ]);
            __m256i out7= _mm256_madd_epi16(input_vector[7 ] , weights[7 ]);
            for(int i=8; i<input_dim/(16) ; i+=8)
            {
                out0= _mm256_add_epi32(out0 , _mm256_madd_epi16(input_vector[i + 0 ] , weights[i + 0 ]));
                out1= _mm256_add_epi32(out1 , _mm256_madd_epi16(input_vector[i + 1 ] , weights[i + 1 ]));
                out2= _mm256_add_epi32(out2 , _mm256_madd_epi16(input_vector[i + 2 ] , weights[i + 2 ]));
                out3= _mm256_add_epi32(out3 , _mm256_madd_epi16(input_vector[i + 3 ] , weights[i + 3 ]));
                out4= _mm256_add_epi32(out4 , _mm256_madd_epi16(input_vector[i + 4 ] , weights[i + 4 ]));
                out5= _mm256_add_epi32(out5 , _mm256_madd_epi16(input_vector[i + 5 ] , weights[i + 5 ]));
                out6= _mm256_add_epi32(out6 , _mm256_madd_epi16(input_vector[i + 6 ] , weights[i + 6 ]));
                out7= _mm256_add_epi32(out7 , _mm256_madd_epi16(input_vector[i + 7 ] , weights[i + 7 ]));
            }
            __m256i out01 = _mm256_add_epi32(out0 ,out1);
            __m256i out23 = _mm256_add_epi32(out2 ,out3);
            __m256i out45 = _mm256_add_epi32(out4, out5);
            __m256i out67 = _mm256_add_epi32(out6 , out7);
            __m256i out0123=_mm256_add_epi32(out01, out23);
            __m256i out4567=_mm256_add_epi32(out45 , out67);
            __m256i sum=  _mm256_add_epi32(out0123, out4567);
            __m128i high_sum = _mm256_extractf128_si256(sum , 1);
            __m128i low_sum = _mm256_castsi256_si128(sum);
              *(__m128i*) &result[0] = _mm_add_epi32(high_sum , low_sum);
            output[j] = (biases[j] + result[0] + result[1] +result[2] +result[3])/(SCALE_WEIGHT*SCALE_WEIGHT*1.0); // Todo: Write this operation with intel instrincs.
        }
        clipped_relu(output, 8);
    }
    return 1;
}
int matrix_multp(float weight_matrix[] ,float biases[], float input[] , float output[] , int input_dim , int output_dim )
{
    if(output_dim == 1)
    {   
        output[0]=biases[0];
         for(int i=0 ; i<input_dim ; i++)
        {
            if(input[i] > 0.001)
            {
                output[0] += input[i]*weight_matrix[i];
            }
        }
        return 1;
    }
    else if(output_dim ==32)
    {
        __m256 out0,out1,out2,out3, scalar;
        out0= _mm256_load_ps(&biases[0]);
        out1= _mm256_load_ps(&biases[8]);
        out2= _mm256_load_ps(&biases[16]);
        out3= _mm256_load_ps(&biases[24]);
        
            for(int i=0 ; i<input_dim ; i++)
            {
                if(input[i] > 0.99)// if input == 1 do not multipy just do addition
                {
                    out0 = _mm256_add_ps( _mm256_loadu_ps(&weight_matrix[i*output_dim ]) ,out0);
                    out1 = _mm256_add_ps( _mm256_loadu_ps(&weight_matrix[i*output_dim+8 ]) ,out1);
                    out2 = _mm256_add_ps( _mm256_loadu_ps(&weight_matrix[i*output_dim +16]) ,out2);
                    out3 = _mm256_add_ps( _mm256_loadu_ps(&weight_matrix[i*output_dim +24]) ,out3);
                }
                //if input[i] is nonzero, calculate it.
                //I could not find intel instrincs for scaling so just do normal multipication.
                else if(input[i] > 0.01)
                {
                    scalar = _mm256_set1_ps(input[i]);
                    out0 =_mm256_fmadd_ps(  scalar,  _mm256_loadu_ps(&weight_matrix[i*output_dim ]) ,out0) ;
                    out1 =_mm256_fmadd_ps(  scalar, _mm256_loadu_ps(&weight_matrix[i*output_dim  +8]) ,out1) ; 
                    out2 =_mm256_fmadd_ps(  scalar,_mm256_loadu_ps(&weight_matrix[i*output_dim + 16]) ,out2) ;
                    out3 =_mm256_fmadd_ps(  scalar, _mm256_loadu_ps(&weight_matrix[i*output_dim +24]) , out3);
                }
            }
        _mm256_store_ps(&output[0] , out0);
        _mm256_store_ps(&output[8] , out1);
        _mm256_store_ps(&output[16] , out2);
        _mm256_store_ps(&output[24] , out3);
        clipped_relu(output , output_dim);
        return 1;
    }
    else if(output_dim == 8)
    {
        __m256 out0, scalar;
        out0= _mm256_load_ps(&biases[0]);
        
            for(int i=0 ; i<input_dim ; i++)
            {
                if(input[i] > 0.99)
                {
                    out0 = _mm256_add_ps( _mm256_loadu_ps(&weight_matrix[i*output_dim ]) ,out0);
                }
                else if(input[i] > 0.01 )//if input[i] is nonzero, calculate it
                {
                        scalar = _mm256_set1_ps(input[i]);
                        out0 =_mm256_fmadd_ps(  scalar,  _mm256_loadu_ps(&weight_matrix[i*output_dim ]) ,out0) ;
                }
            }
        _mm256_store_ps(&output[0] , out0);
        clipped_relu(output , output_dim);
        return 1;
    }
}
int16_t evaluate_network(Position* pos )
{
    if(pos->half_move >= 100)
        return 0;
    if(black_pawns == 0 && white_pawns == 0 
     && PieceList[QUEEN][0] == 0 && PieceList[B_QUEEN][0] == 0
     && PieceList[ROOK][0] == 0 && PieceList[B_ROOK][0] == 0
     && PieceList[B_BISHOP][1] == 0 && PieceList[BISHOP][1] == 0 
     && PieceList[B_KNIGHT][1] == 0 && PieceList[KNIGHT][1] == 0  )//insufficent material detection 
    {       
            int k=0;
            k += (PieceList[BISHOP][0] > 0 );
            k += (PieceList[B_BISHOP][0] > 0);
            k += (PieceList[KNIGHT][0] > 0);
            k += (PieceList[B_KNIGHT][0] > 0);
            if(k<= 1)
            {
                return 0;
            }
    }
    calculate_indices(pos);
    calculate_input_layer(pos->side_to_move);
    quan_matrix_multp(layer1_weights[game_phase] ,layer1_biases[game_phase] , layer_1, layer_2 , 2*L1 ,L2);
    matrix_multp(layer2_weights[game_phase] ,layer2_biases[game_phase] , layer_2, layer_3 , L2 ,L3);
    matrix_multp(layer3_weights[game_phase] ,layer3_biases[game_phase] , layer_3, output ,  L3 , 1);
     return output[0]*SCALE * ((100.0 -pos->half_move)/100.0); 
}
#endif
