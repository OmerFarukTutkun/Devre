#!/usr/bin/env python3
"""Analyze Devre self-play training data (see datagen.cpp for the format).

Reconstructs every scored position by replaying the stored moves, then writes a
self-contained interactive HTML dashboard (pure stdlib, no matplotlib): open it
in a browser and hover any bar or heatmap square to read exact percentages and
counts. A percentage text summary is also printed to the terminal.

  python analyze_data.py DATA                  # -> datagen_report.html + text
  python analyze_data.py DATA --html out.html  # choose the output path
  python analyze_data.py DATA --no-html        # text summary only
  python analyze_data.py DATA --max 2000000    # sample first N positions (faster)

DATA is a .bin file or a directory of them.
"""

import argparse
import glob
import json
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
PIECE_GLYPH = {0: "♙", 1: "♘", 2: "♗", 3: "♖", 4: "♕", 5: "♔",
               8: "♟", 9: "♞", 10: "♝", 11: "♜", 12: "♛", 13: "♚"}
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


def frm_of(move):
    return (move >> 10) & 63


def move_is_capture(move):
    typ = move & 15
    return typ in (CAPTURE, EN_PASSANT) or typ >= 12  # 12..15 are capture-promotions


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
        board[7 if stm == 0 else 63] = -1
        board[to - 1] = ROOK | (8 if stm else 0)
    elif typ == QUEEN_CASTLE:
        board[to] = piece
        board[frm] = -1
        board[0 if stm == 0 else 56] = -1
        board[to + 1] = ROOK | (8 if stm else 0)
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


# ---- main analysis -------------------------------------------------------
def analyze(paths, max_positions=0, do_check=True, bin_cp=50):
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
    calib = {}

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
            halfmove_hist[min(half, 100)] += 1

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

            if do_check:
                checked_positions += 1
                if king_in_check(board, stm):
                    in_check += 1

            for s in range(64):
                p = board[s]
                if p != -1:
                    piece_squares[p][s] += 1

            reset = (piece_type(board[frm_of(mv)]) == PAWN) or move_is_capture(mv)
            half = 0 if reset else half + 1

            stm = apply_move(board, stm, mv)
            if max_positions and n_positions >= max_positions:
                stop = True
                break
        if stop:
            break

    k, mse = fit_scale(eval_samples, result_samples)
    return {
        "n_games": n_games, "n_positions": n_positions, "n_corrupt": n_corrupt,
        "wdl": wdl_counter, "game_len": game_len, "score_hist": score_hist,
        "piececount": piececount_hist, "stm": stm_counter, "halfmove": halfmove_hist,
        "ply": ply_hist, "movetype": movetype_counter, "in_check": in_check,
        "checked": checked_positions, "piece_squares": piece_squares,
        "calib": calib, "k": k, "mse": mse, "bin_cp": bin_cp,
    }


# ---- text report ---------------------------------------------------------
def print_text_report(s, total_bytes, n_files):
    ng, npos = s["n_games"], s["n_positions"]

    def pct(x, tot):
        return f"{100.0 * x / tot:.1f}%" if tot else "n/a"

    print("=" * 60)
    print("Devre self-play data report")
    print("=" * 60)
    print(f"files            : {n_files}  ({total_bytes/1e6:.2f} MB)")
    print(f"games            : {ng}")
    print(f"scored positions : {npos}")
    if npos:
        print(f"bytes / position : {total_bytes / npos:.2f}")
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
        print(f"\n-- in-check positions --\n  {pct(s['in_check'], s['checked'])}")
    if s["k"]:
        print("\n-- score -> WDL calibration --")
        print(f"  fitted sigmoid scale K = {s['k']:.1f} cp   (P(win) = 1/(1+exp(-eval/K)))")
        print(f"  fit MSE                = {s['mse']:.4f}")


