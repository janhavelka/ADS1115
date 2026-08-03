# Open Validation Items

Last reviewed: 2026-08-03 against ADS1115 2.0.0 and pioarduino
`55.03.311`.

There is no known unresolved high- or medium-severity core-driver defect. This
page contains only evidence still needed before making broader hardware or
product claims.

## Hardware and integration evidence

- [ ] Requalify the Arduino diagnostic surface on ESP32-S2 and ESP32-S3 using
  pioarduino `55.03.311`, with runtime Arduino-ESP32 `3.3.11` / ESP-IDF
  `v5.5.5` identity, CLI framing, repeated-start reads, expected-absent NACKs,
  bounded timeout/recovery behavior, and a reviewable stress/soak run.
- [ ] Validate final-board address straps, supply, pull-ups, bus speed, analog
  source impedance, protection, input limits, accuracy, saturation,
  disconnection, and calibration for every supported production profile.
- [ ] Validate fault and recovery behavior on target hardware: contention,
  unplug/replug, stuck bus, reset/brownout, delayed or ambiguous transfers,
  cancellation reconciliation, and profile replay.
- [ ] Validate shared-bus fairness, deadlines, cancellation, and the final task
  workload at production cadence, including a reviewable endurance run.
- [ ] Run native ESP-IDF hardware validation on every ESP32-S2/S3 target
  claimed by a release.
- [ ] If a production profile enables comparator or ALERT/RDY, capture its
  electrical and timing matrix. This is not a gate for profiles that keep the
  feature disabled.

Use [`ADS1115_HARDWARE_VALIDATION_PLAN.md`](ADS1115_HARDWARE_VALIDATION_PLAN.md)
and [`ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md`](ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md).
Historical HIL transcripts, generated summaries, and superseded fixture notes
are intentionally not retained in this repository. Store required lab evidence
in the approved evidence system and record only its stable reference in a
dated result.

When an item is completed, remove it from this page. Record durable behavior or
compatibility changes in `CHANGELOG.md`; do not accumulate completed work here.
