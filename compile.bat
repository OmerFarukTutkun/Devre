clang -march=native  -Wall   -Ofast -D USE_AVX2  main.c -o devre-avx2.exe 
clang -march=barcelona  -Wall   -Ofast -D USE_SSE3  main.c -o devre-sse3.exe
