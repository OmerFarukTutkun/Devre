# Devre self-play data tools

## Generating data

Build the engine, then run the built-in generator (it bypasses UCI entirely):

```
Devre datagen [threads] [outDir] [targetPositions] [softNodes]
```

| arg               | default              | meaning                                        |
|-------------------|----------------------|------------------------------------------------|
| `threads`         | hardware concurrency | one independent self-play worker per thread    |
| `outDir`          | `./data`             | one `devre_<runstamp>_<worker>.bin` per worker |
| `targetPositions` | `0` (until Ctrl-C)   | stop after ~this many scored positions         |
| `softNodes`       | `5000`               | soft node budget per move                      |

Each worker:

1. plays 8–9 random plies from the start position (alternating, for color balance);
2. runs a short verification search and discards openings with `|eval| > 300cp`;
3. plays out the game at the soft node budget, always choosing the best move
   (diversity comes from the random openings, not from move randomization);
4. adjudicates: win at `|eval| ≥ 1000cp` for 4 consecutive plies (or a mate
   score), draw at `|eval| ≤ 8cp` for 10 plies after ply 40, plus natural
   terminals (mate, stalemate, 50-move, repetition, insufficient material).

Per-worker files are self-delimiting records, so you can just concatenate them:

```
cat data/*.bin > all.bin
```

## On-disk format

See the header comment in [`datagen.cpp`](datagen.cpp) for the exact
byte layout. In short, every game is a 32-byte marlinformat-compatible packed
start position (with the white-POV game result in its `wdl` byte) followed by
one 4-byte `(move, eval)` pair per scored position and a `0,0` terminator.

- `move` is Devre's native 16-bit encoding (`from<<10 | to<<4 | type`).
- `eval` is the **white-relative** search score in centipawns.
- `wdl` is the **white-relative** game result: `0` loss, `1` draw, `2` win.

Storing the played best move plus the full move list means every derived signal
(position, ply, halfmove clock, piece counts, in-check, capture/promo best move)
is reconstructable offline, so all filtering decisions stay reversible — nothing
is thrown away at generation time except already-decided openings.

## Analyzing data

```
python src/datagen/analyze_data.py DATA --plots        # gallery -> analysis/index.html
python src/datagen/analyze_data.py DATA --plots out    # gallery -> out/index.html
python src/datagen/analyze_data.py DATA                # text summary only
python src/datagen/analyze_data.py DATA --max 2000000  # sample first N positions (faster)
```

`DATA` is a `.bin` file or a directory of them. Every distribution is reported
as a **percentage**.

The easiest way to read the results is `--plots`: it writes a self-contained
gallery — `index.html` plus PNGs — into one directory. Open `index.html` to see
a summary table and every chart on one page:

- game result (WDL) balance
- white-relative score distribution
- score→WDL calibration with the fitted sigmoid scale `K`
- game-length, position-ply and halfmove-clock distributions
- piece-count distribution and best-move type mix
- a 12-panel per-piece square-occupancy heatmap

Without `--plots` you still get the full percentage-based text summary (pure
stdlib). Plots need `matplotlib` (`pip install matplotlib`). `--piece wp` adds a
text heatmap for one piece; `--max N` samples the first N positions; `--no-check`
skips the slower in-check detection.
