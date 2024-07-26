## Devre

Devre is a strong open-source UCI compatible chess engine written in C++. While writing the engine, I got great help from chessprogramming wiki , talkchess forum,stcokfish discord ,and some open-source engines: Ethereal, Vice, Koivisto. 

## Rating

| Version  | [CEGT](http://www.cegt.net/40_4_Ratinglist/40_4_single/rangliste.html) | [CEGT 40/20](http://www.cegt.net/40_40%20Rating%20List/40_40%20SingleVersion/rangliste.html) | [CCRL Blitz](http://ccrl.chessdom.com/ccrl/404/) |  [CCRL 40/15](http://ccrl.chessdom.com/ccrl/4040/) | [Owl Chess Blitz](http://chessowl.blogspot.com/)  | [SPCC](https://www.sp-cc.de/)
| ------------- | ------------- |----------|----------|----------|-----------|----------|
| Devre 4.0     | 3321          |   3305   |3401      | 3311     |  3179   | 3442
| Devre 3.07    |               |          |          | 3292     |         |
| Devre 3.0     | 3099          |          |3225      |          |           |
| Devre 2.0     |               |          |3104      |          | 3035     |
| Devre 1.0     |               |          |2955      |          |2874       |


## Movegen

* Fancy magic bitboards
* legal movegen with make/unmake.



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
* Futility prunings
* SEE pruning
* Singular Extension
## Move ordering
*  Hash move
*  Good Captures sorted by Capture History
*  Killer moves
*  countermove
*  History heuristic
*  Bad Captures sorted  by Capture History


## Evaluation

Devre uses a NNUE for evaluation. The Network architure is 768 -> (1536x2) -> 1.
The default net was trained with T80 Leela data. The training code written in C/CUDA,and can be found in https://github.com/OmerFarukTutkun/CUDA-Trainer .  The training resources and other useful information about NNUE can be found in stockfish discord.
Thanks to Stockfish and Leela teams for publishing their training data in public. 

## Compiling 
 To compile in Linux/Windows with a cpu that supports AVX512, AVX2 or SSE3:
 * to compile with makefile you can use one of the options: ```make avx512``` ```make avx2``` , ```make sse3``` , ```make NATIVE=0 avx2``` ,```make NATIVE=0 sse3```
