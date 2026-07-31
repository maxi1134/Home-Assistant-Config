#!/usr/bin/env python3
"""Solve the bed scale's per-corner gains from calibration captures.

Companion to bed-weight-sensor.yaml. Requires numpy (pip install numpy).

Procedure
---------
1. Bed empty (bedding on) -> press "Tare" in Home Assistant.
2. Set "Calibration Known Weight" to the helper person's weight in lb
   (weigh them on a decent bathroom scale).
3. The person gets on the bed, holds still ~15 s, then press "Capture
   Calibration Sample". Best positions: KNEEL directly ON each of the four
   corners (concentrates the load) with feet and hands OFF the floor - any
   weight leaking to the floor corrupts the fit - then lie center, left
   edge and right edge. 6+ captures total. Re-tare (empty bed) if the
   session runs long, and re-capture after any re-tare.
4. Each capture publishes a CSV row in the "Calibration Sample" text sensor
   (also in the ESPHome log): tl,tr,bl,br,known_lb
   Paste the rows into a text file, one per line. Lines starting with #
   and blank lines are ignored.
5. Run:  python bed-scale-calibrate.py samples.csv
6. Enter the recommended gains into the "Gain ..." number entities, then
   verify: lie in the center -> Total Weight should read the known weight
   within ~2%.

The script fits three nested models and recommends the richest one the data
actually supports (the 4-gain fit through a mattress is often ill-conditioned;
tiny residuals alone do NOT mean the gains are right, so conditioning and
ridge-stability are checked too):
  1 gain  - one shared scale factor (always well-posed)
  2 gains - left pair / right pair (preserves the axis used for side occupancy)
  4 gains - full per-corner trim, ridge-regularized toward the 1-gain solution
"""

import sys

import numpy as np

COND_LIMIT_4GAIN = 100.0   # above this, the 4-gain directions are noise
GAIN_BAND = (0.5, 2.0)     # plausible range for a solved gain
RIDGE_ALPHA = 1e-2         # ridge strength relative to trace(D'D)/4
STABILITY_ALPHAS = (1e-3, 1e-2, 1e-1)
STABILITY_TOL = 0.05       # max relative gain change across alphas

CORNERS = ["Top Left", "Top Right", "Bottom Left", "Bottom Right"]


