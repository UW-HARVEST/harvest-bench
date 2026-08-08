/* Scenario-level result recording for pngvalid's four monolithic test groups.
 *
 * Upstream pngvalid packs thousands of independent scenarios into four
 * functions that stop at the first failure, so wrapping each as a single
 * GoogleTest case yields one pass/fail bit for all of them. This helper lets
 * the suite expose one gtest case per scenario while still executing each
 * upstream group only once.
 *
 * How it works:
 *   - glue_pngvalid.c defines ONE walk per group (harvest_pngvalid_walk) that
 *     reproduces the upstream loop structure and visits every scenario. The
 *     same walk serves two modes, so the names the suite registers and the
 *     scenarios it runs cannot drift apart:
 *       * enumerate — report each scenario's name, run nothing. Used only by
 *         glue/dump_scenarios.c, which regenerates glue/scenario_list.h;
 *       * execute   — run each scenario and report its verdict.
 *   - The recording run happens in a FORKED CHILD that streams its report down
 *     a pipe with unbuffered writes. A crash or hang in the library under test
 *     therefore still leaves a usable partial record in the parent, and —
 *     because the parent survives — every scenario that already ran keeps its
 *     verdict. The fork also contains the walk's side effects: setting up a
 *     png_modifier advances pngvalid's global pseudo-random stream, so running
 *     it in the parent would leave the recording run unable to reproduce a
 *     plain pngvalid run bit for bit.
 *   - Records live in process-local memory, one per group. Every test sharing
 *     the process shares one recording run. gtest_suite/manifest.json batches
 *     each group into one process invocation. Standalone standard-gtest usage
 *     works unchanged, at the cost of re-recording once per invocation.
 *   - Test REGISTRATION reads glue/scenario_list.h and runs nothing at all.
 *     See that file's header for why the scenario names are checked in rather
 *     than enumerated at startup.
 *
 * Verdicts: a scenario that ran without raising a libpng error or warning
 * PASSED, one that raised either FAILED, and one the recording run never
 * reached is NOT_REACHED. NOT_REACHED must be reported as a test failure, not
 * a skip: skipped tests are excluded from the harness's denominator, which
 * would make an early crash score better than a late one.
 *
 * The implementation is C++ (pv_record.cc) but the interface is plain C: the
 * translation unit that includes pngvalid.c must be C, because that source is
 * not valid C++. Nothing may throw across this boundary — an exception
 * unwinding through upstream's C frames would be undefined behavior — so every
 * entry point is noexcept.
 */
#ifndef HARVEST_PV_RECORD_H
#define HARVEST_PV_RECORD_H

#ifdef __cplusplus
#define PV_NOEXCEPT noexcept
extern "C" {
#else
#define PV_NOEXCEPT
#endif

/* The four upstream test groups, in the order pngvalid's main() runs them. */
enum {
    PV_GROUP_STANDARD = 0,
    PV_GROUP_SIZE = 1,
    PV_GROUP_TRANSFORM = 2,
    PV_GROUP_GAMMA = 3,
    PV_NGROUPS = 4
};

typedef enum {
    PV_NOT_REACHED = 0,
    PV_PASSED = 1,
    PV_FAILED = 2
} pv_verdict_t;

/* ---------------------------------------------------------------------
 * Implemented by glue_pngvalid.c (the C side).
 */

/* Visits every scenario of `group`. With `execute` zero it only reports
 * names (used by the scenario-list generator); with `execute` nonzero it runs
 * each scenario and reports verdicts. Returns 0 when the whole group was
 * visited. */
int harvest_pngvalid_walk(int group, int execute) PV_NOEXCEPT;

/* Stable gtest suite name of `group`, e.g. "LibpngSize". */
const char *harvest_pngvalid_group_suite(int group) PV_NOEXCEPT;

/* ---------------------------------------------------------------------
 * Called BY the walk, from inside a recording child.
 */

/* Reports scenario `index`'s name (enumerate mode). */
void pv_emit_name(int index, const char *name) PV_NOEXCEPT;

/* Reports scenario `index`'s verdict (execute mode). `message` may be NULL. */
void pv_emit_result(int index, int failed, const char *message) PV_NOEXCEPT;

/* Reports how many scenarios the walk visited, once it has finished. This is
 * what lets the suite detect a scenario_list.h that upstream has outgrown. */
void pv_emit_count(int count) PV_NOEXCEPT;

/* ---------------------------------------------------------------------
 * Used by the gtest side (the C++ TU).
 */

/* Runs `group` in a forked child once, recording every verdict. `expected` is
 * the group's entry in pv_scenario_counts, which sizes the verdict table so
 * that scenarios the run never reached are still addressable. Idempotent.
 * Returns 0 on success, nonzero if the recording run could not be started. */
int pv_ensure_recorded(int group, int expected) PV_NOEXCEPT;

/* How many scenarios the recording run of `group` reported visiting, or -1 if
 * it did not get that far. Compare against pv_scenario_counts to detect a
 * stale scenario_list.h. */
int pv_recorded_count(int group) PV_NOEXCEPT;

/* Verdict of scenario `index`, valid after pv_ensure_recorded(). */
pv_verdict_t pv_verdict(int group, int index) PV_NOEXCEPT;

/* Failure message of scenario `index` ("" when it passed or never ran). */
const char *pv_failure(int group, int index) PV_NOEXCEPT;

/* Nonzero if the recording run of `group` completed without dying. */
int pv_run_completed(int group) PV_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#endif /* HARVEST_PV_RECORD_H */
