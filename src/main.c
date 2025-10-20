#include "warp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void usage(void) {
  fprintf(stderr,
    "warpc - high-speed (de)compression in C (Zstd/LZ4/Snappy/COPY/ZERO)\n"
    "Usage:\n"
    "  warpc compress <in> <out.warp> [options]\n"
    "  warpc decompress <in.warp> <out> [options]\n"
    "\nOptions:\n"
    "  --algo {auto,zstd,lz4,snappy,copy}   (default auto)\n"
    "  --auto {throughput,balanced,ratio}   (AUTO scoring; default balanced)\n"
    "  --auto-lock N                        (warm-up chunks to test, default 4)\n"
    "  --level N                            (zstd level; default 1)\n"
    "  --threads N                          (default 1)\n"
    "  --chunk NMiB                         (override policy)\n"
    "  --index / --no-index                 (default: index on)\n"
    "  --checksum {none,xxh64}              (default none)\n"
    "  --verify                             (verify checksum on decompress when present)\n"
    "  --verbose\n");
}

static int parse_algo(const char *s) {
  if (!s || !*s) return 0;
  if (!strcmp(s,"auto"))   return 0;
  if (!strcmp(s,"zstd"))   return WARP_ALGO_ZSTD;
  if (!strcmp(s,"lz4"))    return WARP_ALGO_LZ4;
  if (!strcmp(s,"snappy")) return WARP_ALGO_SNAPPY;
  if (!strcmp(s,"copy"))   return WARP_ALGO_COPY;
  return -1;
}
static int parse_auto(const char *s) {
  if (!s || !*s) return WARP_AUTO_BALANCED;
  if (!strcmp(s,"throughput")) return WARP_AUTO_THROUGHPUT;
  if (!strcmp(s,"balanced"))   return WARP_AUTO_BALANCED;
  if (!strcmp(s,"ratio"))      return WARP_AUTO_RATIO;
  return -1;
}
static int parse_chk(const char *s) {
  if (!s || !*s || !strcmp(s,"none")) return WARP_CHK_NONE;
  if (!strcmp(s,"xxh64")) return WARP_CHK_XXH64;
  return -1;
}
static int parse_mib(const char *s) {
  int n = atoi(s);
  if (strstr(s, "MiB") || strstr(s, "mib") || strstr(s, "M") || strstr(s, "m")) return n<<20;
  return n;
}

int main(int argc, char **argv) {
  if (argc < 2) { usage(); return 1; }
  const char *cmd = argv[1];

  warp_opts_t opt = {0};
  opt.auto_mode = WARP_AUTO_BALANCED;
  opt.auto_lock = 4;
  opt.level     = 1;
  opt.threads   = 1;
  opt.do_index  = 1;
  opt.chk_kind  = WARP_CHK_NONE;

  for (int i=2;i<argc;i++) {
    if (!strcmp(argv[i],"--algo") && i+1<argc)      { int a=parse_algo(argv[++i]); if (a<0){usage();return 1;} opt.algo=a; }
    else if (!strcmp(argv[i],"--auto") && i+1<argc) { int a=parse_auto(argv[++i]); if (a<0){usage();return 1;} opt.auto_mode=a; }
    else if (!strcmp(argv[i],"--auto-lock") && i+1<argc) { opt.auto_lock=atoi(argv[++i]); }
    else if (!strcmp(argv[i],"--level") && i+1<argc)     { opt.level=atoi(argv[++i]); }
    else if (!strcmp(argv[i],"--threads") && i+1<argc)   { opt.threads=atoi(argv[++i]); }
    else if (!strcmp(argv[i],"--chunk") && i+1<argc)     { opt.chunk_bytes=parse_mib(argv[++i]); }
    else if (!strcmp(argv[i],"--index"))                 { opt.do_index=1; }
    else if (!strcmp(argv[i],"--no-index"))              { opt.do_index=0; }
    else if (!strcmp(argv[i],"--checksum") && i+1<argc)  { int c=parse_chk(argv[++i]); if (c<0){usage();return 1;} opt.chk_kind=c; }
    else if (!strcmp(argv[i],"--verify"))                { opt.verify=1; }
    else if (!strcmp(argv[i],"--verbose"))               { opt.verbose=1; }
  }

  if (!strcmp(cmd, "compress")) {
    if (argc < 4) { usage(); return 1; }
    return warp_compress_file(argv[2], argv[3], &opt);
  } else if (!strcmp(cmd, "decompress")) {
    if (argc < 4) { usage(); return 1; }
    return warp_decompress_file(argv[2], argv[3], &opt);
  } else {
    usage();
    return 1;
  }
}

