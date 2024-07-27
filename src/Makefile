CC       = g++
CXX_FLAGS   = -funroll-loops --param max-unroll-times=100 -Wall -Wformat=0 -Ofast -flto -std=c++20
AVX2     =  -D USE_AVX2
SSE3     =  -D USE_SSE3
AVX512   =  -D USE_AVX512
EXE      = devre
SRC      = *.cpp
LIBS     = -lm
EXT      =
NETWORK  = devreT80L1=1536.nnue
DOWNLOAD_NETWORK     =
NATIVE   = 1
VERSION = 5.08
ifeq ($(OS),Windows_NT)
	EXT += .exe
	DOWNLOAD_NETWORK += curl -skL "https://github.com/OmerFarukTutkun/DevreNets/raw/main/$(NETWORK)" > "./$(NETWORK)"
else
	DOWNLOAD_NETWORK += wget https://github.com/OmerFarukTutkun/DevreNets/raw/main/$(NETWORK)
endif

ifeq ($(NATIVE),1)
	AVX2 += -march=native
	AVX512 += -march=native
	SSE3 += -march=native
else
	SSE3 += -msse -msse2 -msse3
	AVX2 += -msse -msse2 -msse3 -mssse3 -mavx -msse4.1 -mavx2
endif

avx512: network
	$(CC) $(CXX_FLAGS) $(AVX512) $(SRC) $(LIBS) -DVERSION=\"$(VERSION)\" -DNET=\"$(NETWORK)\" -o $(EXE)$(EXT)
avx2: network
	$(CC) $(CXX_FLAGS) $(AVX2) $(SRC) $(LIBS) -DVERSION=\"$(VERSION)\" -DNET=\"$(NETWORK)\" -o $(EXE)$(EXT)
sse3: network
	$(CC) $(CXX_FLAGS) $(SSE3) $(SRC) $(LIBS) -DVERSION=\"$(VERSION)\" -DNET=\"$(NETWORK)\" -o $(EXE)$(EXT)
network:
ifeq (,$(wildcard ./$(NETWORK)))
	$(DOWNLOAD_NETWORK)
endif