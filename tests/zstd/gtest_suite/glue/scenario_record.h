/* Scenario-level result recording for monolithic upstream unit-test functions.
 *
 * Upstream packs dozens of independent scenarios into one function that aborts
 * at the first failure, so wrapping it as a single GoogleTest case yields one
 * pass/fail bit for all of them. This helper lets the suite expose one gtest
 * case per scenario while still executing the upstream function only once.
 *
 * How it works:
 *   - The glue hooks the upstream progress printf and calls sr_scenario_start()
 *     as each scenario's banner is emitted (project-specific parsing lives in
 *     the suite, next to the code it describes).
 *   - sr_run_once() executes the upstream function in a FORKED CHILD, which
 *     streams each scenario start down a pipe with unbuffered writes. A child
 *     crash therefore leaves a usable partial record in the parent, and —
 *     because the parent survives — every scenario is still attributed
 *     correctly. The child is killed if it exceeds the time limit, so a
 *     hanging library also yields a record instead of stalling the run.
 *   - The parsed record lives in process-local memory, keyed by tag; every
 *     test sharing the process shares one recording run. Under the HARVEST
 *     harness, gtest_suite/manifest.json batches a recording test and its
 *     per-scenario tests into one process invocation. Standalone standard
 *     gtest usage works unchanged (a separate single-case invocation simply
 *     re-records its tag once).
 *
 * Verdicts: a scenario that started and was followed by another start (or by
 * the run completing) PASSED; the last one started before an abort FAILED;
 * scenarios never started were NOT_REACHED. NOT_REACHED must be reported as a
 * test failure, not a skip: skipped tests are excluded from the harness's
 * denominator, which would make an early crash score better than a late one.
 *
 * The implementation is C++ (scenario_record.cc) but the interface is plain C:
 * the translation units that include the upstream test sources must be C,
 * because those sources are not valid C++ (implicit void* conversions, and
 * potentially C-only constructs upstream may add later). Nothing may throw
 * across this boundary — an exception unwinding through upstream's C frames
 * would be undefined behaviour — so every entry point is noexcept.
 */
#ifndef HARVEST_SCENARIO_RECORD_H
#define HARVEST_SCENARIO_RECORD_H

#ifdef __cplusplus
#define SR_NOEXCEPT noexcept
extern "C" {
#else
#define SR_NOEXCEPT
#endif

#define SR_MAX_SCENARIOS 1024

typedef enum {
    SR_NOT_REACHED = 0,
    SR_PASSED = 1,
    SR_FAILED = 2
} sr_verdict_t;

/* Called from the suite's print hook when scenario `id` begins. No-op unless a
 * recording run is active. */
void sr_scenario_start(int id) SR_NOEXCEPT;

/* Ensure the record for `tag` exists, running `body` in a forked child if it
 * does not (or if the cached record is stale). `any_symbol` is any function
 * pointer from the library under test; its file's identity keys the cache.
 * Returns 0 on success, nonzero if the record could not be produced. */
int sr_run_once(const char *tag, int (*body)(void), void *any_symbol) SR_NOEXCEPT;

/* Verdict for scenario `id`, valid after sr_run_once(). */
sr_verdict_t sr_verdict(int id) SR_NOEXCEPT;

/* Highest scenario id observed in the record (-1 if none). Lets a suite detect
 * that upstream grew scenarios its recorded list does not know about. */
int sr_max_observed(void) SR_NOEXCEPT;

/* Nonzero if the recorded run completed without aborting. */
int sr_run_completed(void) SR_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#endif /* HARVEST_SCENARIO_RECORD_H */
