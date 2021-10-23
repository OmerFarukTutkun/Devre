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
    __m128i zero = _mm_setzero_si128 (); 
    __m128i max =  _mm_set1_epi16(SCALE_WEIGHT);
    __m128i* input = (__m128i*) &x[0];
    for (int i = 0; i < dim/8; i+=4)
    {
      input[i+0] =  _mm_max_epi16(zero, input[i + 0]);
      input[i+0] =  _mm_min_epi16(max, input[i + 0]);

      input[i+1] =  _mm_max_epi16(zero, input[i + 1]);
      input[i+1] =  _mm_min_epi16(max, input[i + 1]);

      input[i+2] =  _mm_max_epi16(zero, input[i + 2]);
      input[i+2] =  _mm_min_epi16(max, input[i + 2]);

      input[i+3] =  _mm_max_epi16(zero, input[i + 3]);
      input[i+3] =  _mm_min_epi16(max, input[i + 3]);
    }
}
void clipped_relu(float x[] , int dim)
{
    __m128 zero = _mm_setzero_ps(); 
    __m128 max = _mm_set1_ps(1.0);
    __m128* input = (__m128*) &x[0];
    for (int i = 0; i < dim/4; i+=2)
    {
      input[i+0] =  _mm_max_ps(zero, input[i + 0]);
      input[i+0] =  _mm_min_ps(max, input[i + 0]);

      input[i+1] =  _mm_max_ps(zero, input[i + 1]);
      input[i+1] =  _mm_min_ps(max, input[i + 1]);
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
    __m128i *biases = (__m128i *) &feature_biases[game_phase][0];
    __m128i *outputs1 = (__m128i*) &layer_1[0];//side_to_move
    __m128i *outputs2 = (__m128i*) &layer_1[L1];//for enemy
    if(L1 == 128)
    {
        for(int k=0 ; k<L1/8; k+=4)
        {
            outputs1[k +0]= biases[k+ 0];
            outputs1[k +1]= biases[k+ 1];
            outputs1[k+ 2]= biases[k+ 2];
            outputs1[k+ 3]= biases[k+ 3];

            outputs2[k +0]= biases[k+ 0];
            outputs2[k +1]= biases[k+ 1];
            outputs2[k+ 2]= biases[k+ 2];
            outputs2[k+ 3]= biases[k+ 3];
        }
        for(int j=0 ; j < sz; j++)
        {
            __m128i *weights1 = (__m128i*) &feature_weights[game_phase][weight_indices[side][j]];
            __m128i *weights2 = (__m128i*) &feature_weights[game_phase][weight_indices[!side][j]];

            for(int k=0 ; k<L1/8; k+=4)
            {
                outputs1[k +0]= _mm_add_epi16(outputs1[k +  0] , weights1[k + 0]);
                outputs1[k +1]= _mm_add_epi16(outputs1[k +  1] , weights1[k + 1]);
                outputs1[k+ 2]= _mm_add_epi16(outputs1[k +  2] , weights1[k + 2]);
                outputs1[k+ 3]= _mm_add_epi16(outputs1[k +  3] , weights1[k + 3]);

                outputs2[k +0]= _mm_add_epi16(outputs2[k +  0] , weights2[k + 0]);
                outputs2[k +1]= _mm_add_epi16(outputs2[k +  1] , weights2[k + 1]);
                outputs2[k+ 2]= _mm_add_epi16(outputs2[k +  2] , weights2[k + 2]);
                outputs2[k+ 3]= _mm_add_epi16(outputs2[k +  3] , weights2[k + 3]);
            }
        }
        quan_clipped_relu(layer_1 , 2*L1);//activation func.
    }
}
int quan_matrix_multp(int16_t weight_matrix[] ,int32_t biases[], int16_t input[] , float output[] , int input_dim , int output_dim )
{
    if(output_dim == 8)
    {
        __m128i* input_vector=(__m128i*)&input[0];
        for(int j=0 ; j<output_dim ; j++)
        {
            __m128i* weights = (__m128i*)&weight_matrix[j*input_dim];
            alignas(64) int32_t result[4];
            __m128i out0= _mm_madd_epi16(input_vector[0 ] , weights[0 ]);
            __m128i out1= _mm_madd_epi16(input_vector[1 ] , weights[1 ]);
            __m128i out2= _mm_madd_epi16(input_vector[2 ] , weights[2 ]);
            __m128i out3= _mm_madd_epi16(input_vector[3 ] , weights[3 ]);
            __m128i out4= _mm_madd_epi16(input_vector[4 ] , weights[4 ]);
            __m128i out5= _mm_madd_epi16(input_vector[5 ] , weights[5 ]);
            __m128i out6= _mm_madd_epi16(input_vector[6 ] , weights[6 ]);
            __m128i out7= _mm_madd_epi16(input_vector[7 ] , weights[7 ]);
            for(int i=8; i<input_dim/(8) ; i+=8)
            {
                out0= _mm_add_epi32(out0 , _mm_madd_epi16(input_vector[i + 0 ] , weights[i + 0 ]));
                out1= _mm_add_epi32(out1 , _mm_madd_epi16(input_vector[i + 1 ] , weights[i + 1 ]));
                out2= _mm_add_epi32(out2 , _mm_madd_epi16(input_vector[i + 2 ] , weights[i + 2 ]));
                out3= _mm_add_epi32(out3 , _mm_madd_epi16(input_vector[i + 3 ] , weights[i + 3 ]));
                out4= _mm_add_epi32(out4 , _mm_madd_epi16(input_vector[i + 4 ] , weights[i + 4 ]));
                out5= _mm_add_epi32(out5 , _mm_madd_epi16(input_vector[i + 5 ] , weights[i + 5 ]));
                out6= _mm_add_epi32(out6 , _mm_madd_epi16(input_vector[i + 6 ] , weights[i + 6 ]));
                out7= _mm_add_epi32(out7 , _mm_madd_epi16(input_vector[i + 7 ] , weights[i + 7 ]));
            }
            __m128i out01 = _mm_add_epi32(out0 ,out1);
            __m128i out23 = _mm_add_epi32(out2 ,out3);
            __m128i out45 = _mm_add_epi32(out4, out5);
            __m128i out67 = _mm_add_epi32(out6 , out7);
            __m128i out0123=_mm_add_epi32(out01, out23);
            __m128i out4567=_mm_add_epi32(out45 , out67);
            __m128i sum=  _mm_add_epi32(out0123, out4567);
            *(__m128i*) &result[0] = sum;
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
        __m128 out0,out1,out2,out3,out4,out5,out6,out7, scalar;
        out0= _mm_load_ps(&biases[0]);
        out1= _mm_load_ps(&biases[4]);
        out2= _mm_load_ps(&biases[8]);
        out3= _mm_load_ps(&biases[12]);
        out4= _mm_load_ps(&biases[16]);
        out5= _mm_load_ps(&biases[20]);
        out6= _mm_load_ps(&biases[24]);
        out7= _mm_load_ps(&biases[28]);
        
            for(int i=0 ; i<input_dim ; i++)
            {
                if(input[i] > 0.99)// if input == 1 do not multipy just do addition
                {
                    out0 = _mm_add_ps( _mm_loadu_ps(&weight_matrix[i*output_dim ]) ,out0);
                    out1 = _mm_add_ps( _mm_loadu_ps(&weight_matrix[i*output_dim+4 ]) ,out1);
                    out2 = _mm_add_ps( _mm_loadu_ps(&weight_matrix[i*output_dim +8]) ,out2);
                    out3 = _mm_add_ps( _mm_loadu_ps(&weight_matrix[i*output_dim +12]) ,out3);
                    out4 = _mm_add_ps( _mm_loadu_ps(&weight_matrix[i*output_dim +16]) ,out4);
                    out5 = _mm_add_ps( _mm_loadu_ps(&weight_matrix[i*output_dim+ 20 ]) ,out5);
                    out6 = _mm_add_ps( _mm_loadu_ps(&weight_matrix[i*output_dim +24]) ,out6);
                    out7 = _mm_add_ps( _mm_loadu_ps(&weight_matrix[i*output_dim +28]) ,out7);
                }
                //if input[i] is nonzero, calculate it.
                //I could not find intel instrincs for scaling so just do normal multipication.
                else if(input[i] > 0.01)
                {
                    scalar = _mm_set1_ps(input[i]);
                    out0 =_mm_add_ps(out0 ,  _mm_mul_ps(  scalar,  _mm_loadu_ps(&weight_matrix[i*output_dim ])) );
                    out1 =_mm_add_ps(out1 ,  _mm_mul_ps(  scalar,  _mm_loadu_ps(&weight_matrix[i*output_dim  +4])) ); 
                    out2 =_mm_add_ps(out2 ,  _mm_mul_ps(  scalar,  _mm_loadu_ps(&weight_matrix[i*output_dim + 8])) );
                    out3 =_mm_add_ps(out3 ,  _mm_mul_ps(  scalar,  _mm_loadu_ps(&weight_matrix[i*output_dim +12])));
                    out4 =_mm_add_ps(out4 ,  _mm_mul_ps(  scalar,  _mm_loadu_ps(&weight_matrix[i*output_dim +16]))) ;
                    out5 =_mm_add_ps(out5 ,  _mm_mul_ps(  scalar,  _mm_loadu_ps(&weight_matrix[i*output_dim  +20]))) ; 
                    out6 =_mm_add_ps(out6 ,  _mm_mul_ps(  scalar,  _mm_loadu_ps(&weight_matrix[i*output_dim + 24]))) ;
                    out7 =_mm_add_ps(out7 ,  _mm_mul_ps(  scalar,  _mm_loadu_ps(&weight_matrix[i*output_dim +28])));
                } 
            }
        _mm_store_ps(&output[0] , out0);
        _mm_store_ps(&output[4] , out1);
        _mm_store_ps(&output[8] , out2);
        _mm_store_ps(&output[12] , out3);
        _mm_store_ps(&output[16] , out4);
        _mm_store_ps(&output[20] , out5);
        _mm_store_ps(&output[24] , out6);
        _mm_store_ps(&output[28] , out7);
        clipped_relu(output , output_dim);
        return 1;
    }
    else if(output_dim == 8)
    {
        __m128 out0,out1, scalar;
        out0= _mm_load_ps(&biases[0]);
        out1= _mm_load_ps(&biases[1]);
        
            for(int i=0 ; i<input_dim ; i++)
            {
                if(input[i] > 0.999)
                {
                    out0 = _mm_add_ps( _mm_loadu_ps(&weight_matrix[i*output_dim ]) ,out0);
                    out1 = _mm_add_ps( _mm_loadu_ps(&weight_matrix[i*output_dim +4 ]) ,out1);
                }
                else if(input[i] > 0.001 )//if input[i] is nonzero, calculate it
                {
                        scalar = _mm_set1_ps(input[i]);
                        out0 =_mm_add_ps(out0 ,  _mm_mul_ps(  scalar,  _mm_loadu_ps(&weight_matrix[i*output_dim ])) );
                        out1 =_mm_add_ps(out1 ,  _mm_mul_ps(  scalar,  _mm_loadu_ps(&weight_matrix[i*output_dim  +4])) ); 
                }
            }
        _mm_store_ps(&output[0] , out0);
        _mm_store_ps(&output[4] , out1);
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
