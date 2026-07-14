#!/usr/bin/env python3
"""Analyze Devre self-play training data (see src/datagen.cpp for the format).

Reconstructs every scored position by replaying the stored moves, then reports
distributions useful for judging data quality:

  * record integrity / corruption check
  * game-result (WDL) balance and game-length distribution
  * white-relative score histogram and score -> WDL calibration (fitted sigmoid)
  * piece-count and per-position ply distributions
  * best-move type mix (quiet / capture / promotion) and in-check fraction
  * per-piece square-occupancy heatmap (--piece)

Pure stdlib; if matplotlib is installed, also writes PNG plots via --plots DIR.

Usage:
  python analyze_data.py DATA            # a .bin file or a directory of them
  python analyze_data.py DATA --piece wp # square heatmap for white pawns
  python analyze_data.py DATA --plots out --max 2000000
"""

import argparse
import glob
import math
import os
import struct
import sys
from collections import Counter

# ---- piece encoding (matches Devre) --------------------------------------
# 0..5 white PNBRQK, 8..13 black pnbrqk, -1 empty
PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING = range(6)
PIECE_LETTER = "PNBRQK??pnbrqk"
WDL_NAME = {0: "black-win", 1: "draw", 2: "white-win"}

# move type enum (move.h)
QUIET, DOUBLE_PUSH, KING_CASTLE, QUEEN_CASTLE, CAPTURE, EN_PASSANT = range(6)
KNIGHT_PROMO_BASE = 8  # move types >= this are promotions

HDR = struct.Struct("<Q16sBBHhBB")  # occupancy, pieces, stmEp, halfmove, fullmove, eval, wdl, extra
SM = struct.Struct("<Hh")           # move (u16), eval (i16)


def piece_type(p):
    return p & 7


def piece_color(p):
    return 1 if (p & 8) else 0


def sq_file(sq):
    return sq & 7


def sq_rank(sq):
    return sq >> 3


# ---- attack tables for in-check detection --------------------------------
def _knight_table():
    t = [0] * 64
    for s in range(64):
        r, f = s >> 3, s & 7
        bb = 0
        for dr, df in ((2, 1), (2, -1), (-2, 1), (-2, -1), (1, 2), (1, -2), (-1, 2), (-1, -2)):
            rr, ff = r + dr, f + df
            if 0 <= rr < 8 and 0 <= ff < 8:
                bb |= 1 << (rr * 8 + ff)
        t[s] = bb
    return t


def _king_table():
    t = [0] * 64
    for s in range(64):
        r, f = s >> 3, s & 7
        bb = 0
        for dr in (-1, 0, 1):
            for df in (-1, 0, 1):
                if dr == 0 and df == 0:
                    continue
                rr, ff = r + dr, f + df
                if 0 <= rr < 8 and 0 <= ff < 8:
                    bb |= 1 << (rr * 8 + ff)
        t[s] = bb
    return t


KNIGHT_ATT = _knight_table()
KING_ATT = _king_table()
BISHOP_DIRS = ((1, 1), (1, -1), (-1, 1), (-1, -1))
ROOK_DIRS = ((1, 0), (-1, 0), (0, 1), (0, -1))


def king_in_check(board, stm):
    """True if the side-to-move (stm: 0 white / 1 black) king is attacked."""
    ksq = -1
    king = KING | (8 if stm else 0)
    for s in range(64):
        if board[s] == king:
            ksq = s
            break
    if ksq < 0:
        return False
    enemy = 1 - stm
    kr, kf = ksq >> 3, ksq & 7

    # pawns: an enemy pawn attacks the king square from the forward diagonals
    pdr = 1 if enemy == 0 else -1  # white pawns attack upward (toward higher ranks)
    for df in (-1, 1):
        rr, ff = kr - pdr, kf + df
        if 0 <= rr < 8 and 0 <= ff < 8:
            if board[rr * 8 + ff] == (PAWN | (8 if enemy else 0)):
                return True

    ep_knight = KNIGHT | (8 if enemy else 0)
    kn = KNIGHT_ATT[ksq]
    b = kn
    while b:
        s = (b & -b).bit_length() - 1
        b &= b - 1
        if board[s] == ep_knight:
            return True

    ep_king = KING | (8 if enemy else 0)
    b = KING_ATT[ksq]
    while b:
        s = (b & -b).bit_length() - 1
        b &= b - 1
        if board[s] == ep_king:
            return True

    ebish = BISHOP | (8 if enemy else 0)
    equeen = QUEEN | (8 if enemy else 0)
    erook = ROOK | (8 if enemy else 0)
    for dr, df in BISHOP_DIRS:
        rr, ff = kr + dr, kf + df
        while 0 <= rr < 8 and 0 <= ff < 8:
            p = board[rr * 8 + ff]
            if p != -1:
                if p == ebish or p == equeen:
                    return True
                break
            rr += dr
            ff += df
    for dr, df in ROOK_DIRS:
        rr, ff = kr + dr, kf + df
        while 0 <= rr < 8 and 0 <= ff < 8:
            p = board[rr * 8 + ff]
            if p != -1:
                if p == erook or p == equeen:
                    return True
                break
            rr += dr
            ff += df
    return False


