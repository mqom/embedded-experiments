#!/usr/bin/env python3
"""
reproduce_table.py — Compile, flash, capture UART output and format the paper
benchmark tables for the MQOM embedded-experiments artifact.

Usage:
    python3 tools/reproduce_table.py --table mqom-l1 --port /dev/ttyACM0
    python3 tools/reproduce_table.py --table rijndael --port /dev/ttyACM0 --board leia
    python3 tools/reproduce_table.py --table matmul --port /dev/ttyACM0 --output markdown
    python3 tools/reproduce_table.py --list-tables

Requires: pyserial  (pip install pyserial)
Run from the repository root directory.
"""

import argparse
import contextlib
import io
import re
import subprocess
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("Error: pyserial not installed.  Run:  pip install pyserial")

# ── Global defaults ────────────────────────────────────────────────────────────

BAUD_RATE       = 38400
DEFAULT_TIMEOUT = 600   # seconds per firmware run

# ── Build / flash helpers ──────────────────────────────────────────────────────

def run_make(board, make_vars):
    cmd = f"make clean && BOARD={board} {make_vars} make firmware"
    short = make_vars[:80] + ("..." if len(make_vars) > 80 else "")
    print(f"  [build] BOARD={board} {short} make firmware")
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr[-3000:], file=sys.stderr)
        raise RuntimeError("Compilation failed — see output above.")
    print("  [build] OK")


def run_flash(board):
    cmd = f"BOARD={board} make flash"
    print(f"  [flash] BOARD={board}")
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr[-3000:], file=sys.stderr)
        raise RuntimeError("Flash failed — see output above.")
    print("  [flash] OK")


# ── UART capture ───────────────────────────────────────────────────────────────

_G_RETRY_INTERVAL = 2.0  # seconds between 'g' retries

_REFLASH_AFTER_RETRIES = 5   # reflash if board silent after this many 'g' retries

_SPIN_CHARS = "|/-\\"


def _spin_write(frame, msg):
    sys.stdout.write(f"\033[2K\r  {_SPIN_CHARS[frame % 4]} {msg}")
    sys.stdout.flush()


def _spin_clear(msg):
    sys.stdout.write(f"\033[2K\r  {msg}\n")
    sys.stdout.flush()


def capture_uart(ser, timeout, reflash_fn=None, verbose=False):
    """Send 'g' to start the firmware, collect lines until END SERIAL COMM.

    Accepts an already-open Serial object so the port stays open for the whole
    session (avoids re-asserting DTR/RTS on every run).

    If the board stays silent after _REFLASH_AFTER_RETRIES consecutive 'g'
    retries, reflash_fn() is called automatically (handles the Nucleo quirk
    where one flash out of two leaves the target in debug-halt).
    """
    print(f"  [uart]  port={ser.port}  baud={BAUD_RATE}  timeout={timeout}s")
    lines          = []
    last_g         = time.time()
    got_output     = False
    silent_retries = 0
    spin_frame     = 0

    ser.reset_input_buffer()
    ser.write(b"g")

    deadline = time.time() + timeout
    while time.time() < deadline:
        raw = ser.readline()

        if not raw:
            if not verbose:
                _spin_write(spin_frame, "Capturing serial…")
                spin_frame += 1
            if time.time() - last_g > _G_RETRY_INTERVAL:
                if not got_output:
                    silent_retries += 1
                    if verbose:
                        print(f"  [uart]  no output yet, resending 'g'… (#{silent_retries})")
                    if reflash_fn and silent_retries % _REFLASH_AFTER_RETRIES == 0:
                        if not verbose:
                            _spin_clear(f"[uart]  silent after {silent_retries} retries — reflashing…")
                        else:
                            print(f"  [uart]  board silent after {silent_retries} retries — reflashing…")
                        reflash_fn()
                        ser.reset_input_buffer()
                        spin_frame = 0
                ser.write(b"g")
                last_g = time.time()
            continue

        got_output = True
        line = raw.decode("utf-8", errors="replace").rstrip("\r\n")

        if "Waiting for the user to press on 'g'" in line:
            ser.write(b"g")
            last_g = time.time()
            continue

        lines.append(line)
        if verbose:
            print(f"    > {line}")
        else:
            _spin_write(spin_frame, f"Capturing serial… ({len(lines)} lines)")
            spin_frame += 1

        if "END SERIAL COMM" in line:
            break
    else:
        if not verbose:
            _spin_clear("[uart]  TIMEOUT")
        raise TimeoutError(
            f"Timed out after {timeout}s waiting for 'END SERIAL COMM'.\n"
            "Check that the board is powered, the port is correct, and the\n"
            "firmware was flashed successfully."
        )

    if not verbose:
        _spin_clear(f"[uart]  done — {len(lines)} lines captured")
    return "\n".join(lines)


# ── Parser helpers ─────────────────────────────────────────────────────────────

def _int(text, pattern):
    m = re.search(pattern, text)
    return int(m.group(1)) if m else None

def _float(text, pattern):
    m = re.search(pattern, text)
    return float(m.group(1)) if m else None

def _mc(cycles):
    """Raw integer cycles → formatted Mc string, or 'N/A'."""
    if cycles is None:
        return "N/A"
    return f"{cycles / 1e6:.2f}"

def _mc_f(cycles):
    """Raw float cycles → formatted Mc string (for btimer output), or 'N/A'."""
    if cycles is None:
        return "N/A"
    return f"{cycles / 1e6:.2f}"

def _kb(b):
    """Bytes → formatted kB string (÷1000, matching paper convention), or 'N/A'."""
    if b is None:
        return "N/A"
    return f"{b / 1000:.2f}"


# ── Parsers ────────────────────────────────────────────────────────────────────

def parse_mqom(text):
    """Standard MQOM test: keygen / sign (PoW-adjusted) / verify."""
    return dict(
        keygen_cycles = _int(text, r"crypto_sign_keypair OK! Took (\d+) cycles"),
        keygen_mem    = _int(text, r"=> crypto_sign_keypair total mem usage: (\d+)"),
        # PoW-adjusted sign figure, NOT the raw 'Took X cycles' line
        sign_cycles   = _int(text, r"=> crypto_sign PoW adjusted cycles = (-?\d+)"),
        sign_mem      = _int(text, r"=> crypto_sign total mem usage: (\d+)"),
        verify_cycles = _int(text, r"crypto_sign_open OK! Took (\d+) cycles"),
        verify_mem    = _int(text, r"=> crypto_sign_open total mem usage: (\d+)"),
        sig_size      = _int(text, r"Signature size \(MAX\): (\d+) B"),
    )


def parse_detailed(text):
    """MQOM test with BENCHMARK=1 BENCHMARK_CYCLES=1: keygen/sign/verify + component breakdown."""
    result = parse_mqom(text)
    components = {}
    for m in re.finditer(r"   - (.+?): [\d.]+ ms \(([\d.]+) cycles\)", text):
        name   = m.group(1).strip()
        cycles = float(m.group(2))
        # Skip debug pin timers
        if name in ("Pin A", "Pin B", "Pin C", "Pin D"):
            continue
        components[name] = cycles
    result["components"] = components
    return result


