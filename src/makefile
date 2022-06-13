CC       = gcc
CFLAGS   =  -Wall -Ofast -flto 
AVX2   += -march=native -D USE_AVX2
SSE3   += -march=barcelona -D USE_SSE3
EXE      = devre
SRC      = *.c
LIBS     = -lm
EXT      =
ifeq ($(OS),Windows_NT)
	EXT += .exe
endif
avx2: 
	$(CC) $(CFLAGS) $(AVX2) $(SRC) $(LIBS) -o $(EXE)-avx2$(EXT)
sse3:
	$(CC) $(CFLAGS) $(SSE3) $(SRC) $(LIBS) -o $(EXE)-sse3$(EXT)