# ---- record parsing ------------------------------------------------------
def unpack_header(buf):
    occ, pieces, stm_ep, halfmove, fullmove, ev, wdl, _extra = HDR.unpack(buf)
    board = [-1] * 64
    idx = 0
    bb = occ
    while bb:
        sq = (bb & -bb).bit_length() - 1
        bb &= bb - 1
        nib = (pieces[idx >> 1] >> 4) if (idx & 1) else (pieces[idx >> 1] & 0xF)
        if nib == 6:       # white unmoved rook
            piece = ROOK
        elif nib == 14:    # black unmoved rook
            piece = ROOK | 8
        else:
            piece = nib
        board[sq] = piece
        idx += 1
    stm = 1 if (stm_ep & 0x80) else 0
    ep = stm_ep & 0x7F
    return {
        "board": board,
        "stm": stm,
        "ep": None if ep == 64 else ep,
        "halfmove": halfmove,
        "fullmove": fullmove,
        "eval": ev,
        "wdl": wdl,
    }


def apply_move(board, stm, move):
    """Apply a Devre-encoded move to the mailbox; return the new side to move.

    Assumes standard (non-FRC) castling, matching v1 datagen output.
    """
    frm = (move >> 10) & 63
    to = (move >> 4) & 63
    typ = move & 15
    piece = board[frm]

    if typ == KING_CASTLE:
        board[to] = piece
        board[frm] = -1
        rook = ROOK | (8 if stm else 0)
        rfrom = (7 if stm == 0 else 63)      # h1 / h8
        board[rfrom] = -1
        board[to - 1] = rook
    elif typ == QUEEN_CASTLE:
        board[to] = piece
        board[frm] = -1
        rook = ROOK | (8 if stm else 0)
        rfrom = (0 if stm == 0 else 56)      # a1 / a8
        board[rfrom] = -1
        board[to + 1] = rook
    elif typ == EN_PASSANT:
        board[to] = piece
        board[frm] = -1
        cap_sq = sq_rank(frm) * 8 + sq_file(to)
        board[cap_sq] = -1
    elif typ >= KNIGHT_PROMO_BASE:
        promoted = (KNIGHT + (typ & 3)) | (8 if stm else 0)
        board[frm] = -1
        board[to] = promoted
    else:  # QUIET, DOUBLE_PUSH, CAPTURE
        board[to] = piece
        board[frm] = -1
    return 1 - stm


def iter_games(paths):
    for path in paths:
        with open(path, "rb") as fh:
            data = fh.read()
        n = len(data)
        off = 0
        while off + 32 <= n:
            header = unpack_header(data[off:off + 32])
            off += 32
            moves = []
            ok = True
            while True:
                if off + 4 > n:
                    ok = False
                    break
                mv, ev = SM.unpack_from(data, off)
                off += 4
                if mv == 0:  # terminator
                    break
                moves.append((mv, ev))
            yield path, header, moves, ok


# ---- sigmoid fit ---------------------------------------------------------
def sigmoid(x):
    if x < -60:
        return 0.0
    if x > 60:
        return 1.0
    return 1.0 / (1.0 + math.exp(-x))


