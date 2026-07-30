// GoogleTest wrappers around the upstream jansson v2.15.0 API unit-test
// programs (test/suites/api/*.c), included verbatim through the glue TUs in
// glue/ (see upstream/README.md). Each upstream program defines run_tests()
// and gets its main() from api/util.h; the glue renames main and exposes a
// harvest_<name>() entry point returning 0 on success (upstream exits nonzero
// via fail()/exit(1) on any assertion failure). The harness runs each test in
// its own process, so an upstream failure or crash is attributed to exactly
// one test here.
//
// These are pure library-API tests: no randomness, no wall-clock dependence.
// The few that touch the filesystem do so only for negative paths (a
// nonexistent file, NULL arguments) or a self-created temp file they remove.
// json_pack/json_unpack/json_sprintf are exercised directly with literal
// arguments (the variadic surface that the retired single-signature dispatch
// could not reach).

#include <gtest/gtest.h>

extern "C" {
int harvest_test_array(void);
int harvest_test_copy(void);
int harvest_test_equal(void);
int harvest_test_fixed_size(void);
int harvest_test_loadb(void);
int harvest_test_number(void);
int harvest_test_object(void);
int harvest_test_pack(void);
int harvest_test_simple(void);
int harvest_test_sprintf(void);
int harvest_test_unpack(void);
int harvest_test_version(void);
int harvest_test_dump(void);
int harvest_test_load(void);
int harvest_test_dump_callback(void);
int harvest_test_load_callback(void);
int harvest_test_memory_funcs(void);
int harvest_test_memory_funcs_realloc(void);
int harvest_test_chaos(void);
int harvest_test_chaos_realloc(void);
}

namespace {

// Wave 1: pure-API tests (values, containers, parse/serialize, pack/unpack).
TEST(UpstreamApi, Array) { EXPECT_EQ(harvest_test_array(), 0); }
TEST(UpstreamApi, Copy) { EXPECT_EQ(harvest_test_copy(), 0); }
TEST(UpstreamApi, Equal) { EXPECT_EQ(harvest_test_equal(), 0); }
TEST(UpstreamApi, FixedSize) { EXPECT_EQ(harvest_test_fixed_size(), 0); }
TEST(UpstreamApi, Loadb) { EXPECT_EQ(harvest_test_loadb(), 0); }
TEST(UpstreamApi, Number) { EXPECT_EQ(harvest_test_number(), 0); }
TEST(UpstreamApi, Object) { EXPECT_EQ(harvest_test_object(), 0); }
TEST(UpstreamApi, Pack) { EXPECT_EQ(harvest_test_pack(), 0); }
TEST(UpstreamApi, Simple) { EXPECT_EQ(harvest_test_simple(), 0); }
TEST(UpstreamApi, Sprintf) { EXPECT_EQ(harvest_test_sprintf(), 0); }
TEST(UpstreamApi, Unpack) { EXPECT_EQ(harvest_test_unpack(), 0); }
TEST(UpstreamApi, Version) { EXPECT_EQ(harvest_test_version(), 0); }

// Wave 2: tests that touch the filesystem (negative paths / self-removed temp
// files) and tests that install custom allocator hooks (global state, isolated
// by the per-test process model).
TEST(UpstreamApi, Dump) { EXPECT_EQ(harvest_test_dump(), 0); }
TEST(UpstreamApi, Load) { EXPECT_EQ(harvest_test_load(), 0); }
TEST(UpstreamApi, DumpCallback) { EXPECT_EQ(harvest_test_dump_callback(), 0); }
TEST(UpstreamApi, LoadCallback) { EXPECT_EQ(harvest_test_load_callback(), 0); }
TEST(UpstreamApi, MemoryFuncs) { EXPECT_EQ(harvest_test_memory_funcs(), 0); }
TEST(UpstreamApi, MemoryFuncsRealloc) {
  EXPECT_EQ(harvest_test_memory_funcs_realloc(), 0);
}

// Wave 3: deterministic allocation-failure injection (fail the Nth malloc/
// realloc, verify graceful failure rather than a crash or leak). A faithful
// C-ABI translation is expected to degrade gracefully here too; a Rust port
// that aborts on allocation failure instead of returning an error is a
// legitimate difference this test will surface.
TEST(UpstreamApi, Chaos) { EXPECT_EQ(harvest_test_chaos(), 0); }
TEST(UpstreamApi, ChaosRealloc) { EXPECT_EQ(harvest_test_chaos_realloc(), 0); }

}  // namespace