def parse_rijndael(text):
    """Rijndael / AES performance test."""
    r = {}

    # ── x1 (single block) ────────────────────────────────────────────────────
    m = re.search(r"\[\+\] AES-128 keysched: (\d+) cycles, encryption (\d+) cycles", text)
    if m:
        r["aes128_x1_ks"], r["aes128_x1_enc"] = int(m.group(1)), int(m.group(2))

    m = re.search(r"\[\+\] RIJNDAEL-256 keysched: (\d+) cycles, encryption (\d+) cycles", text)
    if m:
        r["r256_x1_ks"], r["r256_x1_enc"] = int(m.group(1)), int(m.group(2))

    # ── xN sequential-key enc ────────────────────────────────────────────────
    for scheme, tag in [("aes128", "AES-128"), ("r256", "RIJNDAEL-256")]:
        for n in (2, 4):
            m = re.search(rf"\[\+\] {tag} X{n} encryption performance: (\d+) cycles", text)
            if m:
                r[f"{scheme}_x{n}_enc"] = int(m.group(1))

    # ── xNxN amortised keysched+enc ──────────────────────────────────────────
    for scheme, tag in [("aes128", "AES-128"), ("r256", "RIJNDAEL-256")]:
        for n in (2, 4):
            m = re.search(rf"\[\+\] {tag} X{n} X{n} key sched performance: (\d+) cycles", text)
            if m:
                r[f"{scheme}_x{n}x{n}_ks"] = int(m.group(1))
            m = re.search(rf"\[\+\] {tag} X{n} X{n} encryption performance: (\d+) cycles", text)
            if m:
                r[f"{scheme}_x{n}x{n}_enc"] = int(m.group(1))

    # ── Hardware (leia board): single-block values from POLLING section ───────
    poll = re.search(r"==== POLLING\r?\n([\d ]+)", text)
    if poll:
        vals = list(map(int, poll.group(1).split()))
        if len(vals) >= 1:
            r["aes128_hw_x1"] = vals[0]
        if len(vals) >= 2:
            r["aes128_hw_x2"] = vals[1]
        if len(vals) >= 4:
            r["aes128_hw_x4"] = vals[3]

    # ── Derive sequential-key keysched from x1 (n * x1_ks) ──────────────────
    for scheme in ("aes128", "r256"):
        if f"{scheme}_x1_ks" in r:
            for n in (2, 4):
                r[f"{scheme}_x{n}_ks"] = n * r[f"{scheme}_x1_ks"]

    return r


def parse_dma_polling(text):
    """Extract POLLING and DMA cycle-count sequences (one per block count 1..2047)."""
    m_poll = re.search(r"==== POLLING\r?\n([\d ]+)", text)
    m_dma  = re.search(r"==== DMA\r?\n([\d ]+)",     text)
    polling = list(map(int, m_poll.group(1).split())) if m_poll else []
    dma     = list(map(int, m_dma.group(1).split()))  if m_dma  else []
    return polling, dma


def parse_matmul(text):
    """Matrix multiplication test — values are in raw cycles (printed as %.2f)."""
    pats = {
        "default":                 r"- MatMul\(default\): ([\d.]+) cycles",
        "ref":                     r"- MatMul\(ref\): ([\d.]+) cycles",
        "transform_bs":            r"- Transform\(bitslice\): ([\d.]+) cycles",
        "bitslice":                r"- MatMul\(bitslice\): ([\d.]+) cycles",
        "transform_bs_comp":       r"- Transform\(bitslice-composite\): ([\d.]+) cycles",
        "bitslice_composite":      r"- MatMul\(bitslice-composite\): ([\d.]+) cycles",
        "transform_bs_cond":       r"- Transform\(bitslice-cond\): ([\d.]+) cycles",
        "bitslice_cond":           r"- MatMul\(bitslice-cond\): ([\d.]+) cycles",
        "transform_bs_comp_cond":  r"- Transform\(bitslice-composite-cond\): ([\d.]+) cycles",
        "bitslice_composite_cond": r"- MatMul\(bitslice-composite-cond\): ([\d.]+) cycles",
    }
    return {k: _float(text, p) for k, p in pats.items()}


def parse_streaming(text):
    """Streaming verification test."""
    r = {}
    r["sign_cycles"]         = _int(text, r"StreamedVerify_Sign OK! Took (\d+) cycles")
    r["init_cycles"]         = _int(text, r"StreamedVerify_Init OK! Took (\d+) cycles")
    r["update_first_cycles"] = _int(text, r"StreamedVerify_Update FIRST_CHUNK_BYTESIZE OK! Took (\d+) cycles")
    r["finalize_cycles"]     = _int(text, r"StreamedVerify_Finalize OK! Took (\d+) cycles")
    r["sig_size"]            = _int(text, r"Signature size \(MAX\): (\d+) B")
    all_updates = re.findall(r"StreamedVerify_Update .* OK! Took (\d+) cycles", text)
    others = [int(x) for x in all_updates[1:]]
    r["update_other_cycles"] = sum(others) // len(others) if others else None
    r["tau_measured"]        = len(others)
    return r


def parse_presign(text):
    """Pre-signature test."""
    return dict(
        keygen_cycles   = _int(text, r"crypto_sign_keypair OK! Took (\d+) cycles"),
        keygen_mem      = _int(text, r"=> crypto_sign_keypair total mem usage: (\d+)"),
        prepare_cycles  = _int(text, r"crypto_sign_prepare OK! Took (\d+) cycles"),
        prepare_mem     = _int(text, r"=> crypto_sign_prepare total mem usage: (\d+)"),
        finalize_cycles = _int(text, r"=> crypto_sign PoW adjusted cycles = (-?\d+)"),
        finalize_mem    = _int(text, r"=> crypto_sign_finalize total mem usage: (\d+)"),
        verify_cycles   = _int(text, r"crypto_sign_open OK! Took (\d+) cycles"),
        verify_mem      = _int(text, r"=> crypto_sign_open total mem usage: (\d+)"),
        sig_size        = _int(text, r"SIZE of SIGNATURE: (\d+)"),
        presig_size     = _int(text, r"SIZE of PRESIGNATURE: (\d+)"),
    )


PARSERS = {
    "mqom":      parse_mqom,
    "detailed":  parse_detailed,
    "rijndael":  parse_rijndael,
    "matmul":    parse_matmul,
    "streaming": parse_streaming,
    "presign":   parse_presign,
}


# ── DMA vs Polling helpers ─────────────────────────────────────────────────────

def save_dma_polling_csv(polling, dma, path="dma_polling.csv"):
    n = min(len(polling), len(dma), 2047)
    if n == 0:
        print("  [dma-polling] WARNING: no data parsed", file=sys.stderr)
        return
    with open(path, "w") as f:
        f.write("blocks,polling_cycles,dma_cycles\n")
        for i in range(n):
            f.write(f"{i + 1},{polling[i]},{dma[i]}\n")
    print(f"  [dma-polling] {n} rows saved → {path}")
    for i in range(n):
        if dma[i] <= polling[i]:
            print(f"  [dma-polling] DMA faster than polling from block {i + 1} onward")
            break


def plot_dma_polling(polling, dma, path="dma_polling.png"):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("  [dma-polling] matplotlib not installed — skipping plot")
        return
    n = min(len(polling), len(dma))
    blocks = list(range(1, n + 1))
    plt.figure(figsize=(10, 5))
    plt.plot(blocks, polling[:n], label="Polling")
    plt.plot(blocks, dma[:n],     label="DMA")
    plt.xlabel("Number of AES-128 blocks")
    plt.ylabel("Cycles")
    plt.title("Hardware AES-128: Polling vs DMA latency")
    plt.legend()
    plt.tight_layout()
    plt.savefig(path, dpi=150)
    print(f"  [dma-polling] plot saved → {path}")


# ── Shared make-var building blocks ───────────────────────────────────────────

_M2_LUT = ("USE_PRG_CACHE=1 USE_PIOP_CACHE=0 MEMORY_EFFICIENT_PIOP=0 PIOP_BITSLICE=1 "
            "FIELDS_BITSLICE_COMPOSITE=1 FIELDS_BITSLICE_PUBLIC_JUMP=1 "
            "MEMORY_EFFICIENT_BLC=1 MEMORY_EFFICIENT_KEYGEN=1 "
            "RIJNDAEL_OPT_ARMV7M=1 USE_GF256_TABLE_LOG_EXP=1")

