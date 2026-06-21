# Spec

## Price encoding

- Price is `PRICE_WIDTH`-bit (0..511).
- `0` (bid) and `511` (ask) are **reserved sentinels** marking empty levels.
- Valid tradable prices: **`[1, 510]`**.
- Inserts at a sentinel price are **dropped silently** (no error, no state change).
