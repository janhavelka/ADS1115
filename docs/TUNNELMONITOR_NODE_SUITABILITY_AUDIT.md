# TunnelMonitor-node Integration Gates

Last reviewed: 2026-07-22 against ADS1115 v2.0.0 (`785515a`)

## Status

The ADS1115 library-side findings from the v1.2.0 suitability audit are closed
by the v2.0 owner-safe API. The original audit, findings, implementation
disposition, and dated evidence are preserved in
`docs/archive/audits/TUNNELMONITOR_NODE_SUITABILITY_AUDIT_v1.2.0_2026-07-18.md`.

TunnelMonitor integration is still blocked by product and board decisions. No
TunnelMonitor firmware contract was changed as part of the ADS1115 v2.0 work.

Recommended first scope: a fixed compile-time profile, single-shot reads,
CONFIG OS-bit polling, comparator and ALERT/RDY disabled, and one I2C callback
per owner-task poll.

## Open Product And Board Decisions

- [ ] Decide whether ADS1115 replaces the existing INA228 measurement meaning
  or is a distinct analog source.
- [ ] Select the target board revision, required/optional population, ADS1115
  address/ADDR strap, supply, I2C pull-ups, and bus rate.
- [ ] Define every populated channel: application channel ID, MUX, PGA, sample
  rate, source impedance, protection, filtering, and allowed input range.
- [ ] Define the product result meaning: engineering unit, polarity, valid
  range, calibration, calibration persistence, freshness/skew, disconnect
  behavior, and saturation behavior.
- [ ] Approve the selected-profile capacities and acceptance criteria for
  measurement, health, settings, storage, replay, display, and cloud output.

## Open TunnelMonitor Implementation Work

Begin this work only after the product and board profile above is frozen.

- [ ] Add the selected device/source/operation/result identities using
  append-only enum changes where compatibility requires them.
- [ ] Add the smallest adapter around the v2.0 owner-safe lifecycle. Keep the
  TunnelMonitor I2C task as the sole bus owner and allow one callback per normal
  poll.
- [ ] Publish exactly one token-matched terminal result containing raw code,
  input microvolts, channel, MUX, PGA, data rate, configuration generation, and
  validity/quality information.
- [ ] Implement checked conversion from ADC-input microvolts to the approved
  product unit. Board-divider, shunt, amplifier, offset, and calibration policy
  remain outside the ADS1115 library.
- [ ] Define required-versus-optional failure behavior, partial multi-channel
  acquisition, queue expiry, deadline, cancellation, and recovery policy.
- [ ] Add capacity and maximum-size tests for every affected fixed-size
  contract, including storage/replay and public output when applicable.

## Open Final-Board Evidence

- [ ] Demonstrate the ADS1115 and the full I2C population together at the
  selected bus rate, including owner fairness and deadlines.
- [ ] Measure known references near zero and at representative/range-limit
  points for every selected signal path; include negative differential input
  when a differential MUX is used.
- [ ] Verify analog-front-end scaling, calibration tolerance, channel order,
  first-to-last skew, clipping, saturation, and disconnected-input behavior.
- [ ] Verify required/optional absence, unplug/replug, reset/brownout, profile
  replay, and held-bus recovery on the final hardware.
- [ ] Run the final measurement, storage, display, web, and cloud workload at
  production cadence, followed by a several-hour or overnight soak with
  concise, dated, reviewable evidence.

Use [`ADS1115_HARDWARE_VALIDATION_PLAN.md`](ADS1115_HARDWARE_VALIDATION_PLAN.md)
for the physical evidence format. Keep raw console transcripts out of this
document; store only the compact result, fixture identity, failures, artifact
hashes, and links required to reproduce or inspect the run.
