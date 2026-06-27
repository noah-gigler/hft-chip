#!/usr/bin/env python3
"""Plot a directed trader scenario CSV (tb/*_scenario.csv) for the report.

Generic over the scenario CSV schema shared by tb_hft_chip.cpp's
scenario_*() functions: cycle,mid_target,book_mid,fill_side,fill_qty,
fill_price,position,mtm_pnl,error,pending,state. Works unchanged for the
ema/momentum/arb scenarios since they all write this same column layout.

Usage: python3 scripts/plot_scenario.py tb/ema_scenario.csv [--title EMA] [--out report/figures/ema_scenario.png]
"""
import argparse
import csv

import matplotlib.pyplot as plt


def load(csv_path):
    rows = []
    with open(csv_path, newline="") as f:
        for row in csv.DictReader(f):
            rows.append({k: float(v) for k, v in row.items()})
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv_path")
    ap.add_argument("--title", default=None, help="Trader name for the plot title (e.g. EMA)")
    ap.add_argument("--out", default=None, help="Save to file instead of showing interactively")
    ap.add_argument("--two-mids", action="store_true",
                     help="Plot mid_target and book_mid as two separate series (e.g. arb's two markets). "
                          "Default plots only book_mid, for scenarios where mid_target is just a synthetic "
                          "target the book tracks closely (e.g. EMA's OU process).")
    ap.add_argument("--imbalance", action="store_true",
                     help="Add a third panel plotting bid_qty (positive) and ask_qty (negative, mirrored) "
                          "from the CSV's bid_qty/ask_qty columns -- the resting-quantity imbalance signal "
                          "momentum_trader reacts to, shown leading the price panel above it.")
    args = ap.parse_args()

    rows = load(args.csv_path)
    cycle = [r["cycle"] for r in rows]
    mid_target = [r["mid_target"] for r in rows]
    book_mid = [r["book_mid"] for r in rows]
    pnl = [r["mtm_pnl"] for r in rows]

    fills = [r for r in rows if r["fill_side"] != -1]
    # anchor each fill marker to the cycle its order was QUOTED at (fill_cycle),
    # not the cycle the fill happened to land at -- by fill time, latency means
    # the live mid has usually moved on, so the marker would float off the line
    fill_cycle = [r.get("fill_cycle", r["cycle"]) for r in fills]
    fill_price = [r["fill_price"] for r in fills]
    fill_color = ["tab:green" if r["fill_side"] == 0 else "tab:red" for r in fills]  # Bid=buy, Ask=sell

    plt.style.use("seaborn-v0_8-whitegrid")
    title = args.title or args.csv_path

    if args.two_mids:
        # one panel per market (its own mid + only the fills that landed on that
        # market's book, per fill_market), plus the shared PnL panel below
        fig, axes = plt.subplots(3, 1, sharex=True, figsize=(6, 6.0))
        ax_m0, ax_m1, ax_pnl = axes
        ax_m0.plot(cycle, mid_target, color="tab:blue", linewidth=0.9)
        ax_m1.plot(cycle, book_mid, color="tab:orange", linewidth=0.9)
        for ax, market in ((ax_m0, 0), (ax_m1, 1)):
            mc = [c for c, r in zip(fill_cycle, fills) if r["fill_market"] == market]
            mp = [p for p, r in zip(fill_price, fills) if r["fill_market"] == market]
            mcol = [col for col, r in zip(fill_color, fills) if r["fill_market"] == market]
            ax.scatter(mc, mp, c=mcol, s=6, linewidths=0, zorder=3)
        ax_m0.set_ylabel("price")
        ax_m1.set_ylabel("price")
        ax_m0.set_xlabel("market0", fontsize=9)
        ax_m1.set_xlabel("market1", fontsize=9)
        ax_m0.xaxis.set_label_position("bottom")
        ax_m1.xaxis.set_label_position("bottom")
        ax_m0.set_title(title, fontsize=11)
        ax_pnl.plot(cycle, pnl, color="tab:green", linewidth=1.2)
        ax_pnl.axhline(0, color="0.7", linewidth=0.6)
        ax_pnl.set_ylabel("PnL")
        ax_pnl.set_xlabel("cycle")
        for ax in axes:
            ax.spines["top"].set_visible(False)
            ax.spines["right"].set_visible(False)
        fig.tight_layout()
        if args.out:
            fig.savefig(args.out, dpi=150)
            print(f"wrote {args.out}")
        else:
            plt.show()
        return

    nrows = 3 if args.imbalance else 2
    fig, axes = plt.subplots(nrows, 1, sharex=True, figsize=(6, 4.2 if nrows == 2 else 5.4))
    if args.imbalance:
        ax_imb, ax_price, ax_pnl = axes
    else:
        ax_price, ax_pnl = axes

    ax_price.plot(cycle, book_mid, color="tab:blue", linewidth=0.8)
    ax_price.scatter(fill_cycle, fill_price, c=fill_color, s=6, linewidths=0, zorder=3)
    ax_price.set_ylabel("price")

    if args.imbalance:
        imbalance = [r["bid_qty"] - r["ask_qty"] for r in rows]
        # raw per-cycle imbalance is high-frequency noise (it's driven by a fast
        # mean-reverting OU process); the slow trend that actually correlates with
        # price lives in its rolling average, which is what the trader's cumulative
        # impact on price tracks -- plot that instead of the noisy raw signal
        window = 50
        smoothed = []
        acc = 0.0
        for idx, v in enumerate(imbalance):
            acc += v
            if idx >= window:
                acc -= imbalance[idx - window]
            smoothed.append(acc / min(idx + 1, window))
        ax_imb.plot(cycle, smoothed, color="tab:purple", linewidth=1.1)
        ax_imb.axhline(0, color="0.7", linewidth=0.6)
        ax_imb.set_ylabel(f"imbalance\n({window}-cyc avg)")
        ax_imb.set_title(title, fontsize=11)
    else:
        ax_price.set_title(title, fontsize=11)

    ax_pnl.plot(cycle, pnl, color="tab:green", linewidth=1.2)
    ax_pnl.axhline(0, color="0.7", linewidth=0.6)
    ax_pnl.set_ylabel("PnL")
    ax_pnl.set_xlabel("cycle")

    for ax in axes:
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)

    fig.tight_layout()
    if args.out:
        fig.savefig(args.out, dpi=150)
        print(f"wrote {args.out}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
