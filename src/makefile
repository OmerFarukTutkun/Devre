CC       = gcc
CFLAGS   =  -march=native -Wall -Ofast -flto -D USE_AVX2
EXE      = devre
SRC      = *.c
EXT      = .exe
RELEASE  = $(CC) $(CFLAGS) $(SRC) -o $(EXE)
release: 
	$(RELEASE)$(EXT)