#thanks for https://github.com/gab8192/Obsidian for Makefile
CXX      = g++
LIBS     = -lm
# Named after the first 12 hex of its sha256. Override with NETWORK=<file>.
NETWORK ?= devre-6e8b27f4e26d.bin

M64     = -m64 -mpopcnt
MSSE2   = $(M64) -msse -msse2
MSSSE3  = $(MSSE2) -mssse3
MAVX2   = $(MSSSE3) -msse4.1 -mbmi -mfma -mavx2
MAVX512 = $(MAVX2) -mavx512f -mavx512bw

FILES = *.cpp datagen/*.cpp fathom/src/tbprobe.c

# -I. lets sources under datagen/ include the engine headers that live in src/.
CXX_FLAGS  = -I. -static -pthread -fno-rtti -funroll-loops --param max-unroll-times=100 -Wall -Wformat=0 -Wno-coverage-mismatch -O3 -flto=auto -std=c++17 -D TB_NO_HELPER_API

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
ifeq ($(OS),Windows_NT)
	-if not exist devre_pgo mkdir devre_pgo 2>nul
else
	mkdir -p devre_pgo
endif
	$(COMMAND) -fprofile-generate="devre_pgo"
ifeq ($(OS),Windows_NT)
	$(EXE) bench
else
	./$(EXE) bench
endif
	$(COMMAND) -fprofile-use="devre_pgo"
ifeq ($(OS),Windows_NT)
	-if exist devre_pgo rmdir /s /q devre_pgo 2>nul
else
	rm -rf devre_pgo
endif

nopgo: network
	$(COMMAND)

network:
ifeq (,$(wildcard ./$(NETWORK)))
	$(DOWNLOAD_NETWORK)
endif

clean:
ifeq ($(OS),Windows_NT)
	-del /q /f $(EXE) 2>nul
	-if exist devre_pgo rmdir /s /q devre_pgo 2>nul
else
	rm -rf $(EXE) devre_pgo
endif
