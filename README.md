## Devre

Devre is an open-source UCI compatible chess engine written in C as a hobby project.The code is a bit badly written now but I'll try to make it cleaner and more efficent in my free time. Although Devre is an original chess engine written from scratch, I got great help from chessprogramming wiki , talkchess forum,stcokfish discord ,and some open-source C engines: Ethereal, Vice, CPW. 

## Rating

| Version  | [CCRL (Blitz)](http://ccrl.chessdom.com/ccrl/404/) | [Owl Chess Blitz](http://chessowl.blogspot.com/) |
| ------------- | ------------- |----------|
| Devre 2.0  | -  | 2962
| Devre 1.0  | 2954  | 2797


## Movegen

* 12x10 mailbox board representation 
* Piece lists
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
* Hash move
* Promotion Capture
* Promotion
*  Ep-capture
*  Captures sorted by MVV-LVA
*  Killer move
*  Castlings
*  countermove
*  History heuristic


## Evaluation

Devre uses two small NNUE for evaluation. The nets were trained using Stockfish and Leela data with my own trainer in Tensorflow/Keras. Current nets were trained with ~600 million positions from Leela data. The training resources and other useful information about NNUE can be found in stockfish discord.
Thanks to Stockfish and Leela teams for publishing their training data in public. 

## Compiling 
 To compile in Windows with a cpu that supports AVX2 or SSE3:

 * gcc -march=native -Ofast -D USE_AVX2 main.c -o devre_avx2
 * gcc -march=native -Ofast -D USE_SSE3 main.c -o devre_sse3
