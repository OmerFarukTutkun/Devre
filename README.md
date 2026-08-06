## Devre

Devre is a strong open-source UCI-compatible chess engine written in C++. While writing the engine, I got great help from chessprogramming wiki, talkchess forum, Stockfish discord, and some open-source engines: Ethereal, Vice, and Koivisto. 


## Features

* Multithreaded search (Lazy SMP)
* Syzygy tablebase support
* Chess960 (FRC and DFRC)
* NNUE evaluation, embedded in the binary



## Evaluation

Devre uses a `(768x12 + 4560) -> 1024x2 -> 16 -> 32 -> 1` NNUE network for evaluation.

The inputs are king-bucketed piece-square features over 12 mirrored buckets, plus 4560 pawn-pair features. Each accumulator is combined pairwise into 512 activations, and the output head is picked from 8 material buckets.

The net is trained using the [bullet](https://github.com/JWinslow23/bullet) trainer on self-generated training data.


## Compiling 
 To compile in Linux/Windows with a CPU that supports AVX512/AVX2/SSSE3:
 * to compile with makefile you can use one of the options: ```make``` ```make build=avx512``` ```make build=avx2``` ```make build=ssse3```
