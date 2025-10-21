#include "container.h"
#include "warp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char* prog) {
  fprintf(stderr,
    "Usage:\n"
    "  %s compress  <in> <out.warp> [--threads N] [--level L] [--algo zstd|lz4|snappy|copy|auto]\n"
    "               [--chunk BYTES] [--auto balanced|throughput|ratio] [--auto-lock N]\n"
    "               [--index 0|1] [--checksum none|xxh64] [--verbose]\n"
    "  %s decompress <in.warp> <out> [--threads N] [--verify] [--verbose]\n", prog, prog);
}

int main(int argc, char** argv) {
  if (argc < 2) { usage(argv[0]); return 1; }
  int compress = !strcmp(argv[1],"compress");
  int decompress = !strcmp(argv[1],"decompress");
  if (!compress && !decompress) { usage(argv[0]); return 1; }
  if (argc < 4) { usage(argv[0]); return 1; }

  const char* in  = argv[2];
  const char* out = argv[3];

  warp_opts_t opt = warp_opts_default();

  for (int i=4;i<argc;i++) {
    if (!strcmp(argv[i],"--threads") && i+1<argc) opt.threads = atoi(argv[++i]);
    else if (!strcmp(argv[i],"--level") && i+1<argc) opt.level = atoi(argv[++i]);
    else if (!strcmp(argv[i],"--chunk") && i+1<argc) opt.chunk_bytes = (uint32_t)strtoull(argv[++i],NULL,10);
    else if (!strcmp(argv[i],"--index") && i+1<argc) opt.do_index = atoi(argv[++i]);
    else if (!strcmp(argv[i],"--auto-lock") && i+1<argc) opt.auto_lock = atoi(argv[++i]);
    else if (!strcmp(argv[i],"--verbose")) opt.verbose = 1;
    else if (!strcmp(argv[i],"--verify")) opt.verify = 1;
    else if (!strcmp(argv[i],"--checksum") && i+1<argc) {
      const char* v=argv[++i];
      opt.chk_kind = (!strcmp(v,"xxh64"))? WARP_CHK_XXH64 : WARP_CHK_NONE;
    } else if (!strcmp(argv[i],"--algo") && i+1<argc) {
      const char* a=argv[++i];
      if (!strcmp(a,"auto")) opt.algo=0;
      else if (!strcmp(a,"zstd")) opt.algo=WARP_ALGO_ZSTD;
      else if (!strcmp(a,"lz4")) opt.algo=WARP_ALGO_LZ4;
      else if (!strcmp(a,"snappy")) opt.algo=WARP_ALGO_SNAPPY;
      else if (!strcmp(a,"copy")) opt.algo=WARP_ALGO_COPY;
    } else if (!strcmp(argv[i],"--auto") && i+1<argc) {
      const char* m=argv[++i];
      if (!strcmp(m,"throughput")) opt.auto_mode=WARP_AUTO_THROUGHPUT;
      else if (!strcmp(m,"ratio")) opt.auto_mode=WARP_AUTO_RATIO;
      else opt.auto_mode=WARP_AUTO_BALANCED;
    } else {
      fprintf(stderr,"Unknown/invalid arg: %s\n", argv[i]); usage(argv[0]); return 1;
    }
  }

  int rc = compress ? warp_compress_file(in, out, &opt)
                    : warp_decompress_file(in, out, &opt);
  return rc;
}