_M2_BAL = ("USE_PRG_CACHE=1 USE_PIOP_CACHE=0 MEMORY_EFFICIENT_PIOP=0 PIOP_BITSLICE=1 "
            "FIELDS_BITSLICE_COMPOSITE=1 FIELDS_BITSLICE_PUBLIC_JUMP=1 "
            "MEMORY_EFFICIENT_BLC=1 MEMORY_EFFICIENT_KEYGEN=1 "
            "RIJNDAEL_OPT_ARMV7M=1 USE_GF256_TABLE_LOG_EXP=0")

_M2_MEM = ("USE_PRG_CACHE=0 USE_PIOP_CACHE=0 MEMORY_EFFICIENT_PIOP=1 PIOP_BITSLICE=0 "
            "FIELDS_BITSLICE_COMPOSITE=1 FIELDS_BITSLICE_PUBLIC_JUMP=1 "
            "MEMORY_EFFICIENT_BLC=1 MEMORY_EFFICIENT_KEYGEN=1 "
            "RIJNDAEL_OPT_ARMV7M=1 USE_GF256_TABLE_LOG_EXP=0")

_M2_HW  = ("USE_PRG_CACHE=1 USE_PIOP_CACHE=0 MEMORY_EFFICIENT_PIOP=0 PIOP_BITSLICE=1 "
            "FIELDS_BITSLICE_COMPOSITE=1 FIELDS_BITSLICE_PUBLIC_JUMP=1 "
            "MEMORY_EFFICIENT_BLC=1 MEMORY_EFFICIENT_KEYGEN=1 "
            "RIJNDAEL_OPT_ARMV7M=1 USE_GF256_TABLE_LOG_EXP=0")

_LUT_FLAGS = ("RIJNDAEL_BITSLICE=0 RIJNDAEL_TABLE=1 RIJNDAEL_EXTERNAL=0 "
              "USE_ENC_CTX_CLEANSING=0 USE_ENC_X8=0 USE_XOF_X4=0 BLC_INTERNAL_X2=0 "
              "BENCHMARK=0 VERIFY_MEMOPT=0 NO_EXPANDMQ_PRG_CACHE=1 SEED_COMMIT_MEMOPT=0 "
              "RIJNDAEL_TABLE_FORCE_IN_FLASH=0 USE_SIGNATURE_BUFFER_AS_TEMP=0 "
              "BLC_SEEDCOMMIT_CACHE=1 BLC_SEEDEXPAND_CACHE=1")

_BAL_FLAGS = ("RIJNDAEL_BITSLICE=1 RIJNDAEL_TABLE=0 RIJNDAEL_EXTERNAL=0 "
              "USE_ENC_CTX_CLEANSING=0 USE_ENC_X8=0 USE_XOF_X4=0 BLC_INTERNAL_X2=0 "
              "BENCHMARK=0 NO_EXPANDMQ_PRG_CACHE=1 "
              "GGMTREE_NB_ENC_CTX_IN_MEMORY=0 BLC_SEEDCOMMIT_CACHE=1 BLC_SEEDEXPAND_CACHE=1")

_MEM_FLAGS = ("RIJNDAEL_BITSLICE=1 RIJNDAEL_TABLE=0 RIJNDAEL_EXTERNAL=0 "
              "USE_ENC_CTX_CLEANSING=0 USE_ENC_X8=0 USE_XOF_X4=0 BLC_INTERNAL_X2=0 "
              "BENCHMARK=0 VERIFY_MEMOPT=1 NO_EXPANDMQ_PRG_CACHE=1 PRG_ONE_RIJNDAEL_CTX=1 "
              "SEED_COMMIT_MEMOPT=1 RIJNDAEL_TABLE_FORCE_IN_FLASH=1 "
              "GGMTREE_NB_ENC_CTX_IN_MEMORY=0 "
              "PIOP_NB_PARALLEL_REPETITIONS_SIGN=9 PIOP_NB_PARALLEL_REPETITIONS_VERIFY=4")

_HW_FLAGS  = ("RIJNDAEL_BITSLICE=0 RIJNDAEL_TABLE=0 RIJNDAEL_EXTERNAL=1 "
              "USE_ENC_CTX_CLEANSING=1 USE_ENC_X8=0 USE_XOF_X4=0 BLC_INTERNAL_X2=0 "
              "BENCHMARK=0 VERIFY_MEMOPT=0 NO_EXPANDMQ_PRG_CACHE=1 PRG_ONE_RIJNDAEL_CTX=0 "
              "SEED_COMMIT_MEMOPT=0 RIJNDAEL_TABLE_FORCE_IN_FLASH=1 "
              "GGMTREE_NB_ENC_CTX_IN_MEMORY=3 "
              'EXTRA_CFLAGS="-DEXTERNAL_COMMON_OVERRIDE -I../common_tests/"')

# Onetree variants: identical to above but MEMORY_EFFICIENT_BLC=0 (incompatible with BLC_ONETREE_MEMOPT)
_M2_OT_LUT = _M2_LUT.replace('MEMORY_EFFICIENT_BLC=1', 'MEMORY_EFFICIENT_BLC=0')
_M2_OT_BAL = _M2_BAL.replace('MEMORY_EFFICIENT_BLC=1', 'MEMORY_EFFICIENT_BLC=0')
_M2_OT_MEM = _M2_MEM.replace('MEMORY_EFFICIENT_BLC=1', 'MEMORY_EFFICIENT_BLC=0')
_M2_OT_HW  = _M2_HW.replace('MEMORY_EFFICIENT_BLC=1', 'MEMORY_EFFICIENT_BLC=0')

_LEAVES_FAST   = "GGMTREE_NB_SIMULTANEOUS_LEAVES_LOG=5 BLC_NB_LEAF_SEEDS_IN_PARALLEL=32"
_LEAVES_FASTER = _LEAVES_FAST
_LEAVES_SHORT  = "GGMTREE_NB_SIMULTANEOUS_LEAVES_LOG=7 BLC_NB_LEAF_SEEDS_IN_PARALLEL=64"
_LEAVES_MEM    = "GGMTREE_NB_SIMULTANEOUS_LEAVES_LOG=4 BLC_NB_LEAF_SEEDS_IN_PARALLEL=8"


def _mqom_vars(variant, m2_opts, outer_flags, leaves=""):
    return f'MQOM2_OPTIONS="{variant} {m2_opts}" {outer_flags} {leaves}'.strip()


def _with_benchmark(flags):
    """Replace BENCHMARK=0 with BENCHMARK=1 BENCHMARK_CYCLES=1."""
    return flags.replace("BENCHMARK=0", "BENCHMARK=1 BENCHMARK_CYCLES=1")


# ── Rijndael / AES performance ────────────────────────────────────────────────

_RIJNDAEL_RUNS = [
    dict(label="Bitslice", board="nucleol4r5zi", parser="rijndael",
         tags=frozenset({"bitslice"}),
         make_vars="RIJNDAEL_OPT_ARMV7M=1 RIJNDAEL_BITSLICE=1 RIJNDAEL_TEST=1"),
    dict(label="Table",    board="nucleol4r5zi", parser="rijndael",
         tags=frozenset({"table"}),
         make_vars="RIJNDAEL_OPT_ARMV7M=1 RIJNDAEL_TABLE=1 RIJNDAEL_TEST=1"),
    dict(label="Hardware", board="leia",         parser="rijndael",
         tags=frozenset({"hardware"}),
         make_vars='RIJNDAEL_OPT_ARMV7M=1 RIJNDAEL_EXTERNAL=1 RIJNDAEL_TEST=1 '
                   'EXTRA_CFLAGS="-DNO_AES256_TESTS -DNO_RIJNDAEL256_TESTS"'),
]


def _rval(d, key):
    v = d.get(key)
    return str(v) if v is not None else "N/A"


