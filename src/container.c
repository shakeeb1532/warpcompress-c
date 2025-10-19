#include "warp.h"
#include "threadpool.h"
#include "codecs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#define ftruncate _chsize_s
#else
#include <unistd.h>
#endif

typedef struct {
  const char *in, *out;
  int algo, level, threads, verbose;
} c_args_t;

typedef struct {
  FILE *fin;
  size_t offset, len;
  int algo, level;
  unsigned idx;
  unsigned char *out;
  size_t out_len, out_cap;
  int ok;
} c_job_t;

/* --- helpers --- */
static size_t fsize(const char *path) {
  struct stat st; if (stat(path, &st) != 0) return 0; return (size_t)st.st_size;
}

/* --- compression worker --- */
static void do_compress(void *arg) {
  c_job_t *j = (c_job_t*)arg;
  unsigned char *in = (unsigned char*)malloc(j->len);
  fseek(j->fin, (long)j->offset, SEEK_SET);
  if (fread(in, 1, j->len, j->fin) != j->len) { free(in); j->ok=0; return; }

  size_t cap = zstd_max_compressed_size(j->len);
  j->out = (unsigned char*)malloc(cap);
  size_t got = zstd_compress(j->out, cap, in, j->len, j->level);
  free(in);

  if (got==0) { free(j->out); j->out=NULL; j->ok=0; return; }
  j->out_len = got; j->out_cap = cap; j->ok=1;
}

/* --- decompression worker --- */
typedef struct {
  FILE *fin;
  unsigned idx;
  warp_chunk_t ent;
  unsigned char *buf; /* filled with decompressed data */
  int ok;
} d_job_t;

static void do_decompress(void *arg) {
  d_job_t *j = (d_job_t*)arg;
  unsigned char *comp = (unsigned char*)malloc(j->ent.comp_len);
  fseek(j->fin, (long)j->ent.offset, SEEK_SET);
  if (fread(comp, 1, j->ent.comp_len, j->fin) != j->ent.comp_len) { free(comp); j->ok=0; return; }
  j->buf = (unsigned char*)malloc(j->ent.orig_len);
  size_t got = zstd_decompress(j->buf, j->ent.orig_len, comp, j->ent.comp_len);
  free(comp);
  if (got != j->ent.orig_len) { free(j->buf); j->buf=NULL; j->ok=0; return; }
  j->ok=1;
}

/* --- public API --- */

int warp_compress_file(const char *in_path, const char *out_path,
                       int algo, int level, int threads, int verbose)
{
  (void)algo; /* currently Zstd-only; keep for future */
  if (threads <= 0) threads = 1;
  if (level <= 0) level = 1;

  size_t total = fsize(in_path);
  if (!total) { fprintf(stderr, "input not found or empty\n"); return 1; }

  uint32_t chunk = warp_pick_chunk_size(total);
  uint32_t n = (uint32_t)((total + chunk - 1) / chunk);

  FILE *fin = fopen(in_path, "rb");
  if (!fin) { perror("fopen in"); return 1; }
  FILE *fout = fopen(out_path, "wb+");
  if (!fout) { perror("fopen out"); fclose(fin); return 1; }

  warp_header_t hdr = {
    .magic = WARP_MAGIC, .version = WARP_VER, .algo = WARP_ALGO_ZSTD,
    .flags=0, .chunk_size=chunk, .chunk_count=n,
    .orig_size=total, .comp_size=0
  };

  /* Reserve space for header + table */
  size_t table_sz = sizeof(warp_chunk_t) * n;
  if (fwrite(&hdr, sizeof(hdr), 1, fout) != 1) { perror("write hdr"); goto fail; }
  long table_pos = ftell(fout);
  warp_chunk_t *table = (warp_chunk_t*)calloc(n, sizeof(warp_chunk_t));
  if (!table) { fprintf(stderr, "oom\n"); goto fail; }
  if (fwrite(table, sizeof(warp_chunk_t), n, fout) != n) { perror("write table"); goto fail; }

  /* Compress chunks in a pool */
  tp_t *tp = tp_create(threads);
  c_job_t *jobs = (c_job_t*)calloc(n, sizeof(c_job_t));
  for (uint32_t i=0;i<n;i++) {
    size_t off = (size_t)i * chunk;
    size_t len = (off + chunk <= total) ? chunk : (total - off);
    jobs[i] = (c_job_t){ .fin=fin, .offset=off, .len=len, .algo=WARP_ALGO_ZSTD,
                         .level=level, .idx=i };
    tp_submit(tp, do_compress, &jobs[i]);
  }
  tp_barrier(tp);

  /* Write payloads and finalize table */
  for (uint32_t i=0;i<n;i++) {
    if (!jobs[i].ok) { fprintf(stderr, "compress chunk %u failed\n", i); goto fail; }
    long pos = ftell(fout);
    if (fwrite(jobs[i].out, 1, jobs[i].out_len, fout) != jobs[i].out_len) { perror("write payload"); goto fail; }
    table[i].orig_len = (uint32_t)jobs[i].len;
    table[i].comp_len = (uint32_t)jobs[i].out_len;
    table[i].offset   = (uint64_t)pos;
    hdr.comp_size += jobs[i].out_len;
    free(jobs[i].out);
  }

  /* Patch header + table */
  fseek(fout, sizeof(hdr), SEEK_SET);
  if (fwrite(table, sizeof(warp_chunk_t), n, fout) != n) { perror("rewrite table"); goto fail; }
  fseek(fout, 0, SEEK_SET);
  if (fwrite(&hdr, sizeof(hdr), 1, fout) != 1) { perror("rewrite hdr"); goto fail; }

  if (verbose) fprintf(stderr, "compressed %zu -> %llu bytes in %u chunks\n",
                       total, (unsigned long long)hdr.comp_size, n);

  free(table); free(jobs); tp_destroy(tp); fclose(fin); fclose(fout);
  return 0;

fail:
  free(table); free(jobs); tp_destroy(tp); fclose(fin); fclose(fout);
  return 2;
}

