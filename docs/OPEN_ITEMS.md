# Open Release And Integration Items

Last reviewed: 2026-07-22 against ADS1115 v2.0.0 (`dd25d61`)

This is the single index of unfinished repository-level work. Completed
hardening reports are under [`archive/`](archive/), and dated hardware evidence
is under [`evidence/hil/`](evidence/hil/).

The v2.0 release review recorded no unresolved high- or medium-severity core
software blocker. The remaining work is target evidence and product
integration, not a known core-driver implementation defect.

## Physical Release Evidence

A clean, dated v2.0 ESP32-S2 diagnostic run is recorded in
[`evidence/hil/2026-07-22_COM6/README.md`](evidence/hil/2026-07-22_COM6/README.md).
Only the following unfinished physical and final-product evidence remains:

- [ ] Copy the ignored 2026-07-22 raw captures and flashed firmware into an
  immutable external archive, preserving the paths, sizes, and SHA-256 hashes
  recorded by the evidence summary.
- [ ] Validate final-board address straps, supply, pull-ups, bus speed, analog
  source impedance, protection, input limits, accuracy, saturation,
  disconnection, and calibration for every supported production profile.
- [ ] Validate fault and recovery behavior on target hardware: contention,
  unplug/replug, stuck bus, reset/brownout, delayed or ambiguous transfers,
  cancellation reconciliation, and profile replay.
- [ ] Validate shared-bus fairness, deadlines, and the final task workload at
  production cadence, including a reviewable soak.
- [ ] Run native ESP-IDF hardware validation on each ESP32-S2/S3 target claimed
  by the release.
- [ ] If a production profile enables comparator or ALERT/RDY, capture its
  electrical and timing matrix. This is not a gate for profiles that keep the
  feature disabled.

The execution procedure and evidence fields live in
[`ADS1115_HARDWARE_VALIDATION_PLAN.md`](ADS1115_HARDWARE_VALIDATION_PLAN.md) and
the results template. Do not paste long serial transcripts into review-facing
documents; retain compact outcomes and hashes for any archived raw artifacts.

## TunnelMonitor-node

The product role, board/channel profile, result meaning, firmware integration,
capacity changes, and final-board acceptance remain open. The actionable list
is in
[`TUNNELMONITOR_NODE_SUITABILITY_AUDIT.md`](TUNNELMONITOR_NODE_SUITABILITY_AUDIT.md).

## CI Maintenance

- [ ] Update the GitHub Actions revisions that still declare the deprecated
  Node 20 runtime. The v2.0.0 release run succeeded, but GitHub emitted runtime
  warnings for `actions/checkout@v4`, `actions/setup-python@v5`, and
  `actions/cache@v4`.

When an item is completed, remove it from this page and record durable history
in the changelog, a dated validation summary, or the archive as appropriate.
