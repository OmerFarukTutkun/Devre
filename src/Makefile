CC       = g++
CXX_FLAGS   = -funroll-loops --param max-unroll-times=100 -Wall -Wformat=0 -Ofast -flto
AVX2     =  -D USE_AVX2
SSE3     =  -D USE_SSE3
EXE      = devre
SRC      = *.cpp
LIBS     = -lm
EXT      =
NETWORK  = devre_23.09.22_ep120.nnue
DOWNLOAD_NETWORK     =
NATIVE   = 1
VERSION = 4.72
ifeq ($(OS),Windows_NT)
	EXT += .exe
	DOWNLOAD_NETWORK += curl -skL "https://github.com/OmerFarukTutkun/DevreNets/raw/main/$(NETWORK)" > "./$(NETWORK)"
else
	DOWNLOAD_NETWORK += wget https://github.com/OmerFarukTutkun/DevreNets/raw/main/$(NETWORK)
endif

ifeq ($(NATIVE),1)
	AVX2 += -march=native
	SSE3 += -march=native
else
	SSE3 += -msse -msse2 -msse3
	AVX2 += -msse -msse2 -msse3 -mssse3 -mavx -msse4.1 -mavx2
endif

avx2: network
	$(CC) $(CXX_FLAGS) $(AVX2) $(SRC) $(LIBS) -DVERSION=$(VERSION) -o $(EXE)$(EXT)
sse3: network
	$(CC) $(CXX_FLAGS) $(SSE3) $(SRC) $(LIBS) -DVERSION=$(VERSION) -o $(EXE)$(EXT)
network:
ifeq (,$(wildcard ./$(NETWORK)))
	$(DOWNLOAD_NETWORK)
endif