#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>   /* sysconf, unlink */

#include "warpc/container.h"
#include "warpc/codecs.h"
#include "warpc/threadpool.h"
#include "warpc/bufpool.h"
#include "warpc/util.h"

typedef struct {
  const codec_vtable* vt;
  int    codec_id;
  int    level;
  size_t chunk_kib;
  int    threads;
  int    verify;   /* round-trip check */
  int    verbose;
} warpc_opts;

static void usage(const char* argv0) {
  fprintf(stderr,
    "Usage:\n"
    "  %s compress  [--codec zstd|lz4|throughput] [--level N] [--chunk-kib N] [--threads N] [--verify] [--verbose] <in> <out.warp>\n"
    "  %s decompress [--threads N] [--verbose] <in.warp> <out>\n"
    "\n"
    "Defaults: --codec zstd, --level zstd:3 lz4:0, --chunk-kib 16384, --threads=CPU\n"
    "Preset  : --codec throughput ⇒ lz4 @ large chunks\n",
    argv0, argv0);
}

static int do_decompress(const char* in, const char* out, const warpc_opts* o); /* fwd */

static int autodetect_threads(void) {
  long n = sysconf(_SC_NPROCESSORS_ONLN);
  if (n <= 0) n = 1;
  if (n > 64) n = 64;
  return (int)n;
}

static void apply_preset(const char* name, warpc_opts* o) {
  if (strcmp(name, "throughput") == 0) {
    const codec_vtable* vt = warpc_get_codec_by_name("lz4");
    if (vt) { o->vt = vt; o->codec_id = warpc_codec_id_from_name("lz4"); o->level = WARPC_DEFAULT_LEVEL_LZ4; }
    if (o->chunk_kib < 32768) o->chunk_kib = 32768; /* ≥ 32 MiB */
  }
}

static int parse_args(int argc, char** argv, int* is_compress, warpc_opts* o, const char** in, const char** out) {
  if (argc < 2) { usage(argv[0]); return -1; }
  *is_compress = (strcmp(argv[1], "compress") == 0);
  int is_decompress = (strcmp(argv[1], "decompress") == 0);
  if (!*is_compress && !is_decompress) { usage(argv[0]); return -1; }

  o->vt = warpc_get_codec_by_name("zstd");
  o->codec_id = warpc_codec_id_from_name("zstd");
  o->level = WARPC_DEFAULT_LEVEL_ZSTD;
  o->chunk_kib = WARPC_DEFAULT_CHUNK_KIB;
  o->threads = autodetect_threads();
  o->verify = 0;
  o->verbose = 0;

  int i = 2;
  while (i < argc) {
    if (strcmp(argv[i], "--codec") == 0 && i+1 < argc) {
      const char* c = argv[++i];
      if (strcmp(c, "throughput") == 0) { apply_preset("throughput", o); }
      else {
        const codec_vtable* vt = warpc_get_codec_by_name(c);
        int id = warpc_codec_id_from_name(c);
        if (!vt || id == 0) { fprintf(stderr, "Unknown/disabled codec '%s'\n", c); return -1; }
        o->vt = vt; o->codec_id = id;
      }
    } else if (strcmp(argv[i], "--level") == 0 && i+1 < argc) {
      o->level = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--chunk-kib") == 0 && i+1 < argc) {
      o->chunk_kib = (size_t)atol(argv[++i]);
    } else if (strcmp(argv[i], "--threads") == 0 && i+1 < argc) {
      o->threads = atoi(argv[++i]);
      if (o->threads <= 0) o->threads = autodetect_threads();
    } else if (strcmp(argv[i], "--verify") == 0) {
      o->verify = 1;
    } else if (strcmp(argv[i], "--verbose") == 0) {
      o->verbose = 1;
    } else {
      break;
    }
    ++i;
  }
  if (argc - i != 2) { usage(argv[0]); return -1; }
  *in = argv[i]; *out = argv[i+1];
  return *is_compress ? 1 : 2;
}

/* ---------------- Compression / Decompression ---------------- */