def format_rijndael(results, **_):
    bs = results.get("Bitslice", {})
    tb = results.get("Table",    {})
    hw = results.get("Hardware", {})

    col_w = [19, 12, 12, 12, 12, 12, 12, 15]
    hdr   = (f"| {'Scheme/Variant':<19} | {'BS KS':>12} | {'BS Enc':>12} | "
             f"{'Tbl KS':>12} | {'Tbl Enc':>12} | {'HW Enc (Poll)':>12} |")
    sep   = "|" + "|".join("-" * (w + 2) for w in col_w[:-1]) + "|"
    print("\n=== Rijndael / AES Performance (cycles) ===")
    print(hdr)
    print(sep)

    rows = [
        ("AES-128 x1",          "aes128_x1_ks",   "aes128_x1_enc",
                                 "aes128_x1_ks",   "aes128_x1_enc",   "aes128_hw_x1"),
        ("AES-128 x2",          "aes128_x2_ks",   "aes128_x2_enc",
                                 "aes128_x2_ks",   "aes128_x2_enc",   "aes128_hw_x2"),
        ("AES-128 x2 amort.",   "aes128_x2x2_ks", "aes128_x2x2_enc",
                                 None,              None,              None),
        ("AES-128 x4",          "aes128_x4_ks",   "aes128_x4_enc",
                                 "aes128_x4_ks",   "aes128_x4_enc",   "aes128_hw_x4"),
        ("AES-128 x4 amort.",   "aes128_x4x4_ks", "aes128_x4x4_enc",
                                 None,              None,              None),
        ("Rijn-256 x1",         "r256_x1_ks",     "r256_x1_enc",
                                 "r256_x1_ks",     "r256_x1_enc",     None),
        ("Rijn-256 x2",         "r256_x2_ks",     "r256_x2_enc",
                                 "r256_x2_ks",     "r256_x2_enc",     None),
        ("Rijn-256 x2 amort.",  "r256_x2x2_ks",   "r256_x2x2_enc",
                                 None,              None,              None),
        ("Rijn-256 x4",         "r256_x4_ks",     "r256_x4_enc",
                                 "r256_x4_ks",     "r256_x4_enc",     None),
        ("Rijn-256 x4 amort.",  "r256_x4x4_ks",   "r256_x4x4_enc",
                                 None,              None,              None),
    ]
    for lbl, bk, be, tk, te, hk in rows:
        b_ks  = _rval(bs, bk) if bk else "-"
        b_enc = _rval(bs, be) if be else "-"
        t_ks  = _rval(tb, tk) if tk else "-"
        t_enc = _rval(tb, te) if te else "-"
        h_enc = _rval(hw, hk) if hk else "N/A"
        print(f"| {lbl:<19} | {b_ks:>12} | {b_enc:>12} | "
              f"{t_ks:>12} | {t_enc:>12} | {h_enc:>12} |")


# ── Matrix multiplication ──────────────────────────────────────────────────────

def _matmul_run(variant, extra=""):
    return f'MQOM2_OPTIONS="MQOM2_VARIANT={variant}" {extra} BENCHMARK=1 MAT_MULT_TEST=1'.strip()


# Tags for each matmul run key (swar runs also contain all bitslice variants)
_MATMUL_RUN_TAGS = {
    "swar":         frozenset({"swar", "bitslice", "bitslice_jump",
                                "bitslice_composite", "bitslice_composite_jump"}),
    "logexp":       frozenset({"logexp"}),
    "fulltable":    frozenset({"fulltable"}),
    "basiccircuit": frozenset({"basiccircuit"}),
}
# Tags per display row (used for row-level filtering inside format_matmul)
_MATMUL_ROW_TAGS = [
    frozenset({"logexp"}),
    frozenset({"fulltable"}),
    frozenset({"basiccircuit"}),
    frozenset({"swar"}),
    frozenset({"bitslice",  "swar"}),
    frozenset({"bitslice",  "bitslice_jump",             "swar"}),
    frozenset({"bitslice",  "bitslice_composite",        "swar"}),
    frozenset({"bitslice",  "bitslice_composite",
               "bitslice_composite_jump", "bitslice_jump", "swar"}),
]
_MATMUL_ALL_IMPL_TAGS = frozenset().union(*_MATMUL_RUN_TAGS.values())

_MATMUL_RUNS = []
for _lvl, _var in [("L1", "cat1-gf16-fast-r5"),
                   ("L3", "cat3-gf16-fast-r5"),
                   ("L5", "cat5-gf16-fast-r5")]:
    for _key, _extra in [
        ("swar",         ""),
        ("logexp",       "USE_GF256_TABLE_LOG_EXP=1"),
        ("fulltable",    "USE_GF256_TABLE_MULT=1 GF256_MULT_TABLE_SRAM=1"),
        ("basiccircuit", "NO_FIELDS_REF_SWAR_OPT=1"),
    ]:
        _MATMUL_RUNS.append(dict(
            label=f"{_key}_{_lvl}",
            board="nucleol4r5zi",
            parser="matmul",
            tags=_MATMUL_RUN_TAGS[_key] | {_lvl.lower()},
            make_vars=_matmul_run(_var, _extra),
        ))


def format_matmul(results, active_filters=frozenset(), **_):
    print("\n=== Matrix Multiplication (Mc) ===")
    hdr = (f"| {'Implementation':<32} | {'GF16 Fast L1':^14} | "
           f"{'GF16 Fast L3':^14} | {'GF16 Fast L5':^14} |")
    sep = "|" + "|".join("-" * (w + 2) for w in [32, 14, 14, 14]) + "|"
    print(hdr)
    print(sep)

    row_defs = [
        ("F256 Log/Exp tables",          "logexp",        "ref",                     None),
        ("Full F256 mult table",          "fulltable",     "ref",                     None),
        ("Basic circuit",                 "basiccircuit",  "ref",                     None),
        ("SWAR 32 bits",                  "swar",          "ref",                     None),
        ("Bitslice",                      "swar",          "bitslice",                "transform_bs"),
        ("Bitslice with jump",            "swar",          "bitslice_cond",           "transform_bs_cond"),
        ("Bitslice composite",            "swar",          "bitslice_composite",      "transform_bs_comp"),
        ("Bitslice composite with jump",  "swar",          "bitslice_composite_cond", "transform_bs_comp_cond"),
    ]
    impl_filters = active_filters & _MATMUL_ALL_IMPL_TAGS
    for (impl_lbl, run_sfx, val_k, tx_k), row_tags in zip(row_defs, _MATMUL_ROW_TAGS):
        if impl_filters and not (impl_filters & row_tags):
            continue
        cols = []
        for lvl in ("L1", "L3", "L5"):
            r = results.get(f"{run_sfx}_{lvl}", {})
            v = r.get(val_k)
            s = _mc_f(v)
            if tx_k:
                s = f"{s} ({_mc_f(r.get(tx_k))})"
            cols.append(s)
        print(f"| {impl_lbl:<32} | {cols[0]:^14} | {cols[1]:^14} | {cols[2]:^14} |")


# ── MQOM base — shared run generator ─────────────────────────────────────────

_CAT_TO_LEVEL = {"cat1": "l1", "cat3": "l3", "cat5": "l5"}


