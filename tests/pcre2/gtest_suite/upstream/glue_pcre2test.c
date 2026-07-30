/* Glue TU for the upstream pcre2test.c test-driver program.
 *
 * pcre2test.c is included verbatim with its main() renamed away, which
 * gives this TU access to it under a fixed, callable entry point. Each
 * invocation mirrors one run of the real pcre2test binary as RunTest
 * would perform it (input file in argv, output file in argv), so a test
 * case here maps directly onto one of RunTest's numbered tests.
 *
 * pcre2test.c is designed to run once per process (like the real
 * upstream binary, exec'd fresh by RunTest for every test); its
 * file-scope statics are not fully reset between repeated in-process
 * calls. That's fine under the benchmark harness, which runs every
 * GoogleTest case in its own process; running the full ./harvest_gtest
 * binary locally (all cases sequentially in one process) is only
 * expected to work because none of the ported tests below rely on
 * state left over from a previous case.
 */
int harvest_pcre2test(int argc, char **argv);

#define main pcre2_upstream_pcre2test_unused_main
#include "pcre2test.c"
#undef main

int harvest_pcre2test(int argc, char **argv)
{
    /* pcre2test.c's file-scope `infile`/`outfile` statics get fclose()d at
     * EXIT: but are never reset to NULL afterwards. A fresh process starts
     * with them zero-initialized, but a second in-process call would
     * otherwise see the previous call's now-dangling pointers - and some
     * argument paths (e.g. -error) jump straight to EXIT: without ever
     * assigning them first, double-fclose()ing the stale pointer. Reset
     * them here so each call behaves like a fresh process, matching how
     * RunTest actually invokes pcre2test (a new process every time). */
    infile = NULL;
    outfile = NULL;
    return pcre2_upstream_pcre2test_unused_main(argc, argv);
}
