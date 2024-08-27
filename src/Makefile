#thanks for https://github.com/gab8192/Obsidian for Makefile
CXX      = g++
LIBS     = -lm
NETWORK  = devre_06.08.2024.nnue

M64     = -m64 -mpopcnt
MSSE2   = $(M64) -msse -msse2
MSSSE3  = $(MSSE2) -mssse3
MAVX2   = $(MSSSE3) -msse4.1 -mbmi -mfma -mavx2
MAVX512 = $(MAVX2) -mavx512f -mavx512bw

FILES = *.cpp

CXX_FLAGS  = -static -funroll-loops --param max-unroll-times=100 -Wall -Wformat=0 -Ofast -flto -std=c++17

ifeq ($(OS),Windows_NT)
	EXE := Devre.exe
	DOWNLOAD_NETWORK := curl -skL "https://github.com/OmerFarukTutkun/DevreNets/raw/main/$(NETWORK)" > "./$(NETWORK)"
else
	EXE := Devre
	DOWNLOAD_NETWORK := wget https://github.com/OmerFarukTutkun/DevreNets/raw/main/$(NETWORK)
endif


ifeq ($(build),)
	build = native
endif

ifeq ($(build), native)
    CXX_FLAGS += -march=native
else ifeq ($(findstring sse2, $(build)), sse2)
	CXX_FLAGS += $(MSSE2)
else ifeq ($(findstring ssse3, $(build)), ssse3)
	CXX_FLAGS += $(MSSSE3)
else ifeq ($(findstring avx2, $(build)), avx2)
	CXX_FLAGS += $(MAVX2)
else ifeq ($(findstring avx512, $(build)), avx512)
	CXX_FLAGS += $(MAVX512)
endif

ifeq ($(build), native)
	PROPS = $(shell echo | $(CC) -march=native -E -dM -)
	ifneq ($(findstring __BMI2__, $(PROPS)),)
		ifeq ($(findstring __znver1, $(PROPS)),)
			ifeq ($(findstring __znver2, $(PROPS)),)
				CXX_FLAGS += -DUSE_PEXT
			else ifeq ($(shell uname), Linux)
				ifneq ($(findstring AMD EPYC 7B, $(shell lscpu)),)
					CXX_FLAGS += -DUSE_PEXT
				endif
			endif
		endif
	endif
else ifeq ($(findstring pext, $(build)), pext)
	CXX_FLAGS += -DUSE_PEXT -mbmi2
endif

COMMAND = $(CXX) $(CXX_FLAGS) $(FILES) $(LIBS) -DNET=\"$(NETWORK)\" -o $(EXE)

default: network
	$(COMMAND) -fprofile-generate="devre_pgo"
ifeq ($(OS),Windows_NT)
	$(EXE) bench
else
	./$(EXE) bench
endif
	$(COMMAND) -fprofile-use="devre_pgo"
ifeq ($(OS),Windows_NT)
	rmdir /s /q devre_pgo
else
	rm -rf devre_pgo
endif

nopgo: network
	$(COMMAND)

network:
ifeq (,$(wildcard ./$(NETWORK)))
	$(DOWNLOAD_NETWORK)
endif
