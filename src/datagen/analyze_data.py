#!/usr/bin/env python3
"""Analyze Devre self-play training data (see datagen.cpp for the format).

Reconstructs every scored position by replaying the stored moves, then reports
data-quality distributions. The easiest way to read the results is the plot
gallery: pass --plots and open the generated index.html.

  python analyze_data.py DATA                 # text summary (percentages)
  python analyze_data.py DATA --plots         # + gallery in ./analysis/index.html
  python analyze_data.py DATA --plots out      # + gallery in ./out/index.html
  python analyze_data.py DATA --max 2000000    # sample the first N positions (faster)

DATA is a .bin file or a directory of them. Plots need matplotlib
(pip install matplotlib); the text summary is pure stdlib.
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
PIECE_CODES = [0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13]
PIECE_NAMES = {
    0: "White pawn", 1: "White knight", 2: "White bishop", 3: "White rook",
    4: "White queen", 5: "White king", 8: "Black pawn", 9: "Black knight",
    10: "Black bishop", 11: "Black rook", 12: "Black queen", 13: "Black king",
}
WDL_NAME = {0: "black win", 1: "draw", 2: "white win"}

# move type enum (move.h)
QUIET, DOUBLE_PUSH, KING_CASTLE, QUEEN_CASTLE, CAPTURE, EN_PASSANT = range(6)
KNIGHT_PROMO_BASE = 8  # move types >= this are promotions

HDR = struct.Struct("<Q16sBBHhBB")  # occupancy, pieces, stmEp, halfmove, fullmove, eval, wdl, extra
SM = struct.Struct("<Hh")           # move (u16), eval (i16)


def piece_type(p):
    return p & 7


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
    king = KING | (8 if stm else 0)
    ksq = -1
    for s in range(64):
        if board[s] == king:
            ksq = s
            break
    if ksq < 0:
        return False
    enemy = 1 - stm
    kr, kf = ksq >> 3, ksq & 7

    pdr = 1 if enemy == 0 else -1
    for df in (-1, 1):
        rr, ff = kr - pdr, kf + df
        if 0 <= rr < 8 and 0 <= ff < 8:
            if board[rr * 8 + ff] == (PAWN | (8 if enemy else 0)):
                return True

    ep_knight = KNIGHT | (8 if enemy else 0)
    b = KNIGHT_ATT[ksq]
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
        "board": board, "stm": stm, "ep": None if ep == 64 else ep,
        "halfmove": halfmove, "fullmove": fullmove, "eval": ev, "wdl": wdl,
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
        board[7 if stm == 0 else 63] = -1
        board[to - 1] = rook
    elif typ == QUEEN_CASTLE:
        board[to] = piece
        board[frm] = -1
        rook = ROOK | (8 if stm else 0)
        board[0 if stm == 0 else 56] = -1
        board[to + 1] = rook
    elif typ == EN_PASSANT:
        board[to] = piece
        board[frm] = -1
        board[sq_rank(frm) * 8 + sq_file(to)] = -1
    elif typ >= KNIGHT_PROMO_BASE:
        board[frm] = -1
        board[to] = (KNIGHT + (typ & 3)) | (8 if stm else 0)
    else:  # QUIET, DOUBLE_PUSH, CAPTURE
        board[to] = piece
        board[frm] = -1
    return 1 - stm


def move_is_capture(move):
    typ = move & 15
    return typ in (CAPTURE, EN_PASSANT) or typ >= 12  # 12..15 are capture-promotions


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
            yield header, moves, ok


# ---- sigmoid fit ---------------------------------------------------------
def sigmoid(x):
    if x < -60:
        return 0.0
    if x > 60:
        return 1.0
    return 1.0 / (1.0 + math.exp(-x))


def fit_scale(evals, results):
    """Golden-section fit of K minimizing MSE of sigmoid(eval/K) vs result."""
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


def parse_piece_arg(s):
    if not s:
        return None
    s = s.strip()
    if len(s) == 2 and s[0] in "wb":
        return (0 if s[0] == "w" else 8) | "pnbrqk".index(s[1].lower())
    if len(s) == 1 and s in PIECE_LETTER:
        return PIECE_LETTER.index(s)
    return None


# ---- text helpers (percentages) ------------------------------------------
def bar_pct(counter, total, keys, width=40):
    """ASCII bars scaled to the largest percentage in the range."""
    maxp = max((100.0 * counter.get(k, 0) / total for k in keys), default=0.0) if total else 0.0
    lines = []
    for k in keys:
        p = 100.0 * counter.get(k, 0) / total if total else 0.0
        n = int(width * p / maxp) if maxp > 0 else 0
        lines.append(f"{k:6d} | {'#' * n} {p:5.1f}%")
    return lines


# ---- main analysis -------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("data", help="a .bin file or directory of .bin files")
    ap.add_argument("--plots", nargs="?", const="analysis", default=None,
                    help="write a plot gallery + index.html to this dir (default: ./analysis)")
    ap.add_argument("--max", type=int, default=0, help="stop after N scored positions (0 = all)")
    ap.add_argument("--piece", default="", help="text square-heatmap piece, e.g. wp bn wq")
    ap.add_argument("--no-check", action="store_true", help="skip (slower) in-check detection")
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
    bin_cp = max(1, args.score_bin)
    total_bytes = sum(os.path.getsize(p) for p in paths)

    # accumulators
    n_games = n_positions = n_corrupt = 0
    wdl_counter = Counter()
    game_len = Counter()
    score_hist = Counter()
    piececount_hist = Counter()
    stm_counter = Counter()
    halfmove_hist = Counter()
    ply_hist = Counter()
    movetype_counter = Counter()
    in_check = checked_positions = 0
    piece_squares = {p: [0] * 64 for p in PIECE_CODES}
    eval_samples = []
    result_samples = []
    calib = {}  # bin -> [count, white_result_sum]

    stop = False
    for header, moves, ok in iter_games(paths):
        if not ok or header["wdl"] not in (0, 1, 2):
            n_corrupt += 1
            continue
        wdl = header["wdl"]
        n_games += 1
        wdl_counter[wdl] += 1
        game_len[len(moves)] += 1
        white_result = {0: 0.0, 1: 0.5, 2: 1.0}[wdl]

        board = list(header["board"])
        stm = header["stm"]
        half = header["halfmove"]
        for ply, (mv, ev) in enumerate(moves):
            n_positions += 1
            stm_counter[stm] += 1
            ply_hist[5 * (ply // 5)] += 1
            halfmove_hist[half] += 1

            b = bin_cp * round(ev / bin_cp)
            score_hist[b] += 1
            eval_samples.append(ev)
            result_samples.append(white_result)
            slot = calib.setdefault(b, [0, 0.0])
            slot[0] += 1
            slot[1] += white_result

            piececount_hist[sum(1 for x in board if x != -1)] += 1

            typ = mv & 15
            if typ >= KNIGHT_PROMO_BASE:
                movetype_counter["promotion"] += 1
            elif typ in (CAPTURE, EN_PASSANT):
                movetype_counter["capture"] += 1
            else:
                movetype_counter["quiet"] += 1

            if not args.no_check:
                checked_positions += 1
                if king_in_check(board, stm):
                    in_check += 1

            for s in range(64):
                p = board[s]
                if p != -1:
                    piece_squares[p][s] += 1

            # halfmove clock for the next position
            reset = (piece_type(board[frm_of(mv)]) == PAWN) or move_is_capture(mv)
            half = 0 if reset else half + 1

            stm = apply_move(board, stm, mv)
            if args.max and n_positions >= args.max:
                stop = True
                break
        if stop:
            break

    k, mse = fit_scale(eval_samples, result_samples)

    stats = {
        "paths": paths, "total_bytes": total_bytes, "n_games": n_games,
        "n_positions": n_positions, "n_corrupt": n_corrupt, "wdl": wdl_counter,
        "game_len": game_len, "score_hist": score_hist, "piececount": piececount_hist,
        "stm": stm_counter, "halfmove": halfmove_hist, "ply": ply_hist,
        "movetype": movetype_counter, "in_check": in_check,
        "checked": checked_positions, "piece_squares": piece_squares,
        "calib": calib, "k": k, "mse": mse, "bin_cp": bin_cp,
    }

    print_text_report(stats, piece_code, args.piece)

    if args.plots is not None:
        try:
            write_gallery(stats, args.plots)
            print(f"\nplot gallery written — open {os.path.join(args.plots, 'index.html')}")
        except ImportError:
            print("\n--plots needs matplotlib: pip install matplotlib", file=sys.stderr)
            return 2
    else:
        print("\ntip: re-run with --plots for readable charts (open analysis/index.html)")
    return 0


def frm_of(move):
    return (move >> 10) & 63


# ---- text report ---------------------------------------------------------
def print_text_report(s, piece_code, piece_arg):
    ng = s["n_games"]
    npos = s["n_positions"]

    def pct(x, tot):
        return f"{100.0 * x / tot:.1f}%" if tot else "n/a"

    print("=" * 60)
    print("Devre self-play data report")
    print("=" * 60)
    print(f"files            : {len(s['paths'])}  ({s['total_bytes']/1e6:.2f} MB)")
    print(f"games            : {ng}")
    print(f"scored positions : {npos}")
    if npos:
        print(f"bytes / position : {s['total_bytes'] / npos:.2f}")
    if ng:
        print(f"avg game length  : {npos / ng:.1f} plies")
    print(f"corrupt records  : {s['n_corrupt']}")

    print("\n-- game result (white POV, % of games) --")
    for kk in (2, 1, 0):
        print(f"  {WDL_NAME[kk]:10s}: {pct(s['wdl'][kk], ng)}")

    print("\n-- side to move (% of positions) --")
    print(f"  white: {pct(s['stm'][0], npos)}   black: {pct(s['stm'][1], npos)}")

    print("\n-- best-move type (% of positions) --")
    tot_mt = sum(s["movetype"].values())
    for kk in ("quiet", "capture", "promotion"):
        print(f"  {kk:10s}: {pct(s['movetype'][kk], tot_mt)}")

    if s["checked"]:
        print("\n-- in-check positions --")
        print(f"  {pct(s['in_check'], s['checked'])}")

    if s["k"]:
        print("\n-- score -> WDL calibration --")
        print(f"  fitted sigmoid scale K = {s['k']:.1f} cp   (P(win) = 1/(1+exp(-eval/K)))")
        print(f"  fit MSE                = {s['mse']:.4f}")

    print("\n-- piece count on board (% of positions) --")
    for line in bar_pct(s["piececount"], npos, range(2, 33)):
        print("  " + line)

    print(f"\n-- white-relative score distribution (% of positions, bin {s['bin_cp']}cp) --")
    keys = list(range(-1000, 1001, s["bin_cp"]))
    for line in bar_pct(s["score_hist"], npos, keys):
        print("  " + line)
    shown = sum(s["score_hist"].get(k, 0) for k in keys)
    if npos - shown:
        print(f"  (|score| > 1000cp: {pct(npos - shown, npos)})")

    if piece_code is not None:
        squares = s["piece_squares"][piece_code]
        tot = sum(squares)
        print(f"\n-- square occupancy for '{piece_arg}' ({PIECE_NAMES.get(piece_code,'?')}), % of its occurrences --")
        if tot:
            mx = max(squares)
            for r in range(7, -1, -1):
                row = "".join(" .:-=+*#@"[min(8, int(8 * squares[r * 8 + f] / mx))] if mx else " " for f in range(8))
                rowpct = 100.0 * sum(squares[r * 8:r * 8 + 8]) / tot
                print(f"  {r+1} {row}   {rowpct:4.1f}%")
            print("    abcdefgh")


# ---- plot gallery --------------------------------------------------------
def write_gallery(s, out_dir):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    os.makedirs(out_dir, exist_ok=True)
    npos = s["n_positions"]
    ng = s["n_games"]
    plots = []  # (filename, caption)

    def save(fig, name, caption):
        fig.tight_layout()
        fig.savefig(os.path.join(out_dir, name), dpi=110)
        plt.close(fig)
        plots.append((name, caption))

    def pct_of(counter, total, keys):
        return [100.0 * counter.get(k, 0) / total if total else 0.0 for k in keys]

    # WDL
    fig, ax = plt.subplots(figsize=(5, 4))
    labels = [WDL_NAME[2], WDL_NAME[1], WDL_NAME[0]]
    vals = [100.0 * s["wdl"][k] / ng if ng else 0 for k in (2, 1, 0)]
    bars = ax.bar(labels, vals, color=["#4c9f70", "#9aa0a6", "#c0504d"])
    for b, v in zip(bars, vals):
        ax.text(b.get_x() + b.get_width() / 2, v, f"{v:.1f}%", ha="center", va="bottom")
    ax.set_ylabel("% of games")
    ax.set_title("Game result (white POV)")
    save(fig, "wdl.png", "Result balance. Very high draw rates or a large white/black skew suggest adjudication or opening bias.")

    # score distribution
    keys = list(range(-1000, 1001, s["bin_cp"]))
    fig, ax = plt.subplots(figsize=(8, 4))
    ax.bar(keys, pct_of(s["score_hist"], npos, keys), width=s["bin_cp"] * 0.9, color="#4472c4")
    ax.set_xlabel("white-relative eval (cp)")
    ax.set_ylabel("% of positions")
    ax.set_title("Score distribution")
    save(fig, "score_hist.png", "Should peak near 0 and taper smoothly. Spikes at the adjudication edges (±1000cp) mean many games ended there.")

    # calibration
    if s["k"]:
        bs = sorted(b for b in s["calib"] if -800 <= b <= 800 and s["calib"][b][0] >= 20)
        if bs:
            emp = [s["calib"][b][1] / s["calib"][b][0] for b in bs]
            pred = [sigmoid(b / s["k"]) for b in bs]
            fig, ax = plt.subplots(figsize=(6, 5))
            ax.plot(bs, emp, "o", label="empirical", color="#4472c4")
            ax.plot(bs, pred, "-", label=f"sigmoid(eval/{s['k']:.0f})", color="#c0504d")
            ax.set_xlabel("white-relative eval (cp)")
            ax.set_ylabel("P(white win)")
            ax.set_title(f"Score to WDL calibration (K={s['k']:.0f}cp, MSE={s['mse']:.4f})")
            ax.legend()
            ax.grid(True, alpha=0.3)
            save(fig, "calibration.png", "Empirical win rate vs the fitted sigmoid. Close agreement means evals and results are consistent; K is the cp-per-winrate scale.")

    # game length
    lb = Counter()
    for length, c in s["game_len"].items():
        lb[10 * (length // 10)] += c
    if lb:
        keys = list(range(0, max(lb) + 10, 10))
        fig, ax = plt.subplots(figsize=(8, 4))
        ax.bar(keys, pct_of(lb, ng, keys), width=9, color="#70ad47")
        ax.set_xlabel("game length (plies, bucketed by 10)")
        ax.set_ylabel("% of games")
        ax.set_title("Game length")
        save(fig, "game_length.png", "How long games run. A wall at the 400-ply cap or a big spike at short lengths is worth a look.")

    # piece count
    keys = list(range(2, 33))
    fig, ax = plt.subplots(figsize=(8, 4))
    ax.bar(keys, pct_of(s["piececount"], npos, keys), color="#8064a2")
    ax.set_xlabel("pieces on board")
    ax.set_ylabel("% of positions")
    ax.set_title("Piece count distribution")
    save(fig, "piece_count.png", "Material coverage from openings (32) to endgames. A pipeline heavy on either extreme trains a lopsided net.")

    # ply distribution
    if s["ply"]:
        keys = list(range(0, max(s["ply"]) + 5, 5))
        fig, ax = plt.subplots(figsize=(8, 4))
        ax.bar(keys, pct_of(s["ply"], npos, keys), width=4.5, color="#4472c4")
        ax.set_xlabel("ply within game (bucketed by 5)")
        ax.set_ylabel("% of positions")
        ax.set_title("Position ply distribution")
        save(fig, "ply.png", "Where in the game scored positions come from; skew toward early plies means short games dominate.")

    # halfmove clock
    if s["halfmove"]:
        keys = list(range(0, 101, 2))
        fig, ax = plt.subplots(figsize=(8, 4))
        ax.bar(keys, pct_of(s["halfmove"], npos, keys), width=1.8, color="#9aa0a6")
        ax.set_xlabel("halfmove clock")
        ax.set_ylabel("% of positions")
        ax.set_title("Halfmove-clock distribution")
        save(fig, "halfmove.png", "50-move-rule counter. A long tail toward 100 indicates many shuffly, drawish endgames.")

    # best-move type
    fig, ax = plt.subplots(figsize=(5, 4))
    tot_mt = sum(s["movetype"].values())
    order = ["quiet", "capture", "promotion"]
    vals = [100.0 * s["movetype"][k] / tot_mt if tot_mt else 0 for k in order]
    bars = ax.bar(order, vals, color=["#4472c4", "#ed7d31", "#70ad47"])
    for b, v in zip(bars, vals):
        ax.text(b.get_x() + b.get_width() / 2, v, f"{v:.1f}%", ha="center", va="bottom")
    ax.set_ylabel("% of positions")
    ax.set_title("Best-move type")
    save(fig, "move_type.png", "Share of positions whose best move is a capture/promotion. Filtering tactical best moves is a common training step.")

    # per-piece heatmaps (within-piece %)
    fig, axes = plt.subplots(2, 6, figsize=(15, 5.5))
    for ax, code in zip(axes.flat, PIECE_CODES):
        squares = s["piece_squares"][code]
        tot = sum(squares)
        grid = [[(100.0 * squares[r * 8 + f] / tot if tot else 0.0) for f in range(8)] for r in range(7, -1, -1)]
        ax.imshow(grid, cmap="viridis", vmin=0)
        ax.set_title(f"{PIECE_LETTER[code]}  {tot}", fontsize=9)
        ax.set_xticks([])
        ax.set_yticks([])
    fig.suptitle("Square occupancy per piece (brighter = larger share of that piece's occurrences)")
    save(fig, "piece_heatmaps.png", "Where each piece spends its time (a1 bottom-left). Pawns should never sit on ranks 1/8; kings should cluster on the back ranks / castled squares.")

    _write_index_html(s, out_dir, plots)


def _write_index_html(s, out_dir, plots):
    ng = s["n_games"]
    npos = s["n_positions"]
    tot_mt = sum(s["movetype"].values()) or 1

    def p(x, tot):
        return f"{100.0 * x / tot:.1f}%" if tot else "n/a"

    rows = [
        ("Files", f"{len(s['paths'])} ({s['total_bytes']/1e6:.2f} MB)"),
        ("Games", f"{ng:,}"),
        ("Scored positions", f"{npos:,}"),
        ("Bytes / position", f"{s['total_bytes']/npos:.2f}" if npos else "n/a"),
        ("Avg game length", f"{npos/ng:.1f} plies" if ng else "n/a"),
        ("Corrupt records", str(s["n_corrupt"])),
        ("White win / draw / black win",
         f"{p(s['wdl'][2], ng)} / {p(s['wdl'][1], ng)} / {p(s['wdl'][0], ng)}"),
        ("Side to move (w/b)", f"{p(s['stm'][0], npos)} / {p(s['stm'][1], npos)}"),
        ("Best move quiet/capture/promo",
         f"{p(s['movetype']['quiet'], tot_mt)} / {p(s['movetype']['capture'], tot_mt)} / {p(s['movetype']['promotion'], tot_mt)}"),
        ("In-check positions", p(s["in_check"], s["checked"]) if s["checked"] else "skipped"),
        ("Fitted sigmoid K", f"{s['k']:.0f} cp (MSE {s['mse']:.4f})" if s["k"] else "n/a"),
    ]
    stat_rows = "\n".join(f"<tr><th>{k}</th><td>{v}</td></tr>" for k, v in rows)
    cards = "\n".join(
        f'<figure><img src="{name}" alt="{name}"><figcaption>{cap}</figcaption></figure>'
        for name, cap in plots
    )

    html = f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Devre datagen report</title>
<style>
 :root {{ color-scheme: light dark; }}
 body {{ font-family: system-ui, sans-serif; margin: 0 auto; max-width: 1100px; padding: 24px; line-height: 1.5; }}
 h1 {{ margin: 0 0 4px; }}
 .sub {{ color: #888; margin-bottom: 20px; }}
 table {{ border-collapse: collapse; margin-bottom: 28px; }}
 th, td {{ text-align: left; padding: 4px 14px 4px 0; border-bottom: 1px solid #8883; }}
 th {{ font-weight: 600; white-space: nowrap; }}
 .grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(340px, 1fr)); gap: 22px; }}
 figure {{ margin: 0; border: 1px solid #8884; border-radius: 8px; padding: 12px; }}
 img {{ width: 100%; height: auto; display: block; }}
 figcaption {{ font-size: 0.86rem; color: #999; margin-top: 8px; }}
</style></head><body>
<h1>Devre self-play data report</h1>
<div class="sub">{ng:,} games &middot; {npos:,} scored positions</div>
<table>{stat_rows}</table>
<div class="grid">{cards}</div>
</body></html>"""
    with open(os.path.join(out_dir, "index.html"), "w", encoding="utf-8") as fh:
        fh.write(html)


if __name__ == "__main__":
    sys.exit(main())
