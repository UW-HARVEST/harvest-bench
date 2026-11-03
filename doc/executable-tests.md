# Writing executable tests

Executable tests run the program's executable and check its output, exit
status, and the files that it writes. They use the helper
`runner/support/harvest_bin.h`. For how a program declares its executable,
see [programs.md](programs.md).

## Setup

Copy `harvest_bin.h` into the suite unchanged. The suite turns the two CMake
variables of the executable into compile definitions:

```cmake
if(NOT DEFINED TEST_BIN_PATH)
  message(FATAL_ERROR "Pass -DTEST_BIN_PATH=/abs/path/to/toy (executable under test)")
endif()
if(NOT DEFINED TEST_BIN_NAME)
  message(FATAL_ERROR "Pass -DTEST_BIN_NAME=<program.bin.name in manifest.json> with TEST_BIN_PATH")
endif()
target_compile_definitions(harvest_gtest PRIVATE
    HARVEST_BIN_PATH="${TEST_BIN_PATH}"
    HARVEST_BIN_NAME="${TEST_BIN_NAME}")
```

Put the directory of the copy on the include path. A test then runs the
executable and checks the result:

```cpp
#include <harvest_bin.h>

TEST(ToyBin, UpperFromStdin) {
  harvest_bin::Request q;
  q.args = {"upper"};          // argv[1..]; argv[0] is HARVEST_BIN_NAME
  q.stdin_data = "hello\n";    // omit for /dev/null
  auto r = harvest_bin::run(q);
  ASSERT_TRUE(r.exited(0)) << r.describe();
  EXPECT_EQ(r.out, "HELLO\n");
  EXPECT_EQ(r.err, "");
}
```

`harvest_bin::read_output_file` reads a file that the executable wrote.

## Run environment

Each run gets a new working directory and `HOME` under the invocation's
scratch directory (`HARVEST_TEST_TMPDIR`). Each invocation gets a new scratch
directory, and the runner removes it after the invocation, also when a test
made directories in it read-only. The environment is small and fixed
(`LC_ALL=C`, `TZ=UTC`). The helper reads stdout and stderr while it writes
stdin, so large input and output cannot block.

A run has a timeout: by default the invocation's granted timeout
(`HARVEST_TEST_SOFT_TIMEOUT_SECS`, that is `max(budget × default_factor,
min_seconds)`) less one second, so a hung executable fails as an assertion
before the runner kills the process group. `Request::timeout_secs` changes
it.

## Process cleanup

Put executable tests in `case` mode. The runner then kills the process group
of the test after each test, which also stops every child process that the
executable left.

Known limitation: a child process that leaves the process group (`setsid`,
or a daemon) is not stopped and stays after the run.

## Examples

`runner/fixtures/toy` is a small program with a library and an executable,
with a suite that tests both. The runner's regression test uses it:

```bash
cargo test --release --test fixtures -- --ignored --nocapture
```
