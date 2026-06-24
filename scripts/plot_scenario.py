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
    args = ap.parse_args()

    rows = load(args.csv_path)
    cycle = [r["cycle"] for r in rows]
    book_mid = [r["book_mid"] for r in rows]
    pnl = [r["mtm_pnl"] for r in rows]

    fills = [r for r in rows if r["fill_side"] != -1]
    fill_cycle = [r["cycle"] for r in fills]
    fill_price = [r["fill_price"] for r in fills]
    fill_color = ["tab:green" if r["fill_side"] == 0 else "tab:red" for r in fills]  # Bid=buy, Ask=sell

    plt.style.use("seaborn-v0_8-whitegrid")
    title = args.title or args.csv_path
    fig, (ax_price, ax_pnl) = plt.subplots(2, 1, sharex=True, figsize=(6, 4.2))

    ax_price.plot(cycle, book_mid, color="0.5", linewidth=0.8)
    ax_price.scatter(fill_cycle, fill_price, c=fill_color, s=6, linewidths=0, zorder=3)
    ax_price.set_ylabel("price")
    ax_price.set_title(title, fontsize=11)

    ax_pnl.plot(cycle, pnl, color="tab:green", linewidth=1.2)
    ax_pnl.axhline(0, color="0.7", linewidth=0.6)
    ax_pnl.set_ylabel("PnL")
    ax_pnl.set_xlabel("cycle")

    for ax in (ax_price, ax_pnl):
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
