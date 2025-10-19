#include "warp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void usage(void) {
  fprintf(stderr,
    "warpC - high-speed (de)compression in C (Zstd)\n"
    "Usage:\n"
    "  warpc compress <in> <out.warp> [--level N] [--threads N] [--verbose]\n"
    "  warpc decompress <in.warp> <out> [--threads N] [--verbose]\n");
}

int main(int argc, char **argv) {
  if (argc < 2) { usage(); return 1; }

  int threads = 0, level = 1, verbose = 0;
  const char *cmd = argv[1];

  for (int i=2;i<argc;i++) {
    if (!strcmp(argv[i], "--threads") && i+1<argc) threads = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--level") && i+1<argc) level = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--verbose")) verbose = 1;
  }

  if (!strcmp(cmd, "compress")) {
    if (argc < 4) { usage(); return 1; }
    const char *in = argv[2], *out = argv[3];
    return warp_compress_file(in, out, WARP_ALGO_ZSTD, level, threads, verbose);
  } else if (!strcmp(cmd, "decompress")) {
    if (argc < 4) { usage(); return 1; }
    const char *in = argv[2], *out = argv[3];
    return warp_decompress_file(in, out, threads, verbose);
  } else {
    usage();
    return 1;
  }
}
