# ADS1115 Chunked Hardening Prompt Sequence

Use these prompts one by one in order. Each prompt is intended to be a bounded work chunk. The coding agent should spawn subagents, make focused changes, run the requested validation, commit, and sync after each chunk.

Do not skip straight to broad refactoring. The audit says the architecture is already strong; the remaining work is precision hardening, tests, validation evidence, and release honesty.

Recommended sequence:

1. `01_branch_baseline_and_rules.md`
2. `02_p0_test_design_for_status_and_partial_state.md`
3. `03_p0_core_status_and_begin_dirty_implementation.md`
4. `04_p0_raw_register_cache_dirty_contract.md`
5. `05_p1_api_contracts_tick_nowms_blocking.md`
6. `06_p1_tests_guards_and_docs_polish.md`
7. `07_integration_examples_ci_and_hil_matrix.md`
8. `08_final_report_release_readiness.md`

After every prompt:
- run the specified checks;
- update the per-prompt report section;
- commit with a clear message;
- push/sync the branch;
- stop and report if anything is ambiguous or unsafe.
