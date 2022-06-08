gcc -march=native  -Wall -Ofast -flto -D USE_AVX2 *.c -o devre-avx2.exe
gcc -march=barcelona  -Wall -Ofast -flto -D USE_SSE3 *.c -o devre-sse3.exe