static int do_compress(const char* in, const char* out, const warpc_opts* o) {
  if (!o->vt) { fprintf(stderr, "No codec available.\n"); return 1; }

  uint64_t fsize = 0;
  if (file_stat_size(in, &fsize) != 0) { perror("stat"); return 1; }
  int fd_in = file_open_rd(in); if (fd_in < 0) { perror("open input"); return 1; }
  int fd_out = file_open_trunc(out); if (fd_out < 0) { perror("open output"); close(fd_in); return 1; }

  warpc_header hdr = {0};
  hdr.magic = WARPC_MAGIC;
  hdr.version = WARPC_VERSION;
  hdr.codec = (uint16_t)o->codec_id;
  hdr.chunk_size_k = (uint32_t)o->chunk_kib;
  hdr.orig_size = fsize;

  if (write_all(fd_out, &hdr, sizeof(hdr)) != 0) { perror("write header"); close(fd_in); close(fd_out); return 1; }

  size_t chunk = o->chunk_kib * 1024;
  struct bufpool* inpool  = pool_create((size_t)o->threads + 2, chunk);
  struct bufpool* outpool = pool_create((size_t)o->threads + 2, chunk * 2); /* dst bound ≈ bigger */
  if (!inpool || !outpool) { fprintf(stderr, "OOM: buffers\n"); close(fd_in); close(fd_out); return 1; }

  uint64_t remaining = fsize;
  off_t in_off = 0;

  while (remaining > 0) {
    size_t this_in = (remaining > chunk) ? chunk : (size_t)remaining;
    void* ibuf = pool_acquire(inpool);
    if (!ibuf) { ibuf = malloc(chunk); if (!ibuf) { fprintf(stderr, "OOM\n"); break; } }
    if (pread_all(fd_in, ibuf, this_in, in_off) != 0) { perror("pread"); free(ibuf); break; }
    in_off += (off_t)this_in;
    remaining -= (uint64_t)this_in;

    size_t bound = 0;
    if (o->vt->compress_bound(this_in, &bound) != 0 || bound == 0) bound = this_in + (this_in / 16) + 64;
    void* obuf = pool_acquire(outpool);
    if (!obuf || bound > outpool->bufsz) { if (obuf) pool_release(outpool, obuf); obuf = malloc(bound); if (!obuf) { fprintf(stderr, "OOM\n"); free(ibuf); break; } }

    size_t got = o->vt->compress(ibuf, this_in, obuf, bound, o->level);
    if (got == 0) {
      fprintf(stderr, "Compression failed (codec=%s, level=%d). Check that the library is installed and enabled.\n", o->vt->name, o->level);
      free(ibuf); free(obuf); close(fd_in); close(fd_out); pool_destroy(inpool); pool_destroy(outpool); return 1;
    }

    uint64_t u = (uint64_t)this_in, c = (uint64_t)got;
    if (write_all(fd_out, &u, sizeof(u)) != 0 || write_all(fd_out, &c, sizeof(c)) != 0 ||
        write_all(fd_out, obuf, got) != 0) {
      perror("write chunk");
      free(ibuf); free(obuf); break;
    }

    pool_release(inpool, ibuf);
    pool_release(outpool, obuf);

    if (o->verbose) fprintf(stderr, "compressed %zu -> %zu bytes (%s)\n", this_in, got, o->vt->name);
  }

  pool_destroy(inpool);
  pool_destroy(outpool);
  close(fd_in); close(fd_out);

  if (o->verify) {
    char* tmp = (char*)malloc(strlen(out) + 8);
    if (!tmp) return 0;
    sprintf(tmp, "%s.out", out);
    warpc_opts v = *o; v.verify = 0; v.verbose = 0;
    if (do_decompress(out, tmp, &v) != 0) {
      fprintf(stderr, "verify: decompress failed\n"); free(tmp); return 1;
    }
    uint64_t a = fnv1a64_file(in, 1<<20);
    uint64_t b = fnv1a64_file(tmp, 1<<20);
    if (a != b) {
      fprintf(stderr, "verify: hash mismatch! (0x%016llx != 0x%016llx)\n",
              (unsigned long long)a, (unsigned long long)b);
      free(tmp); return 1;
    }
    if (o->verbose) fprintf(stderr, "verify: OK (FNV-1a 64)\n");
    unlink(tmp);
    free(tmp);
  }

  return 0;
}

static int do_decompress(const char* in, const char* out, const warpc_opts* o) {
  (void)o;
  int fd_in = file_open_rd(in); if (fd_in < 0) { perror("open input"); return 1; }
  int fd_out = file_open_trunc(out); if (fd_out < 0) { perror("open output"); close(fd_in); return 1; }

  warpc_header hdr;
  if (read_all(fd_in, &hdr, sizeof(hdr)) != 0) { fprintf(stderr, "read header failed\n"); close(fd_in); close(fd_out); return 1; }
  if (hdr.magic != WARPC_MAGIC || hdr.version != WARPC_VERSION) { fprintf(stderr, "bad container\n"); close(fd_in); close(fd_out); return 1; }

  const codec_vtable* vt = warpc_get_codec_by_id((int)hdr.codec);
  if (!vt) { fprintf(stderr, "codec %u not available\n", (unsigned)hdr.codec); close(fd_in); close(fd_out); return 1; }

  size_t chunk = (size_t)hdr.chunk_size_k * 1024;
  void* ibuf = malloc(chunk * 2);
  void* obuf = malloc(chunk);
  if (!ibuf || !obuf) { fprintf(stderr, "OOM\n"); close(fd_in); close(fd_out); free(ibuf); free(obuf); return 1; }

  uint64_t done = 0;
  while (done < hdr.orig_size) {
    uint64_t u = 0, c = 0;
    if (read_all(fd_in, &u, sizeof(u)) != 0) break;
    if (read_all(fd_in, &c, sizeof(c)) != 0) break;
    if (c > (uint64_t)(chunk*2)) { ibuf = realloc(ibuf, (size_t)c); if (!ibuf) { fprintf(stderr, "OOM\n"); break; } }
    if (u > (uint64_t)chunk)     { obuf = realloc(obuf, (size_t)u); if (!obuf) { fprintf(stderr, "OOM\n"); break; } }
    if (read_all(fd_in, ibuf, (size_t)c) != 0) { fprintf(stderr, "read chunk payload failed\n"); break; }

    size_t got = vt->decompress(ibuf, (size_t)c, obuf, (size_t)u);
    if (got != (size_t)u) { fprintf(stderr, "decompress failed (%s)\n", vt->name); break; }
    if (write_all(fd_out, obuf, (size_t)u) != 0) { perror("write"); break; }

    done += u;
  }

  free(ibuf); free(obuf);
  close(fd_in); close(fd_out);
  return (done == hdr.orig_size) ? 0 : 1;
}

/* ---------------- main ---------------- */

int main(int argc, char** argv) {
  int is_compress = 0;
  warpc_opts opt;
  const char* in = NULL;
  const char* out = NULL;

  int mode = parse_args(argc, argv, &is_compress, &opt, &in, &out);
  if (mode < 0) return 2;

  if (mode == 1) { /* compress */
    return do_compress(in, out, &opt);
  } else { /* decompress */
    return do_decompress(in, out, &opt);
  }
}