def fit_scale(evals, results):
    """Golden-section fit of K minimizing MSE of sigmoid(eval/K) vs result.

    results are in {0.0, 0.5, 1.0} (white POV). Returns (K, mse)."""
    if not evals:
        return None, None

    def mse(k):
        s = 0.0
        for e, r in zip(evals, results):
            s += (sigmoid(e / k) - r) ** 2
        return s / len(evals)

    lo, hi = 20.0, 1500.0
    gr = (math.sqrt(5) - 1) / 2
    c = hi - gr * (hi - lo)
    d = lo + gr * (hi - lo)
    fc, fd = mse(c), mse(d)
    for _ in range(60):
        if fc < fd:
            hi, d, fd = d, c, fc
            c = hi - gr * (hi - lo)
            fc = mse(c)
        else:
            lo, c, fc = c, d, fd
            d = lo + gr * (hi - lo)
            fd = mse(d)
    k = (lo + hi) / 2
    return k, mse(k)


# ---- main analysis -------------------------------------------------------
def parse_piece_arg(s):
    """'wp'/'bq'/'P'/'n' -> piece code, or None."""
    if not s:
        return None
    s = s.strip()
    if len(s) == 2 and s[0] in "wb":
        color = 0 if s[0] == "w" else 8
        t = "pnbrqk".index(s[1].lower())
        return color | t
    if len(s) == 1:
        return PIECE_LETTER.index(s) if s in PIECE_LETTER else None
    return None


