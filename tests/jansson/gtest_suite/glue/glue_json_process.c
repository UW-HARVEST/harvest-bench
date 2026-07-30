/* Glue TU: wraps the upstream jansson test/bin/json_process.c so the
 * data-driven suites (valid/invalid/invalid-unicode/encoding-flags) can be
 * driven from GoogleTest. json_process reads a case directory's `input`
 * (and optional `env` config), parses + dumps through the public API, and
 * compares against the case's expected `output` (valid) or `error` (invalid)
 * file exactly as upstream's own `run` script does.
 *
 * The upstream file is included verbatim (main renamed). Its file-scope
 * `conf` struct accumulates state across calls, so the entry point zeroes it
 * before each invocation to keep tests independent within one process. */
#define main json_process_unused_main
#include "json_process.c"
#undef main

#include <string.h>

int harvest_json_process(const char *test_dir, int strip)
{
    memset(&conf, 0, sizeof(conf));
    conf.strip = strip;
    return use_conf((char *)test_dir);
}