def _t4_runs(levels=("cat1",),
             profiles=("LUT", "Balanced", "Memory", "Hardware"),
             instances=(("Faster", "faster"), ("Fast", "fast"), ("Short", "short")),
             prefix="",
             benchmark=False,
             parser_name="mqom"):
    """Generate run descriptors for MQOM table / detailed variants."""
    runs = []
    for cat in levels:
        lvl_tag = _CAT_TO_LEVEL.get(cat, cat)
        for inst_name, inst_slug in instances:
            variant  = f"{cat}-gf16-{inst_slug}-r5"
            leaves_fs = (_LEAVES_FASTER if inst_slug == "faster" else
                         _LEAVES_SHORT  if inst_slug == "short"  else _LEAVES_FAST)

            def _flags(f):
                return _with_benchmark(f) if benchmark else f

            if "LUT" in profiles:
                runs.append(dict(
                    label=f"{prefix}LUT {inst_name}",
                    board="nucleol4r5zi", parser=parser_name,
                    tags=frozenset({lvl_tag, "lut", inst_slug}),
                    make_vars=_mqom_vars(f"MQOM2_VARIANT={variant}",
                                        _M2_LUT, _flags(_LUT_FLAGS), leaves_fs),
                ))
            if "Balanced" in profiles:
                runs.append(dict(
                    label=f"{prefix}Bal. {inst_name}",
                    board="nucleol4r5zi", parser=parser_name,
                    tags=frozenset({lvl_tag, "balanced", inst_slug}),
                    make_vars=_mqom_vars(f"MQOM2_VARIANT={variant}",
                                        _M2_BAL, _flags(_BAL_FLAGS), leaves_fs),
                ))
            if "Memory" in profiles:
                runs.append(dict(
                    label=f"{prefix}Mem. {inst_name}",
                    board="nucleol4r5zi", parser=parser_name,
                    tags=frozenset({lvl_tag, "memory", inst_slug}),
                    make_vars=_mqom_vars(f"MQOM2_VARIANT={variant}",
                                        _M2_MEM, _flags(_MEM_FLAGS), _LEAVES_MEM),
                ))
            if "Hardware" in profiles and inst_slug != "faster":
                runs.append(dict(
                    label=f"{prefix}Hard. {inst_name}",
                    board="leia", parser=parser_name,
                    tags=frozenset({lvl_tag, "hardware", inst_slug}),
                    make_vars=_mqom_vars(f"MQOM2_VARIANT={variant}",
                                        _M2_HW, _flags(_HW_FLAGS), leaves_fs),
                ))
    return runs


_MQOM_L1_RUNS   = _t4_runs()
_MQOM_L3L5_RUNS = (
    _t4_runs(levels=("cat3",),
             profiles=("LUT", "Balanced", "Memory"),
             instances=(("Faster", "faster"), ("Fast", "fast")),
             prefix="L3 ")
    + _t4_runs(levels=("cat5",),
               profiles=("LUT", "Balanced", "Memory"),
               instances=(("Faster", "faster"), ("Fast", "fast")),
               prefix="L5 ")
)
_DETAILED_RUNS  = _t4_runs(benchmark=True, parser_name="detailed")


_LEVEL_TAGS        = {"L1", "L3", "L5"}
_PROFILE_FULL      = {"LUT": "LUT", "Bal.": "Balanced", "Mem.": "Memory", "Hard.": "Hardware"}
_PROFILE_BASE_ORD  = ["LUT", "Bal.", "Mem.", "Hard."]


def _parse_mqom_label(label):
    """Split 'L3 Bal. Fast' → ('L3', 'Bal.', 'Fast'); 'LUT Faster' → ('', 'LUT', 'Faster')."""
    parts = label.split(" ", 2)
    if parts[0] in _LEVEL_TAGS:
        level, rest = parts[0], parts[1:]
    else:
        level, rest = "", parts
    profile  = rest[0] if rest else ""
    instance = " ".join(rest[1:]) if len(rest) > 1 else ""
    return level, profile, instance


def format_mqom_table(results, title, **_):
    # Group by profile, preserving run order within each profile
    groups      = {}
    seen_levels = set()
    for label, r in results.items():
        level, profile, instance = _parse_mqom_label(label)
        seen_levels.add(level)
        groups.setdefault(profile, []).append((level, instance, r))

    has_levels = bool(seen_levels - {""})

    def _prof_key(p):
        base = p.rstrip("-0123456789")
        return (_PROFILE_BASE_ORD.index(base) if base in _PROFILE_BASE_ORD else 99, p)

    sorted_profiles = sorted(groups, key=_prof_key)

    print(f"\n=== {title} ===")
    if has_levels:
        widths = [10, 3, 8, 7, 9, 9, 10, 8, 8, 10]
        hdr = (f"| {'Profile':<10} | {'Lvl':>3} | {'Instance':<8} | {'Sig(B)':>7} | "
               f"{'KGen(Mc)':>9} | {'Sign(Mc)':>9} | {'Verify(Mc)':>10} | "
               f"{'KGen(kB)':>8} | {'Sign(kB)':>8} | {'Verify(kB)':>10} |")
    else:
        widths = [10, 8, 7, 9, 9, 10, 8, 8, 10]
        hdr = (f"| {'Profile':<10} | {'Instance':<8} | {'Sig(B)':>7} | "
               f"{'KGen(Mc)':>9} | {'Sign(Mc)':>9} | {'Verify(Mc)':>10} | "
               f"{'KGen(kB)':>8} | {'Sign(kB)':>8} | {'Verify(kB)':>10} |")
    sep = "|" + "|".join("-" * (w + 2) for w in widths) + "|"
    print(hdr)
    print(sep)

    for i, profile in enumerate(sorted_profiles):
        if i > 0:
            print(sep)
        full = _PROFILE_FULL.get(profile, profile)
        for j, (level, instance, r) in enumerate(groups[profile]):
            prof_cell = full if j == 0 else ""
            data = (f"{str(r.get('sig_size', 'N/A')):>7} "
                    f"| {_mc(r.get('keygen_cycles')):>9} "
                    f"| {_mc(r.get('sign_cycles')):>9} "
                    f"| {_mc(r.get('verify_cycles')):>10} "
                    f"| {_kb(r.get('keygen_mem')):>8} "
                    f"| {_kb(r.get('sign_mem')):>8} "
                    f"| {_kb(r.get('verify_mem')):>10} |")
            if has_levels:
                print(f"| {prof_cell:<10} | {level:>3} | {instance:<8} | {data}")
            else:
                print(f"| {prof_cell:<10} | {instance:<8} | {data}")


# ── Detailed benchmark formatter ──────────────────────────────────────────────

_PROFILE_PREFIX = {
    "LUT":      "LUT",
    "Balanced": "Bal.",
    "Memory":   "Mem.",
    "Hardware": "Hard.",
}

_PROFILES_WITH_INSTANCES = [
    ("LUT",      ["Faster", "Fast", "Short"]),
    ("Balanced", ["Faster", "Fast", "Short"]),
    ("Memory",   ["Faster", "Fast", "Short"]),
    ("Hardware", ["Fast",   "Short"]),
]

_DETAIL_ROWS = [
    ("BLC.Commit",            "BLC.Commit"),
    ("  Expand Trees",        "[BLC.Commit] Expand Trees"),
    ("  Seed Commit",         "[BLC.Commit] Seed Commit"),
    ("  PRG",                 "[BLC.Commit] PRG"),
    ("  XOF",                 "[BLC.Commit] XOF"),
    ("  Arithm",              "[BLC.Commit] Arithm"),
    ("PIOP.Compute",          "PIOP.Compute"),
    ("  ExpandMQ",            "[PIOP.Compute] ExpandMQ"),
    ("  Expand Batching Mat", "[PIOP.Compute] Expand Batching Mat"),
    ("  Matrix Mul Ext",      "[PIOP.Compute] Matrix Mul Ext"),
    ("  Compute t1",          "[PIOP.Compute] Compute t1"),
    ("  Compute P_zi",        "[PIOP.Compute] Compute P_zi"),
    ("  Batch and Mask",      "[PIOP.Compute] Batch and Mask"),
    ("Sample Challenge",      "Sample Challenge"),
    ("BLC.Open",              "BLC.Open"),
]

_SUMMARY_ROWS = [
    ("KeyGen",        "keygen_cycles"),
    ("Sign (PoW adj)", "sign_cycles"),
    ("Verify",        "verify_cycles"),
]