def load_samples(path):
    rows = []
    try:
        fh = open(path, encoding="utf-8-sig")  # -sig: tolerate Notepad's BOM
    except OSError as e:
        sys.exit(str(e))
    with fh:
        for lineno, line in enumerate(fh, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = [p.strip() for p in line.split(",")]
            if len(parts) != 5:
                sys.exit(f"{path}:{lineno}: expected 5 comma-separated values "
                         f"(tl,tr,bl,br,known_lb), got {len(parts)}: {line!r}")
            try:
                rows.append([float(p) for p in parts])
            except ValueError:
                sys.exit(f"{path}:{lineno}: non-numeric value in: {line!r}")
    if not rows:
        sys.exit(f"{path}: no samples found")
    data = np.array(rows, dtype=np.float64)
    return data[:, :4], data[:, 4]


def fit_1gain(D, W):
    sums = D.sum(axis=1)
    den = float(np.dot(sums, sums))
    if den == 0.0:
        sys.exit("all captures sum to zero - check wiring/tare and re-capture")
    return np.full(4, float(np.dot(sums, W) / den))


def fit_2gain(D, W):
    D2 = np.column_stack([D[:, 0] + D[:, 2], D[:, 1] + D[:, 3]])  # left, right
    g, _, rank, _ = np.linalg.lstsq(D2, W, rcond=None)
    if rank < 2:
        return None  # left/right sums collinear -> min-norm split is meaningless
    return np.array([g[0], g[1], g[0], g[1]])


def fit_4gain(D, W, k0, alpha):
    lam = alpha * np.trace(D.T @ D) / 4.0
    return np.linalg.solve(D.T @ D + lam * np.eye(4), D.T @ W + lam * k0)


def residuals(D, W, k):
    return D @ k - W


def report_model(name, D, W, k):
    r = residuals(D, W, k)
    rms = float(np.sqrt(np.mean(r**2)))
    mx = float(np.max(np.abs(r)))
    print(f"\n{name}")
    print(f"  gains: " + "  ".join(f"{c}={v:.4f}" for c, v in zip(["TL", "TR", "BL", "BR"], k)))
    print(f"  residuals per capture (predicted - known, lb): "
          + "  ".join(f"{v:+.2f}" for v in r))
    print(f"  RMS residual: {rms:.2f} lb   worst: {mx:.2f} lb")
    return rms, mx


def main():
    if len(sys.argv) == 2 and sys.argv[1] in ("-h", "--help"):
        print(__doc__)
        sys.exit(0)
    if len(sys.argv) != 2:
        sys.exit("usage: python bed-scale-calibrate.py samples.csv   (-h for the full procedure)")
    D, W = load_samples(sys.argv[1])
    n = len(W)

    if not D.any():
        sys.exit("all corner readings are zero - check wiring/tare and re-capture")

    print(f"Loaded {n} captures.")

    # Statics: the summed reading must match the known weight at every
    # position, so per-capture scales W/sum should agree within a few %.
    sums = D.sum(axis=1)
    if np.all(sums > 0) and n >= 2:
        row_scale = W / sums
        spread = float(row_scale.max() / row_scale.min())
        if spread > 1.2:
            print(f"WARNING: capture totals disagree by {100 * (spread - 1):.0f}% - some "
                  "weight likely bypassed the bed (feet on the floor? frame touching a "
                  "wall?). Re-capture before trusting any fit below.")
    if n < 4:
        print("WARNING: fewer than 4 captures - only the 1-gain fit is meaningful.")
    elif n < 6:
        print("WARNING: fewer than 6 captures - the 4-gain fit will be fragile. "
              "Capture more positions if you can.")

    sv = np.linalg.svd(D, compute_uv=False)
    cond = float(sv[0] / sv[-1]) if sv[-1] > 0 else np.inf
    print(f"\nSingular values of the capture matrix: "
          + "  ".join(f"{v:.2f}" for v in sv))
    if n >= 4:
        print(f"Condition number: {cond:.1f} "
              f"({'OK' if cond < COND_LIMIT_4GAIN else 'ILL-CONDITIONED for a 4-gain fit'})")
    else:
        cond = np.inf
        print("Condition number: undefined (fewer captures than gains; 4-gain fit not possible)")

    k1 = fit_1gain(D, W)
    rms1, _ = report_model("[1 gain] shared scale", D, W, k1)

    k2 = None
    rms2 = rms1
    if n >= 3:
        k2 = fit_2gain(D, W)
        if k2 is None:
            print("\n[2 gains] skipped: left/right sums are collinear across captures - "
                  "capture distinct left- and right-side positions")
        else:
            rms2, _ = report_model("[2 gains] left pair / right pair", D, W, k2)

    k4 = None
    rms4 = None
    stable = False
    if n >= 4:
        k4 = fit_4gain(D, W, k1, RIDGE_ALPHA)
        rms4, _ = report_model(f"[4 gains] per-corner (ridge alpha={RIDGE_ALPHA})", D, W, k4)
        variants = np.array([fit_4gain(D, W, k1, a) for a in STABILITY_ALPHAS])
        spread = float(np.max(np.abs(variants - k4) / np.maximum(np.abs(k4), 1e-9)))
        stable = spread < STABILITY_TOL
        print(f"  ridge stability: max gain change across alpha={STABILITY_ALPHAS} "
              f"is {100 * spread:.1f}% ({'stable' if stable else 'UNSTABLE - do not trust'})")

    # Pick the richest model the data supports, explaining rejections.
    def sane(k):
        return bool(np.all((k >= GAIN_BAND[0]) & (k <= GAIN_BAND[1])))

    print()
    ok4 = k4 is not None
    if ok4 and cond >= COND_LIMIT_4GAIN:
        print(f"4-gain rejected: condition number {cond:.0f} >= {COND_LIMIT_4GAIN:.0f}")
        ok4 = False
    if ok4 and not sane(k4):
        print(f"4-gain rejected: a gain is outside {GAIN_BAND}")
        ok4 = False
    if ok4 and not stable:
        print("4-gain rejected: ridge-unstable (gains change with regularization strength)")
        ok4 = False
    if ok4 and rms4 > rms2 * 1.05 + 0.01:
        print("4-gain rejected: does not fit better than the 2-gain model")
        ok4 = False

    ok2 = k2 is not None
    if not ok4 and ok2:
        if not sane(k2):
            print(f"2-gain rejected: a gain is outside {GAIN_BAND}")
            ok2 = False
        elif rms2 > rms1 * 1.05 + 0.01:
            print("2-gain rejected: does not fit better than the shared scale")
            ok2 = False

    if ok4:
        pick, kbest = "4-gain", k4
    elif ok2:
        pick, kbest = "2-gain", k2
    else:
        pick, kbest = "1-gain", k1
        if not sane(k1):
            print("WARNING: even the shared scale is far from 1.0 - check the "
                  "nominal multiply factor in the YAML, the tare, and the wiring.")

    print(f"\n=== RECOMMENDED: {pick} model ===")
    print("Enter these in Home Assistant:")
    for corner, v in zip(CORNERS, kbest):
        print(f"  Gain {corner}: {v:.4f}")
    print("\nThen verify: lie in the center of the bed -> Total Weight should "
          "read the known weight within ~2%.")


if __name__ == "__main__":
    main()
