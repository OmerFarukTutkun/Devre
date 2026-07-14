#ifndef DEVRE_DATAGEN_H
#define DEVRE_DATAGEN_H

// Self-play training-data generator.
//
// Usage:
//   Devre(.exe) datagen [threads] [outDir] [targetPositions] [softNodes] [temperaturePct]
//
//   threads          number of worker threads (default: hardware concurrency)
//   outDir           directory for output files       (default: ./data)
//   targetPositions  stop after ~this many scored positions, 0 = run until
//                    interrupted with Ctrl-C          (default: 0)
//   softNodes        soft node budget per move        (default: 5000)
//   temperaturePct   move-selection noise probability (0-100, default: 0).
//                    When > 0, with that % probability the best move is
//                    excluded and a second search discovers the second-best
//                    move, which is played instead. This makes games branch
//                    differently without playing outright blunders.
//
// Each worker plays independent games and streams them to its own file
// data/devre_<pid>_<worker>.bin using the on-disk format documented in
// datagen.cpp. Records are self-delimiting, so the per-worker files can simply
// be concatenated afterwards.
void runDatagen(int argc, char** argv);

#endif  //DEVRE_DATAGEN_H