def format_detailed(results, **_):
    comp_w = 26
    col_w  = 12

    for profile, instances in _PROFILES_WITH_INSTANCES:
        pfx = _PROFILE_PREFIX[profile]
        instances = [i for i in instances if results.get(f"{pfx} {i}")]
        if not instances:
            continue

        col_hdrs = [f"{i} (Mc)" for i in instances]
        print(f"\n=== Detailed Benchmarks — {profile} ===")
        hdr = f"| {'Component':<{comp_w}} |" + "".join(f" {c:>{col_w}} |" for c in col_hdrs)
        sep = "|" + "-" * (comp_w + 2) + "|" + "".join("-" * (col_w + 2) + "|" for _ in instances)
        print(hdr)
        print(sep)

        for disp, key in _DETAIL_ROWS:
            row = f"| {disp:<{comp_w}} |"
            for inst in instances:
                r     = results.get(f"{pfx} {inst}", {})
                comps = r.get("components", {})
                cell  = _mc_f(comps.get(key))
                row  += f" {cell:>{col_w}} |"
            print(row)

        print("|" + "-" * (comp_w + 2) + "|" + "".join("-" * (col_w + 2) + "|" for _ in instances))
        for disp, key in _SUMMARY_ROWS:
            row = f"| {disp:<{comp_w}} |"
            for inst in instances:
                r    = results.get(f"{pfx} {inst}", {})
                cell = _mc(r.get(key))
                row += f" {cell:>{col_w}} |"
            print(row)


# ── One-tree experiments ───────────────────────────────────────────────────────

_OT_COMMON = ("BLC_ONETREE=1 BLC_ONETREE_MEMOPT=1 BENCHMARK=0 "
              "USE_ENC_X8=0 USE_XOF_X4=0 NO_EXPANDMQ_PRG_CACHE=1")


def _ot_run(label, board, variant, m2_opts, outer, leaves="", par="", tags=frozenset()):
    mv = (f"ONETREE_TEST=1 "
          f'MQOM2_OPTIONS="MQOM2_VARIANT={variant} {m2_opts}" '
          f"{outer} {_OT_COMMON} {leaves} {par}").strip()
    return dict(label=label, board=board, parser="mqom", make_vars=mv, tags=tags)


_OT_RUNS = []
for _inst, _var in [("Fast", "cat1-gf16-fast-r5"), ("Short", "cat1-gf16-short-r5")]:
    _lv_s   = _LEAVES_SHORT if _inst == "Short" else _LEAVES_FAST
    _inst_t = _inst.lower()
    _par9   = ("BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN=9 "
                "BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY=9")
    _par6   = ("BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN=6 "
                "BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY=6")
    _par_x2 = _par6 if _inst == "Short" else _par9

    _lut_ot = ("RIJNDAEL_BITSLICE=0 RIJNDAEL_TABLE=1 RIJNDAEL_EXTERNAL=0 "
               "USE_ENC_CTX_CLEANSING=0 VERIFY_MEMOPT=0 RIJNDAEL_TABLE_FORCE_IN_FLASH=0 "
               "BLC_INTERNAL_X2=1 GGMTREE_NB_ENC_CTX_IN_MEMORY=3 "
               "USE_SIGNATURE_BUFFER_AS_TEMP=0 PRG_ONE_RIJNDAEL_CTX=0 SEED_COMMIT_MEMOPT=0")
    _OT_RUNS.append(_ot_run(f"LUT-1 {_inst}", "nucleol4r5zi", _var, _M2_OT_LUT, _lut_ot,
                             tags=frozenset({"lut", _inst_t})))
    _OT_RUNS.append(_ot_run(f"LUT-2 {_inst}", "nucleol4r5zi", _var, _M2_OT_LUT, _lut_ot, par=_par_x2,
                             tags=frozenset({"lut", _inst_t})))

    _bal_ot = ("RIJNDAEL_BITSLICE=1 RIJNDAEL_TABLE=0 RIJNDAEL_EXTERNAL=0 "
               "USE_ENC_CTX_CLEANSING=0 BLC_INTERNAL_X2=1 GGMTREE_NB_ENC_CTX_IN_MEMORY=3")
    _OT_RUNS.append(_ot_run(f"Bal.-1 {_inst}", "nucleol4r5zi", _var, _M2_OT_BAL, _bal_ot,
                             tags=frozenset({"balanced", _inst_t})))
    _OT_RUNS.append(_ot_run(f"Bal.-2 {_inst}", "nucleol4r5zi", _var, _M2_OT_BAL, _bal_ot, par=_par_x2,
                             tags=frozenset({"balanced", _inst_t})))

    _mem_ot = ("RIJNDAEL_BITSLICE=1 RIJNDAEL_TABLE=0 RIJNDAEL_EXTERNAL=0 "
               "USE_ENC_CTX_CLEANSING=0 VERIFY_MEMOPT=0 RIJNDAEL_TABLE_FORCE_IN_FLASH=1 "
               "BLC_INTERNAL_X2=0 GGMTREE_NB_ENC_CTX_IN_MEMORY=0 "
               "PRG_ONE_RIJNDAEL_CTX=1 SEED_COMMIT_MEMOPT=1 BLC_NO_FAST_FOLDING=1 "
               "PIOP_NB_PARALLEL_REPETITIONS_SIGN=9 PIOP_NB_PARALLEL_REPETITIONS_VERIFY=4")
    _OT_RUNS.append(_ot_run(f"Mem.-1 {_inst}", "nucleol4r5zi", _var, _M2_OT_MEM, _mem_ot,
                             tags=frozenset({"memory", _inst_t})))
    _OT_RUNS.append(_ot_run(f"Mem.-2 {_inst}", "nucleol4r5zi", _var, _M2_OT_MEM, _mem_ot, par=_par_x2,
                             tags=frozenset({"memory", _inst_t})))

    _hw_ot = ("RIJNDAEL_BITSLICE=0 RIJNDAEL_TABLE=0 RIJNDAEL_EXTERNAL=1 "
              "USE_ENC_CTX_CLEANSING=1 VERIFY_MEMOPT=0 RIJNDAEL_TABLE_FORCE_IN_FLASH=1 "
              "BLC_INTERNAL_X2=0 GGMTREE_NB_ENC_CTX_IN_MEMORY=3 "
              "PRG_ONE_RIJNDAEL_CTX=0 SEED_COMMIT_MEMOPT=0 "
              'EXTRA_CFLAGS="-DEXTERNAL_COMMON_OVERRIDE -I../common_tests/"')
    _OT_RUNS.append(_ot_run(f"Hard.-1 {_inst}", "leia", _var, _M2_OT_HW, _hw_ot,
                             tags=frozenset({"hardware", _inst_t})))
    _OT_RUNS.append(_ot_run(f"Hard.-2 {_inst}", "leia", _var, _M2_OT_HW, _hw_ot, par=_par_x2,
                             tags=frozenset({"hardware", _inst_t})))


# ── Streaming verification ────────────────────────────────────────────────────

_STREAM_OUTER = ("RIJNDAEL_BITSLICE=0 RIJNDAEL_TABLE=1 RIJNDAEL_EXTERNAL=0 "
                 "USE_ENC_CTX_CLEANSING=0 USE_ENC_X8=0 USE_XOF_X4=0 "
                 "BLC_INTERNAL_X2=1 GGMTREE_NB_ENC_CTX_IN_MEMORY=3 "
                 "BENCHMARK=0 VERIFY_MEMOPT=0 NO_EXPANDMQ_PRG_CACHE=1 "
                 "PRG_ONE_RIJNDAEL_CTX=0 SEED_COMMIT_MEMOPT=0 "
                 "RIJNDAEL_TABLE_FORCE_IN_FLASH=0 USE_SIGNATURE_BUFFER_AS_TEMP=0")

