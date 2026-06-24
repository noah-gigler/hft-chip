# TODO

- Verification gap: golden models in `sw/` are transcribed from the RTL, so the
  testbenches prove DUT == model (self-consistency), not spec-correctness. Only
  `anchor_checks()` are independent. A real fix is an independent spec the RTL
  and TB are each derived from. `spec.md` is a start but is still thin.
- More directed `.vec` tests with hand-crafted expected outputs.
- Run the SW models standalone and compare runtime against the chip latency.
