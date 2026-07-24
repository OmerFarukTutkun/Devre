# Devre self-play data tools

## Generating data

Build the engine, then run the built-in generator (it bypasses UCI):

```
Devre datagen [threads] [outDir] [targetPositions] [softNodes] [temperaturePct] [frc] [randomPlies]
```

| arg               | default              | meaning                                                    |
|-------------------|----------------------|------------------------------------------------------------|
| `threads`         | hardware concurrency | one self-play worker per thread                            |
| `outDir`          | `./data`             | one `devre_<runstamp>_<worker>.vf` per worker              |
| `targetPositions` | `0` (until Ctrl-C)   | stop after ~this many scored positions                     |
| `softNodes`       | `5000`               | soft node budget per move                                  |
| `temperaturePct`  | `0`                  | move-selection noise % (0–100); 3–5% if used               |
| `frc`             | `0`                  | `0` standard, `1` DFRC (double Fischer-random) starts       |
| `randomPlies`     | mode default         | random opening plies; `0` = 8 for standard, 2 for DFRC     |

Each worker plays a few random opening plies, discards openings with
`|eval| > 300cp`, then self-plays at the soft node budget (best move, or the
second-best move `temperaturePct`% of the time) until adjudication (win at
`|eval| ≥ 1000cp`, draw at `|eval| ≤ 8cp`, or a natural terminal). In DFRC mode
each game starts from an independently scrambled white/black back rank, so the
scrambled start supplies the diversity and only 2–3 random plies are used. The
output is Chess960-ready (unmoved rooks encoded, castling written as
king-captures-rook) — point your trainer at it in DFRC castling mode.

Per-worker files are self-delimiting, so just concatenate them:

```
cat data/*.vf > all.vf
```

## On-disk format

See the header comment in [`datagen.cpp`](datagen.cpp) for the byte layout. Each
game is a 32-byte marlinformat packed start position (white-POV result in `wdl`)
followed by one 4-byte `(move, eval)` pair per scored position and a `0,0`
terminator.

- `move` is a **viriformat** 16-bit move (`from | to<<6 | promo<<12 | flag<<14`).
- `eval` is the **white-relative** search score in Devre's cp.
- `wdl` is the **white-relative** result: `0` loss, `1` draw, `2` win.

## Analyzing data

```
python analyze_data.py DATA            # -> datagen_report.html + text summary
python analyze_data.py DATA --no-html  # text only
python analyze_data.py DATA --max 0    # process every position (default samples 100k)
```

`DATA` is a `.vf`/`.bin` file or a directory of them. Pure stdlib. The HTML
dashboard has hoverable charts (WDL, score→WDL calibration, piece-square
heatmap, game length, best-move type, ply/halfmove clock).
