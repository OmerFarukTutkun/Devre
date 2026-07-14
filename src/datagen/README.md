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
- `eval` is the **white-relative** search score in Devre's raw internal units
  (= 2 × centipawns; UCI cp = `eval / 2`).
- `wdl` is the **white-relative** game result: `0` loss, `1` draw, `2` win.

Storing the played best move plus the full move list means every derived signal
(position, ply, halfmove clock, piece counts, in-check, capture/promo best move)
is reconstructable offline, so all filtering decisions stay reversible — nothing
is thrown away at generation time except already-decided openings.

## Analyzing data

```
python src/datagen/analyze_data.py DATA                 # -> datagen_report.html + text
python src/datagen/analyze_data.py DATA --html out.html # choose the output path
python src/datagen/analyze_data.py DATA --no-html       # text summary only
python src/datagen/analyze_data.py DATA --max 0         # process every position (not a sample)
```

`DATA` is a `.bin`/`.vf` file or a directory of them (both extensions are
picked up). Every distribution is reported as a **percentage**. Pure stdlib —
no matplotlib or other dependencies.

By default only the first **100000** positions are processed, so a report is
near-instant on datasets of any size; pass `--max 0` to use everything (or
`--max N` for a different cap).

By default it writes a single self-contained **interactive HTML dashboard**
(`datagen_report.html`). Open it in a browser and hover any bar or heatmap
square to read exact percentages and counts. It contains:

- a summary card row (games, positions, WDL, in-check %, capture-best-move %, K)
- an interactive per-piece **square-occupancy heatmap**: pick a piece (or click a
  mini-board) and every square shows that square's share of the piece's
  occurrences, with the number printed in the cell and an exact `square: % (count)`
  tooltip on hover
- game result, white-relative score distribution, score→WDL calibration (with the
  fitted sigmoid scale `K`), piece count, game length, best-move type, position
  ply and halfmove-clock charts — all hoverable

A percentage-based text summary is always printed too. `--no-html` skips the
file; `--max 0` processes all positions; `--no-check` skips the slower in-check
detection.
