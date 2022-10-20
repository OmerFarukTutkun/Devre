## Devre

Devre is a strong open-source UCI compatible chess engine written in C as a hobby project.Devre is currently ranked 22th in CCRL 40/15. While writing the engine, I got great help from chessprogramming wiki , talkchess forum,stcokfish discord ,and some open-source engines: Ethereal, Vice, Koivisto. 

## Rating

| Version  | [CEGT](http://www.cegt.net/40_4_Ratinglist/40_4_single/rangliste.html) | [CEGT 40/20](http://www.cegt.net/40_40%20Rating%20List/40_40%20SingleVersion/rangliste.html) | [CCRL Blitz](http://ccrl.chessdom.com/ccrl/404/) |  [CCRL 40/15](http://ccrl.chessdom.com/ccrl/4040/) | [Owl Chess Blitz](http://chessowl.blogspot.com/) | [BRUCE](https://www.e4e6.com/) | [SPCC](https://www.sp-cc.de/)
| ------------- | ------------- |----------|----------|----------|----------|----------|----------|
| Devre 4.0     | 3320          |   3301   |3401      | 3311     |          |          | 3442
| Devre 3.07    |               |          |          | 3292     |          |          |
| Devre 3.0     | 3101          |          |3225      |          |          |          |
| Devre 2.0     |               |          |3104      |          | 3035     | 3066     |
| Devre 1.0     |               |          |2955      |          |2874      |          |


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
* Futility prunings
* SEE reduction and pruning
## Move ordering
*  Hash move
*  Good Captures sorted by Capture History
*  Killer moves
*  countermove
*  History heuristic
*  Bad Captures sorted  by Capture History


## Evaluation

Devre uses a NNUE for evaluation. The Network architure is 32*768 -> (512x2) -> 1.
The default net was trained with 2 billion positions from Leela data. The training code written in C/CUDA,and can be found in https://github.com/OmerFarukTutkun/CUDA-Trainer .  The training resources and other useful information about NNUE can be found in stockfish discord.
Thanks to Stockfish and Leela teams for publishing their training data in public. 

## Compiling 
 To compile in Linux/Windows with a cpu that supports AVX2 or SSE3:
 * to compile with makefile you can use one of the options: ```make avx2``` , ```make sse3``` , ```make NATIVE=0 avx2``` ,```make NATIVE=0 sse3```
 #### Or
 * First download the default nnue from https://github.com/OmerFarukTutkun/DevreNets and put it into src directory. The name of the default NNUE is written in the makefile.
 * gcc -march=native -Ofast -flto -D USE_AVX2 *.c -lm -o devre_avx2
 * gcc -march=native -Ofast -flto -D USE_SSE3 *.c -lm -o devre_sse3
