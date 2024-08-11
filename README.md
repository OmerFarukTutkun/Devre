## Devre

Devre is a strong open-source UCI compatible chess engine written in C++. While writing the engine, I got great help from chessprogramming wiki, talkchess forum, Stockfish discord ,and some open-source engines: Ethereal, Vice, Koivisto. 


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
* Correction History

## Move ordering
*  Hash move
*  Good Captures sorted by Capture History
*  Killer moves
*  countermove
*  History heuristic
*  Bad Captures sorted  by Capture History


## Evaluation

Devre uses a NNUE for evaluation. The Network architure is 768 -> (1536x2) -> 1.
The default net was trained with Leela data. The training code written in C/CUDA,and can be found in https://github.com/OmerFarukTutkun/CUDA-Trainer .  The training resources and other useful information about NNUE can be found in stockfish discord.
Thanks to Stockfish and Leela teams for publishing their training data in public. 

## Compiling 
 To compile in Linux/Windows with a cpu that supports AVX512:
 * to compile with makefile you can use one of the options: ```make``` ```make build=avx512``` ```make build=avx2```
