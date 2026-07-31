# Open Release And Integration Items

Last reviewed: 2026-07-31 for the pioarduino `55.03.311` upgrade
(runtime library version 2.0.0).

This is the single index of unfinished repository-level work. Dated hardware
evidence is under [`evidence/hil/`](evidence/hil/).

The v2.0 release review recorded no unresolved high- or medium-severity core
software blocker. The remaining work is target evidence and product
integration, not a known core-driver implementation defect.

## Physical Release Evidence

A clean, dated v2.0 ESP32-S2 diagnostic run is recorded in
<a href="evidence/hil/2026-07-22_COM6/README.md">the COM6 evidence summary</a>.
Only the following unfinished physical and final-product evidence remains:

- [ ] Requalify the Arduino diagnostic surface on pioarduino `55.03.311` and
  record exact runtime Arduino-ESP32 `3.3.11` / ESP-IDF `v5.5.5` identity.
  Include ESP32-S2/S3 CLI framing, repeated-start reads, expected-absent NACKs,
  bounded timeout/recovery behavior, and a reviewable stress/soak run.
- [ ] Validate the intended tagged release artifact with matching clean runtime
  identity. The COM6 baseline used clean post-tag commit `dd25d61`, not the
  `v2.0.0` tag at `785515a`.
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
documents or retain them in the workspace; keep only compact dated outcomes.

## TunnelMonitor-node

The ADS1115 library-side owner API is ready, but TunnelMonitor firmware has not
been integrated. Start with a fixed compile-time profile, single-shot reads,
CONFIG OS-bit polling, comparator/ALERT-RDY disabled, and at most one I2C
callback per normal owner-task poll.

### Product and board decisions

- [ ] Decide whether ADS1115 replaces the INA228 measurement meaning or is a
  distinct source.
- [ ] Freeze the board revision, required/optional population, address strap,
  supply, pull-ups, bus rate, and each channel's application channel ID, MUX,
  PGA, data rate, source impedance, protection, filtering, and safe input range.
- [ ] Define the published unit, polarity, valid range, calibration and
  persistence, freshness/skew, disconnect behavior, and saturation behavior.
- [ ] Approve fixed capacities and acceptance criteria for measurement, health,
  settings, storage/replay, display, and cloud output. The reviewed legacy
  limits were 4 source entries, 16 device-health rows, and 11 free numeric slots
  in the selected 48-slot sample schema; recheck them against the target branch.

### Firmware integration

- [ ] Add the selected source/device/operation/result identities with
  append-only enum changes wherever compatibility requires them.
- [ ] Validate exact command identity at the module boundary and reject stale,
  mismatched, duplicate, or otherwise unrelated results.
- [ ] Add the smallest adapter around the owner-safe lifecycle. Keep the
  TunnelMonitor I2C task as sole bus owner and bridge each ADS callback to one
  non-retrying transfer seam; do not call a retrying/recovering transfer helper
  inside a mutating ADS callback because ambiguous writes must remain visible.
- [ ] Probe address presence separately. Until the transport distinguishes
  address and data NACK, map a generic mutating-transfer NACK conservatively to
  `I2C_NACK_DATA` or `I2C_ERROR`, never a definite address NACK.
- [ ] Consume token-matched raw code, microvolts, channel, MUX, PGA, data rate,
  configuration generation, and quality provenance inside the module. Publish
  only the normalized result and validity/stale/error masks selected by the
  product schema.
- [ ] Implement checked board-level scaling and calibration outside this
  library, including divider/shunt/amplifier/offset policy.
- [ ] Define required/optional failure, partial multi-channel acquisition,
  queue expiry, deadline, cancellation, recovery, and profile-replay behavior.
  Availability requires a successful terminal result plus verified clean
  configuration, not `DriverState::READY` alone. Reconcile a cancelled started
  conversion through the bus-silent quiet interval before reusing its slot.
- [ ] Add boundary tests for every affected fixed-capacity contract, including
  storage/replay and public output.
- [ ] Keep the integration result trivially copyable and at most 128 bytes;
  validate reserved bytes and reject nonzero or unsupported encodings.
- [ ] Prove no steady-state allocation and bound owner-task stack use, adapter
  state, and all result storage.
- [ ] Verify exactly one terminal result for every accepted operation across
  success, transport failure, timeout, and cancellation paths.
- [ ] If ADS1115 replaces INA228, add replacement-equivalence tests for every
  affected measurement, availability, health, storage/replay, display, and
  cloud contract.

### Final-board acceptance

- [ ] Demonstrate the complete I2C population at the selected bus rate with
  owner fairness, deadlines, and production task cadence.
- [ ] Measure known references near zero and at representative/range-limit
  points for every selected path, including negative differential input where
  applicable.
- [ ] Verify analog scaling, calibration tolerance, channel order, acquisition
  skew, clipping, saturation, and disconnected-input behavior.
- [ ] Verify required/optional absence, unplug/replug, reset/brownout, held-bus
  recovery, and profile replay on final hardware.
- [ ] Run the final measurement, storage, display, web, and cloud workload,
  followed by a several-hour or overnight soak with a compact dated result.

## CI Maintenance

- [ ] Update the GitHub Actions revisions that still declare the deprecated
  Node 20 runtime. The v2.0.0 release run succeeded, but GitHub emitted runtime
  warnings for `actions/checkout@v4`, `actions/setup-python@v5`, and
  `actions/cache@v4`.

When an item is completed, remove it from this page and record durable history
only when it belongs in the changelog or a compact dated validation summary.