int warp_decompress_file(const char *in_path, const char *out_path,
                         int threads, int verbose)
{
  if (threads <= 0) threads = 1;
  FILE *fin = fopen(in_path, "rb");
  if (!fin) { perror("fopen in"); return 1; }

  warp_header_t hdr;
  if (fread(&hdr, sizeof(hdr), 1, fin) != 1) { fprintf(stderr,"bad header\n"); fclose(fin); return 2; }
  if (hdr.magic != WARP_MAGIC || hdr.version != WARP_VER) { fprintf(stderr,"bad magic/version\n"); fclose(fin); return 2; }
  if (hdr.algo != WARP_ALGO_ZSTD) { fprintf(stderr,"unsupported algo (currently zstd only)\n"); fclose(fin); return 2; }

  warp_chunk_t *table = (warp_chunk_t*)malloc(sizeof(warp_chunk_t) * hdr.chunk_count);
  if (!table) { fclose(fin); return 3; }
  if (fread(table, sizeof(warp_chunk_t), hdr.chunk_count, fin) != hdr.chunk_count) { fprintf(stderr,"bad table\n"); free(table); fclose(fin); return 2; }

  FILE *fout = fopen(out_path, "wb+");
  if (!fout) { perror("fopen out"); free(table); fclose(fin); return 1; }
#ifdef _WIN32
  _chsize_s(_fileno(fout), hdr.orig_size);
#else
  ftruncate(fileno(fout), (off_t)hdr.orig_size);
#endif

  tp_t *tp = tp_create(threads);
  d_job_t *jobs = (d_job_t*)calloc(hdr.chunk_count, sizeof(d_job_t));
  for (uint32_t i=0;i<hdr.chunk_count;i++) {
    jobs[i] = (d_job_t){ .fin=fin, .idx=i, .ent=table[i] };
    tp_submit(tp, do_decompress, &jobs[i]);
  }
  tp_barrier(tp);

  /* ordered write */
  for (uint32_t i=0;i<hdr.chunk_count;i++) {
    if (!jobs[i].ok) { fprintf(stderr, "decompress chunk %u failed\n", i); free(table); free(jobs); tp_destroy(tp); fclose(fin); fclose(fout); return 2; }
    fseek(fout, (long)((size_t)i * (size_t)hdr.chunk_size), SEEK_SET);
    fwrite(jobs[i].buf, 1, table[i].orig_len, fout);
    free(jobs[i].buf);
  }

  if (verbose) fprintf(stderr,"decompressed to %s (%llu bytes)\n", out_path, (unsigned long long)hdr.orig_size);

  free(table); free(jobs); tp_destroy(tp);
  fclose(fin); fclose(fout);
  return 0;
}