_STREAM_PARAMS = {
    "L1 Short": dict(tau=12, chk0_b=84,  chke_b=236),
    "L1 Fast":  dict(tau=17, chk0_b=84,  chke_b=188),
    "L5 Short": dict(tau=25, chk0_b=164, chke_b=474),
    "L5 Fast":  dict(tau=36, chk0_b=164, chke_b=378),
}

_STREAMING_RUNS = []
for _lbl, _var, _stags in [
    ("L1 Short", "cat1-gf16-short-r5", frozenset({"l1", "short"})),
    ("L1 Fast",  "cat1-gf16-fast-r5",  frozenset({"l1", "fast"})),
    ("L5 Short", "cat5-gf16-short-r5", frozenset({"l5", "short"})),
    ("L5 Fast",  "cat5-gf16-fast-r5",  frozenset({"l5", "fast"})),
]:
    _STREAMING_RUNS.append(dict(
        label=_lbl, board="nucleol4r5zi", parser="streaming",
        tags=_stags,
        make_vars=(f"VERIFY_STREAM_TEST=1 "
                   f'MQOM2_OPTIONS="MQOM2_VARIANT={_var} {_M2_LUT}" '
                   f"{_STREAM_OUTER}"),
    ))


def format_streaming(results, **_):
    print("\n=== Streaming Verification ===")
    hdr = (f"| {'Level/Inst.':<12} | {'τ':>3} | {'chk₀(B)':>7} | {'chkₑ(B)':>7} | "
           f"{'Sig(B)':>6} | {'Init(Mc)':>8} | {'UpdChk₀(Mc)':>11} | "
           f"{'UpdChkₑ(Mc)':>11} | {'Final(Mc)':>9} | {'Total(Mc)':>10} |")
    sep = "|" + "|".join("-" * (w + 2) for w in [12, 3, 7, 7, 6, 8, 11, 11, 9, 10]) + "|"
    print(hdr)
    print(sep)
    for label, r in results.items():
        p = _STREAM_PARAMS.get(label, {})
        init   = r.get('init_cycles')
        first  = r.get('update_first_cycles')
        other  = r.get('update_other_cycles')
        tau_m  = r.get('tau_measured')
        final  = r.get('finalize_cycles')
        if None not in (init, first, other, tau_m, final):
            total = init + first + tau_m * other + final
        else:
            total = None
        print(f"| {label:<12} | {p.get('tau', '?'):>3} "
              f"| {p.get('chk0_b', '?'):>7} | {p.get('chke_b', '?'):>7} "
              f"| {str(r.get('sig_size', 'N/A')):>6} "
              f"| {_mc(init):>8} "
              f"| {_mc(first):>11} "
              f"| {_mc(other):>11} "
              f"| {_mc(final):>9} "
              f"| {_mc(total):>10} |")


# ── Pre-signature ─────────────────────────────────────────────────────────────

_PS_OUTER = ("RIJNDAEL_BITSLICE=1 RIJNDAEL_TABLE=0 RIJNDAEL_EXTERNAL=0 "
             "USE_ENC_CTX_CLEANSING=0 USE_ENC_X8=0 USE_XOF_X4=0 "
             "BLC_INTERNAL_X2=0 GGMTREE_NB_ENC_CTX_IN_MEMORY=0 "
             "NO_EXPANDMQ_PRG_CACHE=1 BENCHMARK=0 "
             "BLC_SEEDCOMMIT_CACHE=1 BLC_SEEDEXPAND_CACHE=1")

_PRESIGN_RUNS = []
for _lbl, _var, _leaves, _extra, _ptags in [
    ("Short orig.PoW", "cat1-gf16-short-r5", _LEAVES_SHORT, "",                               frozenset({"short"})),
    ("Short LowPoW",   "cat1-gf16-short-r5", _LEAVES_SHORT, 'EXTRA_CFLAGS="-DWITH_LOW_POW"', frozenset({"short", "lowpow"})),
    ("Fast orig.PoW",  "cat1-gf16-fast-r5",  _LEAVES_FAST,  "",                               frozenset({"fast"})),
    ("Fast LowPoW",    "cat1-gf16-fast-r5",  _LEAVES_FAST,  'EXTRA_CFLAGS="-DWITH_LOW_POW"', frozenset({"fast",  "lowpow"})),
]:
    _PRESIGN_RUNS.append(dict(
        label=_lbl, board="nucleol4r5zi", parser="presign",
        tags=_ptags,
        make_vars=(f"PRESIGN_TEST=1 "
                   f'MQOM2_OPTIONS="MQOM2_VARIANT={_var} {_M2_BAL}" '
                   f"{_PS_OUTER} {_leaves} {_extra}").strip(),
    ))


def format_presign(results, **_):
    print("\n=== Pre-signature ===")
    hdr = (f"| {'Label':<20} | {'Sig(B)':>7} | {'Presig(B)':>9} | "
           f"{'Offline(Mc)':>11} | {'Online(Mc)':>10} |")
    sep = "|" + "|".join("-" * (w + 2) for w in [20, 7, 9, 11, 10]) + "|"
    print(hdr)
    print(sep)
    for label, r in results.items():
        print(f"| {label:<20} | {str(r.get('sig_size',  'N/A')):>7} "
              f"| {str(r.get('presig_size', 'N/A')):>9} "
              f"| {_mc(r.get('prepare_cycles')):>11} "
              f"| {_mc(r.get('finalize_cycles')):>10} |")


# ── Filter helpers ────────────────────────────────────────────────────────────

# Each frozenset is a "dimension": tags in the same dimension are OR-ed together;
# groups from different dimensions are AND-ed.
_FILTER_DIMS = [
    frozenset({"l1", "l3", "l5"}),
    frozenset({"lut", "balanced", "memory", "hardware"}),
    frozenset({"faster", "fast", "short"}),
    frozenset({"bitslice", "table"}),                        # rijndael impls
    frozenset({"logexp", "fulltable", "basiccircuit", "swar",
               "bitslice", "bitslice_jump",
               "bitslice_composite", "bitslice_composite_jump"}),  # matmul impls
    frozenset({"lowpow"}),
]


def _filter_matches(run_tags, active_filters):
    """Return True if run_tags satisfies active_filters with OR-within-dim / AND-across-dims."""
    remaining = set(active_filters)
    for dim in _FILTER_DIMS:
        dim_active = remaining & dim
        if not dim_active:
            continue
        remaining -= dim_active
        if not (run_tags & dim_active):   # run has none of the requested tags for this dim
            return False
    # Any tags that belong to no known dimension → require them all (AND fallback)
    return remaining <= run_tags


# ── Table registry ─────────────────────────────────────────────────────────────

TABLES = {
    "rijndael":   dict(
        runs=_RIJNDAEL_RUNS,
        formatter=format_rijndael,
        desc="Rijndael / AES performance (bitslice, table, hardware AES)",
    ),
    "matmul":     dict(
        runs=_MATMUL_RUNS,
        formatter=format_matmul,
        desc="Matrix multiplication — all implementations, L1/L3/L5",
    ),
    "mqom-l1":    dict(
        runs=_MQOM_L1_RUNS,
        formatter=lambda r, **kw: format_mqom_table(r, "MQOM L1 — Base Optimizations", **kw),
        desc="MQOM base L1 optimizations — all profiles (LUT/Balanced/Memory/Hardware)",
    ),
    "mqom-l3l5":  dict(
        runs=_MQOM_L3L5_RUNS,
        formatter=lambda r, **kw: format_mqom_table(r, "MQOM L3/L5 — Base Optimizations", **kw),
        desc="MQOM base L3 and L5 benchmarks",
    ),
    "onetree":    dict(
        runs=_OT_RUNS,
        formatter=lambda r, **kw: format_mqom_table(r, "MQOM — One-Tree Experiments", **kw),
        desc="MQOM one-tree experiments",
    ),
    "streaming":  dict(
        runs=_STREAMING_RUNS,
        formatter=format_streaming,
        desc="MQOM streaming verification",
    ),
    "presign":    dict(
        runs=_PRESIGN_RUNS,
        formatter=format_presign,
        desc="MQOM pre-signature experiments",
    ),
    "detailed":   dict(
        runs=_DETAILED_RUNS,
        formatter=format_detailed,
        desc="Detailed BLC/PIOP breakdown — all profiles × all instances (BENCHMARK=1)",
    ),
}


