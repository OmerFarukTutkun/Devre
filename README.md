## Devre

Devre is an open-source UCI compatible chess engine written in C as a hobby project. While writing the engine, I got great help from chessprogramming wiki , talkchess forum,stcokfish discord ,and some open-source engines: Ethereal, Vice, Koivisto. 

## Rating

| Version  | [CCRL (Blitz)](http://ccrl.chessdom.com/ccrl/404/) | [Owl Chess Blitz](http://chessowl.blogspot.com/) | [BRUCE](https://www.e4e6.com/)
| ------------- | ------------- |----------|----------|
| Devre 2.0  | 3102  | 3035 | 3066
| Devre 1.0  | 2954  | 2874 |


## Movegen

* Fancy magic bitboards
* pseduo-legal movegen with make/unmake.



## Search
* Alpha beta search (PVS)
* Quiessence search
* Transposition table
* Iterative Depening
* Aspiration Window
* Null Move Pruning
* Mate Distance Pruning
* Late Move Reduction
* Check Extension
## Move ordering
*  Hash move
*  Good Captures sorted by MVV-LVA
*  Killer move
*  countermove
*  History heuristic
*  Bad Captures sorted by MVV-LVA


## Evaluation

Devre uses a small NNUE for evaluation. The Network architure is 768 x (512x2) x 1.
The default net was trained with 1 billion positions from Leela data. The trianing code written in C,and can be found in https://github.com/OmerFarukTutkun/NNUETrainer .  The training resources and other useful information about NNUE can be found in stockfish discord.
Thanks to Stockfish and Leela teams for publishing their training data in public. 

## Compiling 
 To compile in Windows with a cpu that supports AVX2 or SSE3:

 * gcc -march=native -Ofast -flto -D USE_AVX2 *.c -o devre_avx2
 * gcc -march=native -Ofast -flto -D USE_SSE3 *.c -o devre_sse3
