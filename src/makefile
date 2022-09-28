CC       = gcc
CFLAGS   =  -Wall -Wformat=0 -Ofast -flto 
AVX2     =  -D USE_AVX2
SSE3     =  -D USE_SSE3
EXE      = devre
SRC      = *.c
LIBS     = -lm
EXT      =
NETWORK  = devre_23.09.22_ep120.nnue
DOWNLOAD_NETWORK     = wget https://github.com/OmerFarukTutkun/DevreNets/raw/main/$(NETWORK)
NATIVE   = 1
ifeq ($(OS),Windows_NT)
	EXT += .exe
endif

ifeq ($(NATIVE),1)
	AVX2 += -march=native
	SSE3 += -march=native
else
	SSE3 += -msse -msse2 -msse3 
	AVX2 += -msse -msse2 -msse3 -mssse3 -mavx -msse4.1 -mavx2
endif

avx2: network
	$(CC) $(CFLAGS) $(AVX2) $(SRC) $(LIBS) -o $(EXE)-avx2$(EXT)
sse3: network
	$(CC) $(CFLAGS) $(SSE3) $(SRC) $(LIBS) -o $(EXE)-sse3$(EXT)
	
network:
ifeq (,$(wildcard ./$(NETWORK)))
	$(DOWNLOAD_NETWORK)
endif
