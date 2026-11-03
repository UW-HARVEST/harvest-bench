/* The toy executable. Besides its ordinary commands it has misbehaving ones
 * that the abuse probes of the fixture suite use. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "toy.h"

static int usage(const char *argv0) {
  fprintf(stderr,
          "usage: %s upper|cat|gen N|fail MSG|write FILE TEXT|home|bg|hang\n",
          argv0);
  return 2;
}

int main(int argc, char **argv) {
  if (argc < 2) return usage(argv[0]);
  const char *cmd = argv[1];
  if (!strcmp(cmd, "upper") || !strcmp(cmd, "cat")) {
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, stdin)) > 0) {
      if (!strcmp(cmd, "upper")) toy_upper(buf, n);
      fwrite(buf, 1, n, stdout);
    }
    return 0;
  }
  if (!strcmp(cmd, "gen") && argc >= 3) {
    long n = atol(argv[2]);
    for (long i = 0; i < n; i++) putchar('a' + (int)(i % 26));
    return 0;
  }
  if (!strcmp(cmd, "fail") && argc >= 3) {
    fprintf(stderr, "%s: error: %s\n", argv[0], argv[2]);
    return 3;
  }
  if (!strcmp(cmd, "write") && argc >= 4) {
    FILE *f = fopen(argv[2], "w");
    if (!f) {
      perror(argv[2]);
      return 1;
    }
    fputs(argv[3], f);
    fclose(f);
    return 0;
  }
  if (!strcmp(cmd, "home")) {
    const char *home = getenv("HOME");
    char cwd[4096];
    printf("HOME=%s\nCWD=%s\n", home ? home : "", getcwd(cwd, sizeof cwd) ? cwd : "");
    return 0;
  }
  /* bg: leave a sleeping child that keeps the output pipes, exit 0.
   * hang: the same child, and the parent never exits. Neither burns CPU, so
   * only process-group kills end them. Extra arguments are markers. */
  if (!strcmp(cmd, "bg") || !strcmp(cmd, "hang")) {
    if (fork() == 0)
      for (;;) pause();
    printf("%s\n", !strcmp(cmd, "bg") ? "started" : "parent waiting");
    fflush(stdout);
    if (!strcmp(cmd, "bg")) return 0;
    for (;;) pause();
  }
  return usage(argv[0]);
}