# ── CLI ────────────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--table",       choices=list(TABLES.keys()),
                    help="Benchmark table / figure to reproduce.")
    ap.add_argument("--port",
                    help="UART serial port (e.g. /dev/ttyACM0, /dev/ttyUSB0).")
    ap.add_argument("--board", choices=["nucleol4r5zi", "leia"],
                    default="nucleol4r5zi",
                    help="Physical board connected.  'leia' enables hardware-AES runs; "
                         "'nucleol4r5zi' (default) skips them.")
    ap.add_argument("--verbose",      action="store_true",
                    help="Print each serial line as it arrives (default: spinner).")
    ap.add_argument("--no-compile",  action="store_true",
                    help="Skip firmware compilation.")
    ap.add_argument("--no-flash",    action="store_true",
                    help="Skip board flashing.")
    ap.add_argument("--timeout",     type=int, default=DEFAULT_TIMEOUT,
                    help=f"UART capture timeout per run in seconds (default: {DEFAULT_TIMEOUT}).")
    ap.add_argument("--output",      choices=["terminal", "markdown"], default="terminal",
                    help="Output format: 'terminal' (default) or 'markdown' "
                         "(saves <table>-results.md in addition to stdout).")
    ap.add_argument("--filter", action="append", default=[], metavar="TAG",
                    dest="filters",
                    help="Only run/display entries matching TAG (repeatable, AND logic). "
                         "Use --list-filters to see available tags for a table.")
    ap.add_argument("--list-tables", action="store_true",
                    help="List all available tables and exit.")
    ap.add_argument("--list-filters", action="store_true",
                    help="List available filter tags for --table and exit.")
    args = ap.parse_args()

    if args.list_tables:
        print("Available tables:")
        for name, cfg in TABLES.items():
            print(f"  {name:<14}  {cfg['desc']}")
        return

    if not args.table:
        ap.error("--table is required (or use --list-tables)")

    cfg = TABLES[args.table]
    fmt = cfg["formatter"]

    if args.list_filters:
        all_tags = frozenset().union(*(r.get("tags", frozenset()) for r in cfg["runs"]))
        if args.table == "matmul":
            all_tags |= _MATMUL_ALL_IMPL_TAGS
        print(f"Filter tags for '--table {args.table}':")
        for tag in sorted(all_tags):
            print(f"  {tag}")
        return

    if not args.port:
        ap.error("--port is required")

    active_filters = frozenset(f.lower() for f in args.filters)

    # Warn about unknown tags before doing anything
    if active_filters:
        all_run_tags = frozenset().union(*(r.get("tags", frozenset()) for r in cfg["runs"]))
        valid_tags   = all_run_tags | (_MATMUL_ALL_IMPL_TAGS if args.table == "matmul" else frozenset())
        unknown      = active_filters - valid_tags
        if unknown:
            print(f"WARNING: unknown filter tag(s): {', '.join(sorted(unknown))}  "
                  f"— use --list-filters to see valid tags.", file=sys.stderr)

    # Hardware runs (board="leia") are only executed when --board leia is given.
    all_runs = cfg["runs"]
    runs     = all_runs if args.board == "leia" else [
        r for r in all_runs if r["board"] != "leia"
    ]

    # Apply --filter with dimension-aware semantics:
    #   • tags in the SAME dimension are OR-ed  (--filter lut --filter balanced → LUT or Balanced)
    #   • tags across DIFFERENT dimensions are AND-ed (--filter l3 --filter lut → L3 AND LUT)
    if active_filters:
        runs = [r for r in runs if _filter_matches(r.get("tags", frozenset()), active_filters)]

    skipped  = len(all_runs) - len(runs)

    print(f"\n{'='*64}")
    print(f"  Table  : {cfg['desc']}")
    print(f"  Board  : {args.board}"
          + (f"  ({skipped} hardware run(s) skipped — use --board leia to include)"
             if skipped else ""))
    print(f"  Runs   : {len(runs)}")
    print(f"  Port   : {args.port}")
    do_dma_polling = (args.board == "leia" and args.table == "rijndael")
    flags_str = " ".join(f for f in ["no-compile"  if args.no_compile  else "",
                                      "no-flash"    if args.no_flash    else "",
                                      "dma-polling" if do_dma_polling   else ""] if f)
    print(f"  Flags  : {flags_str or '—'}")
    if active_filters:
        # Show interpreted filter groups: OR within a dimension, AND across dimensions
        remaining = set(active_filters)
        groups = []
        for dim in _FILTER_DIMS:
            grp = remaining & dim
            if grp:
                remaining -= grp
                groups.append(f"({' OR '.join(sorted(grp))})" if len(grp) > 1
                               else next(iter(grp)))
        for tag in sorted(remaining):   # unknown-dim tags shown individually (AND)
            groups.append(tag)
        print(f"  Filter : {' AND '.join(groups)}")
    print(f"{'='*64}\n")

    results = {}
    hw_uart_text = None  # kept for DMA/Polling extraction

    print(f"  [serial] opening {args.port} @ {BAUD_RATE} baud (kept open for all runs)")
    with serial.Serial(args.port, BAUD_RATE, timeout=1,
                       dsrdtr=False, rtscts=False, xonxoff=False) as ser:

        for i, run in enumerate(runs, 1):
            label     = run["label"]
            board     = run["board"]
            make_vars = run["make_vars"]
            parser_fn = PARSERS[run["parser"]]

            print(f"[{i}/{len(runs)}] {label}  (board={board})")

            try:
                if not args.no_compile:
                    run_make(board, make_vars)
                if not args.no_flash:
                    run_flash(board)
                reflash_fn = (lambda b=board: run_flash(b)) if not args.no_flash else None
                text = capture_uart(ser, timeout=args.timeout, reflash_fn=reflash_fn,
                                    verbose=args.verbose)
                results[label] = parser_fn(text)
                print(f"  [ok] parsed {len(results[label])} fields\n")

                # Keep raw Hardware output for DMA/Polling extraction
                if do_dma_polling and label == "Hardware":
                    hw_uart_text = text

            except (RuntimeError, TimeoutError, serial.SerialException) as exc:
                print(f"  [ERROR] {exc}\n  Skipping this run.\n", file=sys.stderr)
                results[label] = {}

    # ── DMA vs Polling ────────────────────────────────────────────────────────
    if do_dma_polling:
        if hw_uart_text is None:
            print("WARNING: Hardware run did not complete; DMA/Polling data unavailable.",
                  file=sys.stderr)
        else:
            print("\n[dma-polling] Extracting POLLING and DMA sequences...")
            polling, dma = parse_dma_polling(hw_uart_text)
            save_dma_polling_csv(polling, dma)
            plot_dma_polling(polling, dma)

    # ── Formatted table ───────────────────────────────────────────────────────
    print("\n" + "=" * 64)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        fmt(results, active_filters=active_filters)
    table_output = buf.getvalue()
    sys.stdout.write(table_output)

    if args.output == "markdown":
        md_path = f"{args.table}-results.md"
        with open(md_path, "w") as f:
            f.write(f"# MQOM Benchmark Results\n\n")
            f.write(f"**Table:** {cfg['desc']}  \n")
            f.write(f"**Runs:** {len(runs)}  \n\n")
            f.write(table_output)
        print(f"\n[saved] {md_path}")

    print()


if __name__ == "__main__":
    main()