# ---- interactive dashboard -----------------------------------------------
def build_dashboard_data(s, total_bytes, n_files):
    ng, npos = s["n_games"], s["n_positions"]
    binc = s["bin_cp"]

    def series(counter, keys):
        return [[k, counter.get(k, 0)] for k in keys]

    score_keys = list(range(-1000, 1001, binc))
    score_items = series(s["score_hist"], score_keys)
    score_overflow = npos - sum(c for _, c in score_items)

    calib = []
    for b in sorted(s["calib"]):
        if -1000 <= b <= 1000:
            cnt, wsum = s["calib"][b]
            if cnt >= 1:
                calib.append([b, wsum / cnt, sigmoid(b / s["k"]) if s["k"] else 0.0, cnt])

    len_buckets = Counter()
    for length, c in s["game_len"].items():
        len_buckets[10 * (length // 10)] += c
    len_keys = list(range(0, (max(len_buckets) if len_buckets else 0) + 10, 10))

    ply_keys = list(range(0, (max(s["ply"]) if s["ply"] else 0) + 5, 5))
    half_keys = list(range(0, 101, 2))
    half_bucketed = Counter()
    for hm, c in s["halfmove"].items():
        half_bucketed[2 * (hm // 2)] += c

    pieces = []
    for code in PIECE_CODES:
        cells = s["piece_squares"][code]
        pieces.append({"code": code, "letter": PIECE_LETTER[code], "glyph": PIECE_GLYPH[code],
                       "name": PIECE_NAMES[code], "total": sum(cells), "cells": cells})

    return {
        "files": n_files, "bytes": total_bytes, "games": ng, "positions": npos,
        "corrupt": s["n_corrupt"], "avglen": (npos / ng) if ng else 0,
        "bytesPerPos": (total_bytes / npos) if npos else 0,
        "wdl": {"white": s["wdl"][2], "draw": s["wdl"][1], "black": s["wdl"][0]},
        "stm": {"white": s["stm"][0], "black": s["stm"][1]},
        "moveType": {"quiet": s["movetype"]["quiet"], "capture": s["movetype"]["capture"],
                     "promotion": s["movetype"]["promotion"]},
        "inCheck": s["in_check"], "checked": s["checked"],
        "k": s["k"], "mse": s["mse"], "binCp": binc,
        "scoreHist": {"items": score_items, "overflow": score_overflow},
        "calib": calib,
        "gameLen": series(len_buckets, len_keys),
        "ply": series(s["ply"], ply_keys),
        "halfmove": series(half_bucketed, half_keys),
        "pieceCount": series(s["piececount"], list(range(2, 33))),
        "pieces": pieces,
    }


def write_dashboard(data, path):
    html = DASHBOARD_TEMPLATE.replace("/*DATA_JSON*/", json.dumps(data, separators=(",", ":")))
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(html)


DASHBOARD_TEMPLATE = r"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Devre datagen dashboard</title>
<style>
 :root{color-scheme:light dark;--bg:#fff;--fg:#1a1a1a;--muted:#777;--card:#f6f7f9;--line:#0002;--accent:#4472c4;}
 @media (prefers-color-scheme:dark){:root{--bg:#15171b;--fg:#e6e6e6;--muted:#9aa0a6;--card:#1e2126;--line:#fff2;}}
 *{box-sizing:border-box}
 body{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--fg);margin:0;padding:24px;line-height:1.5;}
 .wrap{max-width:1180px;margin:0 auto;}
 h1{margin:0 0 2px;font-size:1.5rem;}
 h2{font-size:1.05rem;margin:0 0 10px;}
 .sub{color:var(--muted);margin-bottom:22px;}
 .cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px;margin-bottom:28px;}
 .card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px 14px;}
 .card .v{font-size:1.35rem;font-weight:650;}
 .card .l{color:var(--muted);font-size:.8rem;margin-top:2px;}
 .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(360px,1fr));gap:20px;}
 .panel{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px;}
 .panel .hint{color:var(--muted);font-size:.82rem;margin-top:8px;}
 svg{width:100%;height:auto;display:block;overflow:visible;}
 .axis{stroke:var(--line);}
 .axis-text{fill:var(--muted);font-size:11px;}
 .bar{fill:var(--accent);transition:opacity .1s;}
 .bar:hover{opacity:.7;cursor:pointer;}
 .full{grid-column:1/-1;}
 .heatwrap{display:flex;gap:26px;flex-wrap:wrap;align-items:flex-start;}
 .piecebtns{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:12px;}
 .pb{font-size:.85rem;padding:6px 9px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--fg);cursor:pointer;display:flex;gap:6px;align-items:center;}
 .pb .g{font-size:1.15rem;line-height:1;}
 .pb.active{border-color:var(--accent);outline:2px solid var(--accent);}
 .pb .c{color:var(--muted);font-size:.75rem;}
 .board{display:grid;grid-template-columns:repeat(8,1fr);width:min(430px,86vw);aspect-ratio:1;border:1px solid var(--line);}
 .cell{position:relative;display:flex;align-items:center;justify-content:center;font-size:.72rem;font-variant-numeric:tabular-nums;}
 .cell:hover{outline:2px solid #fff8;outline-offset:-2px;cursor:default;}
 .ranks{display:flex;flex-direction:column;justify-content:space-around;color:var(--muted);font-size:.72rem;}
 .files{display:grid;grid-template-columns:repeat(8,1fr);color:var(--muted);font-size:.72rem;text-align:center;margin-top:2px;}
 .miniwrap{display:grid;grid-template-columns:repeat(6,1fr);gap:10px;margin-top:6px;}
 .mini{cursor:pointer;text-align:center;}
 .mini .mb{display:grid;grid-template-columns:repeat(8,1fr);aspect-ratio:1;border:1px solid var(--line);}
 .mini .mc{width:100%;height:100%;}
 .mini .ml{font-size:.72rem;color:var(--muted);margin-top:3px;}
 .mini.active{outline:2px solid var(--accent);border-radius:4px;}
 #tip{position:fixed;pointer-events:none;background:#000d;color:#fff;padding:6px 9px;border-radius:6px;font-size:.78rem;z-index:9;white-space:nowrap;box-shadow:0 2px 8px #0006;}
 #tip[hidden]{display:none;}
 .legend{display:flex;align-items:center;gap:8px;color:var(--muted);font-size:.78rem;margin-top:10px;}
 .legbar{height:12px;width:160px;border-radius:3px;background:linear-gradient(90deg,rgb(68,1,84),rgb(59,82,139),rgb(33,145,140),rgb(94,201,98),rgb(253,231,37));}
</style></head><body><div class="wrap" id="app"></div><div id="tip" hidden></div>
<script>
const DATA = /*DATA_JSON*/;
const app = document.getElementById('app'), tip = document.getElementById('tip');
const SVGNS='http://www.w3.org/2000/svg';
function h(t,a,k){const e=document.createElement(t);if(a)for(const n in a){if(n==='class')e.className=a[n];else if(n==='html')e.innerHTML=a[n];else if(n==='text')e.textContent=a[n];else e.setAttribute(n,a[n]);}if(k!=null)[].concat(k).forEach(c=>c!=null&&e.appendChild(typeof c==='string'?document.createTextNode(c):c));return e;}
function sv(t,a){const e=document.createElementNS(SVGNS,t);for(const n in a)e.setAttribute(n,a[n]);return e;}
function fmt(n){return n.toLocaleString('en-US');}
function showTip(html,ev){tip.innerHTML=html;tip.hidden=false;moveTip(ev);}
function moveTip(ev){tip.style.left=(ev.clientX+14)+'px';tip.style.top=(ev.clientY+14)+'px';}
function hideTip(){tip.hidden=true;}

const VIR=[[68,1,84],[59,82,139],[33,145,140],[94,201,98],[253,231,37]];
function viridis(t){t=Math.max(0,Math.min(1,t));const s=t*4,i=Math.min(3,Math.floor(s)),f=s-i,a=VIR[i],b=VIR[i+1];return `rgb(${Math.round(a[0]+(b[0]-a[0])*f)},${Math.round(a[1]+(b[1]-a[1])*f)},${Math.round(a[2]+(b[2]-a[2])*f)})`;}

// ---- summary cards ----
function pct(x,t){return t? (100*x/t).toFixed(1)+'%':'n/a';}
function cards(){
  const wdl=DATA.wdl, mt=DATA.moveType, mtT=mt.quiet+mt.capture+mt.promotion;
  const items=[
    ['Games',fmt(DATA.games)],['Scored positions',fmt(DATA.positions)],
    ['Bytes / position',DATA.positions?DATA.bytesPerPos.toFixed(2):'n/a'],
    ['Avg game length',DATA.games?DATA.avglen.toFixed(1)+' plies':'n/a'],
    ['Corrupt records',fmt(DATA.corrupt)],
    ['Draw rate',pct(wdl.draw,DATA.games)],
    ['White / black win',pct(wdl.white,DATA.games)+' / '+pct(wdl.black,DATA.games)],
    ['Capture best-move',pct(mt.capture,mtT)],
    ['In-check',DATA.checked?pct(DATA.inCheck,DATA.checked):'n/a'],
    ['Sigmoid K',DATA.k?DATA.k.toFixed(0)+' cp':'n/a'],
  ];
  const box=h('div',{class:'cards'});
  items.forEach(([l,v])=>box.appendChild(h('div',{class:'card'},[h('div',{class:'v',text:v}),h('div',{class:'l',text:l})])));
  return box;
}

// ---- bar chart (percent of total) ----
function barChart(items,total,opt){
  opt=opt||{};
  const W=640,H=270,ml=44,mr=12,mt=12,mb=34,pw=W-ml-mr,ph=H-mt-mb;
  const s=sv('svg',{viewBox:`0 0 ${W} ${H}`});
  const maxPct=Math.max(0.0001,...items.map(d=>total?100*d[1]/total:0));
  for(let g=0;g<=4;g++){const y=mt+ph*(1-g/4),val=maxPct*g/4;
    s.appendChild(sv('line',{class:'axis',x1:ml,y1:y,x2:W-mr,y2:y,'stroke-opacity':g?0.5:1}));
    const tx=sv('text',{class:'axis-text',x:ml-6,y:y+3,'text-anchor':'end'});tx.textContent=val.toFixed(val<1?1:0)+'%';s.appendChild(tx);}
  const n=items.length,bw=pw/n,step=Math.max(1,Math.ceil(n/12));
  items.forEach((d,i)=>{
    const p=total?100*d[1]/total:0,bh=ph*p/maxPct,x=ml+i*bw,y=mt+ph-bh;
    const r=sv('rect',{class:'bar',x:x+bw*0.12,y:y,width:bw*0.76,height:Math.max(0,bh)});
    const label=opt.xfmt?opt.xfmt(d[0]):d[0];
    r.addEventListener('mouseenter',ev=>showTip(`<b>${label}</b><br>${p.toFixed(2)}%<br>${fmt(d[1])} ${opt.unit||'positions'}`,ev));
    r.addEventListener('mousemove',moveTip);r.addEventListener('mouseleave',hideTip);
    s.appendChild(r);
    if(i%step===0){const tx=sv('text',{class:'axis-text',x:x+bw/2,y:H-mb+16,'text-anchor':'middle'});tx.textContent=label;s.appendChild(tx);}
  });
  return s;
}

// ---- calibration ----
function calibChart(){
  const pts=DATA.calib;const W=640,H=340,ml=44,mr=12,mt=12,mb=34,pw=W-ml-mr,ph=H-mt-mb;
  const s=sv('svg',{viewBox:`0 0 ${W} ${H}`});
  const xmin=-1000,xmax=1000,X=v=>ml+pw*(v-xmin)/(xmax-xmin),Y=v=>mt+ph*(1-v);
  for(let g=0;g<=4;g++){const y=Y(g/4);s.appendChild(sv('line',{class:'axis',x1:ml,y1:y,x2:W-mr,y2:y,'stroke-opacity':g?0.4:1}));
    const tx=sv('text',{class:'axis-text',x:ml-6,y:y+3,'text-anchor':'end'});tx.textContent=(g/4).toFixed(2);s.appendChild(tx);}
  [-1000,-500,0,500,1000].forEach(v=>{const tx=sv('text',{class:'axis-text',x:X(v),y:H-mb+16,'text-anchor':'middle'});tx.textContent=v;s.appendChild(tx);});
  if(DATA.k){let d='';pts.forEach((p,i)=>{d+=(i?'L':'M')+X(p[0])+' '+Y(p[2]);});s.appendChild(sv('path',{d:d,fill:'none',stroke:'#c0504d','stroke-width':2}));}
  pts.forEach(p=>{const c=sv('circle',{cx:X(p[0]),cy:Y(p[1]),r:3.2,fill:'#4472c4'});
    c.addEventListener('mouseenter',ev=>showTip(`<b>eval ${p[0]} cp</b><br>empirical ${(p[1]*100).toFixed(1)}%<br>sigmoid ${(p[2]*100).toFixed(1)}%<br>${fmt(p[3])} positions`,ev));
    c.addEventListener('mousemove',moveTip);c.addEventListener('mouseleave',hideTip);s.appendChild(c);});
  return s;
}

// ---- panels ----
function panel(title,node,hint,full){
  const p=h('div',{class:'panel'+(full?' full':'')},[h('h2',{text:title}),node]);
  if(hint)p.appendChild(h('div',{class:'hint',text:hint}));
  return p;
}

// ---- heatmaps ----
function sqName(sq){return 'abcdefgh'[sq&7]+(1+(sq>>3));}
function bigBoard(piece){
  const cells=piece.cells,total=piece.total,mx=Math.max(1,...cells);
  const board=h('div',{class:'board'});
  for(let r=7;r>=0;r--)for(let f=0;f<8;f++){
    const sq=r*8+f,v=cells[sq],p=total?100*v/total:0,t=v/mx;
    const cell=h('div',{class:'cell'});
    cell.style.background=v?viridis(t):'var(--card)';
    cell.style.color=t>0.55?'#111':'#eee';
    cell.textContent=p>=0.05?p.toFixed(1):'';
    cell.addEventListener('mouseenter',ev=>showTip(`<b>${sqName(sq)}</b><br>${p.toFixed(2)}% of ${piece.name.toLowerCase()}s<br>${fmt(v)} occurrences`,ev));
    cell.addEventListener('mousemove',moveTip);cell.addEventListener('mouseleave',hideTip);
    board.appendChild(cell);
  }
  const ranks=h('div',{class:'ranks'});for(let r=8;r>=1;r--)ranks.appendChild(h('div',{text:r}));
  const files=h('div',{class:'files'});'abcdefgh'.split('').forEach(f=>files.appendChild(h('div',{text:f})));
  return h('div',{},[h('div',{style:'display:flex;gap:6px'},[ranks,h('div',{},[board,files])]),
    h('div',{class:'legend'},[h('span',{text:'0%'}),h('div',{class:'legbar'}),h('span',{text:'max'}),h('span',{class:'',text:'  (hover a square for exact numbers)'})])]);
}
function miniBoard(piece,onSelect){
  const cells=piece.cells,mx=Math.max(1,...cells);
  const mb=h('div',{class:'mb'});
  for(let r=7;r>=0;r--)for(let f=0;f<8;f++){const sq=r*8+f,v=cells[sq];const c=h('div',{class:'mc'});c.style.background=v?viridis(v/mx):'var(--card)';mb.appendChild(c);}
  const wrap=h('div',{class:'mini'},[mb,h('div',{class:'ml',html:piece.glyph+' '+fmt(piece.total)})]);
  wrap.addEventListener('click',()=>onSelect(piece.code));
  wrap._code=piece.code;
  return wrap;
}
function heatSection(){
  const wrap=h('div',{});
  const btns=h('div',{class:'piecebtns'});
  const bigHost=h('div',{});
  const minis=h('div',{class:'miniwrap'});
  let current=5; // white king by default
  const byCode={};DATA.pieces.forEach(p=>byCode[p.code]=p);
  function select(code){
    current=code;
    bigHost.innerHTML='';bigHost.appendChild(bigBoard(byCode[code]));
    [...btns.children].forEach(b=>b.classList.toggle('active',+b._code===code));
    [...minis.children].forEach(m=>m.classList.toggle('active',m._code===code));
  }
  DATA.pieces.forEach(p=>{
    const b=h('button',{class:'pb'},[h('span',{class:'g',html:p.glyph}),h('span',{text:p.letter}),h('span',{class:'c',text:fmt(p.total)})]);
    b._code=p.code;b.addEventListener('click',()=>select(p.code));btns.appendChild(b);
    minis.appendChild(miniBoard(p,select));
  });
  wrap.appendChild(btns);
  wrap.appendChild(h('div',{class:'heatwrap'},[bigHost,minis]));
  select(current);
  return wrap;
}

// ---- build ----
function build(){
  app.appendChild(h('h1',{text:'Devre self-play data report'}));
  app.appendChild(h('div',{class:'sub',html:`${fmt(DATA.games)} games &middot; ${fmt(DATA.positions)} scored positions &middot; ${DATA.files} file(s), ${(DATA.bytes/1e6).toFixed(2)} MB`}));
  app.appendChild(cards());

  app.appendChild(panel('Square occupancy per piece',heatSection(),
    'Pick a piece (or click a mini-board). Each square shows that square’s share of the piece’s occurrences; hover for exact % and count. a1 is bottom-left.',true));

  const grid=h('div',{class:'grid'});
  const wdl=DATA.wdl;
  grid.appendChild(panel('Game result (white POV)',barChart([['white win',wdl.white],['draw',wdl.draw],['black win',wdl.black]],DATA.games,{unit:'games',xfmt:x=>x}),
    'Balance of outcomes. A very high draw rate or a big white/black skew hints at opening or adjudication bias.'));
  grid.appendChild(panel('Score distribution',barChart(DATA.scoreHist.items,DATA.positions,{unit:'positions',xfmt:x=>x}),
    `White-relative eval (cp). Should peak near 0 and taper. ${DATA.scoreHist.overflow?fmt(DATA.scoreHist.overflow)+' positions beyond ±1000cp are not shown.':''}`));
  grid.appendChild(panel('Score → WDL calibration',calibChart(),
    DATA.k?`Empirical win rate (dots) vs fitted sigmoid (line). K=${DATA.k.toFixed(0)}cp, MSE=${DATA.mse.toFixed(4)}. Close agreement = evals and results are consistent.`:'not enough data'));
  grid.appendChild(panel('Piece count',barChart(DATA.pieceCount,DATA.positions,{unit:'positions',xfmt:x=>x}),
    'Total pieces on board. Coverage from openings (32) to endgames.'));
  grid.appendChild(panel('Game length',barChart(DATA.gameLen,DATA.games,{unit:'games',xfmt:x=>x}),
    'Plies per game, bucketed by 10. Watch for a wall at the 400-ply cap or a spike at very short games.'));
  grid.appendChild(panel('Best-move type',barChart([['quiet',DATA.moveType.quiet],['capture',DATA.moveType.capture],['promotion',DATA.moveType.promotion]],DATA.moveType.quiet+DATA.moveType.capture+DATA.moveType.promotion,{unit:'positions',xfmt:x=>x}),
    'Share of positions whose best move is a capture/promotion; filtering tactical best moves is a common training step.'));
  grid.appendChild(panel('Position ply',barChart(DATA.ply,DATA.positions,{unit:'positions',xfmt:x=>x}),
    'Where in the game scored positions come from (bucketed by 5).'));
  grid.appendChild(panel('Halfmove clock',barChart(DATA.halfmove,DATA.positions,{unit:'positions',xfmt:x=>x}),
    '50-move-rule counter. A long tail toward 100 = many shuffly, drawish endgames.'));
  app.appendChild(grid);
}
build();
</script></body></html>"""


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("data", help="a .bin file or directory of .bin files")
    ap.add_argument("--html", default="datagen_report.html", help="output dashboard path (default: datagen_report.html)")
    ap.add_argument("--no-html", action="store_true", help="print text summary only, no HTML")
    ap.add_argument("--max", type=int, default=0, help="stop after N scored positions (0 = all)")
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

    total_bytes = sum(os.path.getsize(p) for p in paths)
    s = analyze(paths, max_positions=args.max, do_check=not args.no_check, bin_cp=max(1, args.score_bin))
    print_text_report(s, total_bytes, len(paths))

    if not args.no_html:
        data = build_dashboard_data(s, total_bytes, len(paths))
        write_dashboard(data, args.html)
        print(f"\ninteractive dashboard written: {args.html}")
        print("  open it in a browser and hover any bar or heatmap square for exact numbers")
    return 0


if __name__ == "__main__":
    sys.exit(main())