def histogram(counter, width=48, lo=None, hi=None, step=1):
    if not counter:
        return []
    keys = sorted(counter)
    lo = keys[0] if lo is None else lo
    hi = keys[-1] if hi is None else hi
    mx = max(counter.values())
    lines = []
    for k in range(lo, hi + 1, step):
        v = counter.get(k, 0)
        bar = "#" * int(width * v / mx) if mx else ""
        lines.append(f"{k:6d} | {bar} {v}")
    return lines


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("data", help="a .bin file or directory of .bin files")
    ap.add_argument("--max", type=int, default=0, help="stop after N scored positions (0 = all)")
    ap.add_argument("--piece", default="", help="square-heatmap piece, e.g. wp bn wq")
    ap.add_argument("--no-check", action="store_true", help="skip (slower) in-check detection")
    ap.add_argument("--plots", default="", help="directory to write PNG plots (needs matplotlib)")
    ap.add_argument("--score-bin", type=int, default=50, help="score histogram bin size (cp)")
    args = ap.parse_args()

    if os.path.isdir(args.data):
        paths = sorted(glob.glob(os.path.join(args.data, "*.bin")))
    else:
        paths = sorted(glob.glob(args.data))
    if not paths:
        print(f"no data files matched: {args.data}", file=sys.stderr)
        return 1

    piece_code = parse_piece_arg(args.piece)

    total_bytes = sum(os.path.getsize(p) for p in paths)

    # accumulators
    n_games = 0
    n_positions = 0
    n_corrupt = 0
    wdl_counter = Counter()
    game_len = Counter()
    score_hist = Counter()
    piececount_hist = Counter()
    ply_positions = 0
    stm_counter = Counter()
    halfmove_hist = Counter()
    movetype_counter = Counter()  # quiet / capture / promo
    in_check = 0
    checked_positions = 0
    piece_squares = [0] * 64
    eval_samples = []
    result_samples = []
    calib_bins = {}  # bin -> [count, win_sum]

    bin_cp = max(1, args.score_bin)

    stop = False
    for path, header, moves, ok in iter_games(paths):
        if not ok:
            n_corrupt += 1
            continue
        wdl = header["wdl"]
        if wdl not in (0, 1, 2):
            n_corrupt += 1
            continue
        n_games += 1
        wdl_counter[wdl] += 1
        game_len[len(moves)] += 1
        # white-POV result for calibration
        white_result = {0: 0.0, 1: 0.5, 2: 1.0}[wdl]

        board = list(header["board"])
        stm = header["stm"]
        for ply, (mv, ev) in enumerate(moves):
            n_positions += 1
            ply_positions += ply
            stm_counter[stm] += 1

            # score histogram (white-relative cp)
            score_hist[bin_cp * round(ev / bin_cp)] += 1

            # calibration: white-relative eval -> white game result
            eval_samples.append(ev)
            result_samples.append(white_result)
            b = bin_cp * round(ev / bin_cp)
            slot = calib_bins.setdefault(b, [0, 0.0])
            slot[0] += 1
            slot[1] += white_result

            # piece count
            pc = sum(1 for x in board if x != -1)
            piececount_hist[pc] += 1

            # best-move type
            typ = mv & 15
            if typ >= KNIGHT_PROMO_BASE:
                movetype_counter["promotion"] += 1
            elif typ in (CAPTURE, EN_PASSANT):
                movetype_counter["capture"] += 1
            else:
                movetype_counter["quiet"] += 1

            # in-check
            if not args.no_check:
                checked_positions += 1
                if king_in_check(board, stm):
                    in_check += 1

            # piece square occupancy
            if piece_code is not None:
                for s in range(64):
                    if board[s] == piece_code:
                        piece_squares[s] += 1

            stm = apply_move(board, stm, mv)

            if args.max and n_positions >= args.max:
                stop = True
                break
        if stop:
            break

    # ---- report ----
    def pct(x, total):
        return f"{100.0 * x / total:.1f}%" if total else "n/a"

    print("=" * 64)
    print("Devre self-play data report")
    print("=" * 64)
    print(f"files            : {len(paths)}  ({total_bytes/1e6:.2f} MB)")
    print(f"games            : {n_games}")
    print(f"scored positions : {n_positions}")
    if n_games:
        print(f"bytes / position : {total_bytes / max(1,n_positions):.2f}")
        print(f"avg game length  : {n_positions / n_games:.1f} plies")
    print(f"corrupt records  : {n_corrupt}")

    print("\n-- game result (white POV) --")
    for k in (2, 1, 0):
        print(f"  {WDL_NAME[k]:10s}: {wdl_counter[k]:8d}  {pct(wdl_counter[k], n_games)}")

    print("\n-- side to move at scored positions --")
    print(f"  white: {stm_counter[0]}  {pct(stm_counter[0], n_positions)}")
    print(f"  black: {stm_counter[1]}  {pct(stm_counter[1], n_positions)}")

    print("\n-- best-move type --")
    tot_mt = sum(movetype_counter.values())
    for k in ("quiet", "capture", "promotion"):
        print(f"  {k:10s}: {movetype_counter[k]:8d}  {pct(movetype_counter[k], tot_mt)}")

    if not args.no_check:
        print("\n-- in-check positions --")
        print(f"  in check : {in_check}  {pct(in_check, checked_positions)}")

    print("\n-- piece count (total pieces on board) --")
    for line in histogram(piececount_hist, lo=2, hi=32):
        print("  " + line)

    print("\n-- game length (plies) histogram (bucketed by 10) --")
    len_buckets = Counter()
    for length, c in game_len.items():
        len_buckets[10 * (length // 10)] += c
    for line in histogram(len_buckets, step=10):
        print("  " + line)

    print("\n-- white-relative score distribution (cp) --")
    lo = min(score_hist) if score_hist else 0
    hi = max(score_hist) if score_hist else 0
    lo = max(lo, -1000)
    hi = min(hi, 1000)
    keys = sorted(k for k in score_hist if lo <= k <= hi)
    if keys:
        mx = max(score_hist[k] for k in keys)
        for k in range(lo, hi + 1, bin_cp):
            v = score_hist.get(k, 0)
            bar = "#" * int(48 * v / mx) if mx else ""
            print(f"  {k:6d} | {bar} {v}")
        clipped = n_positions - sum(score_hist[k] for k in keys)
        if clipped:
            print(f"  (|score| > 1000cp not shown: {clipped}  {pct(clipped, n_positions)})")

    # ---- score -> WDL calibration ----
    print("\n-- score -> WDL calibration (white POV) --")
    k, mse = fit_scale(eval_samples, result_samples)
    if k:
        print(f"  fitted sigmoid scale K = {k:.1f} cp   (P(win) = 1/(1+exp(-eval/K)))")
        print(f"  fit MSE                = {mse:.4f}")
        print(f"  {'eval(cp)':>10} {'n':>8} {'empirical':>10} {'sigmoid':>9}")
        for b in sorted(calib_bins):
            if b < -800 or b > 800:
                continue
            cnt, wsum = calib_bins[b]
            if cnt < 20:
                continue
            emp = wsum / cnt
            pred = sigmoid(b / k)
            print(f"  {b:10d} {cnt:8d} {emp:10.3f} {pred:9.3f}")

    # ---- piece square heatmap ----
    if piece_code is not None:
        total = sum(piece_squares)
        print(f"\n-- square occupancy heatmap for '{args.piece}' (total {total}) --")
        if total:
            mx = max(piece_squares)
            for r in range(7, -1, -1):
                row = []
                for f in range(8):
                    v = piece_squares[r * 8 + f]
                    shade = " .:-=+*#@"[min(8, int(8 * v / mx))] if mx else " "
                    row.append(shade)
                pctrow = 100.0 * sum(piece_squares[r * 8:r * 8 + 8]) / total
                print(f"  {r+1} {''.join(row)}   {pctrow:4.1f}%")
            print("    abcdefgh")

    # ---- optional plots ----
    if args.plots:
        try:
            _write_plots(args, calib_bins, k, score_hist, piececount_hist,
                         len_buckets, piece_squares, wdl_counter, bin_cp)
            print(f"\nplots written to {args.plots}/")
        except ImportError:
            print("\n--plots requested but matplotlib is not installed; skipping", file=sys.stderr)

    return 0


def _write_plots(args, calib_bins, k, score_hist, piececount_hist, len_buckets,
                 piece_squares, wdl_counter, bin_cp):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    os.makedirs(args.plots, exist_ok=True)

    # score histogram
    keys = sorted(x for x in score_hist if -1000 <= x <= 1000)
    if keys:
        fig, ax = plt.subplots(figsize=(8, 4))
        ax.bar(keys, [score_hist[x] for x in keys], width=bin_cp * 0.9)
        ax.set_title("white-relative score distribution")
        ax.set_xlabel("cp")
        ax.set_ylabel("positions")
        fig.tight_layout()
        fig.savefig(os.path.join(args.plots, "score_hist.png"), dpi=110)
        plt.close(fig)

    # calibration
    if k:
        bs = sorted(b for b in calib_bins if -800 <= b <= 800 and calib_bins[b][0] >= 20)
        emp = [calib_bins[b][1] / calib_bins[b][0] for b in bs]
        pred = [sigmoid(b / k) for b in bs]
        fig, ax = plt.subplots(figsize=(6, 5))
        ax.plot(bs, emp, "o", label="empirical")
        ax.plot(bs, pred, "-", label=f"sigmoid(eval/{k:.0f})")
        ax.set_title("score -> WDL calibration")
        ax.set_xlabel("white-relative eval (cp)")
        ax.set_ylabel("P(white win)")
        ax.legend()
        ax.grid(True, alpha=0.3)
        fig.tight_layout()
        fig.savefig(os.path.join(args.plots, "calibration.png"), dpi=110)
        plt.close(fig)

    # piece count
    if piececount_hist:
        pcs = sorted(piececount_hist)
        fig, ax = plt.subplots(figsize=(8, 4))
        ax.bar(pcs, [piececount_hist[x] for x in pcs])
        ax.set_title("piece count distribution")
        ax.set_xlabel("pieces on board")
        ax.set_ylabel("positions")
        fig.tight_layout()
        fig.savefig(os.path.join(args.plots, "piece_count.png"), dpi=110)
        plt.close(fig)

    # heatmap
    if sum(piece_squares):
        grid = [[piece_squares[r * 8 + f] for f in range(8)] for r in range(7, -1, -1)]
        fig, ax = plt.subplots(figsize=(5, 5))
        im = ax.imshow(grid, cmap="viridis")
        ax.set_title(f"square occupancy: {args.piece}")
        ax.set_xticks(range(8))
        ax.set_xticklabels(list("abcdefgh"))
        ax.set_yticks(range(8))
        ax.set_yticklabels(list("87654321"))
        fig.colorbar(im, ax=ax)
        fig.tight_layout()
        fig.savefig(os.path.join(args.plots, "piece_heatmap.png"), dpi=110)
        plt.close(fig)


if __name__ == "__main__":
    sys.exit(main())
