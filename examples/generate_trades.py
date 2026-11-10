#!/usr/bin/env python3
"""Writes examples/trades.csv: 200 synthetic trades, deterministic for a fixed seed.

Columns: ts (milliseconds since midnight, int), symbol, side, price (double), qty (int).
Prices follow a small random walk around a per-symbol reference price.
"""
import pathlib
import random

SEED = 281
TRADES = 200
REFERENCE_PRICES = {
    "AAPL": 189.50,
    "MSFT": 421.00,
    "NVDA": 131.20,
    "AMZN": 186.40,
    "GOOG": 171.80,
}
LOT_SIZES = [100, 100, 100, 200, 300, 500, 1000]
MARKET_OPEN_MS = 9 * 3_600_000 + 30 * 60_000


def main() -> None:
    rng = random.Random(SEED)
    prices = dict(REFERENCE_PRICES)
    ts = MARKET_OPEN_MS
    rows = ["ts,symbol,side,price,qty"]
    for _ in range(TRADES):
        ts += rng.randint(50, 2_000)
        symbol = rng.choice(sorted(prices))
        prices[symbol] = round(prices[symbol] * (1 + rng.gauss(0, 0.0015)), 2)
        side = rng.choice(["BUY", "SELL"])
        qty = rng.choice(LOT_SIZES)
        rows.append(f"{ts},{symbol},{side},{prices[symbol]:.2f},{qty}")
    out = pathlib.Path(__file__).with_name("trades.csv")
    out.write_text("\n".join(rows) + "\n")


if __name__ == "__main__":
    main()
