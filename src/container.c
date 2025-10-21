#include "warp.h"
#include "container.h"   /* if you keep a separate container API, else remove */
#include "threadpool.h"
#include "codecs.h"
#include "util.h"

#ifdef HAVE_XXHASH
#include <xxhash.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

/* ===== Helpers ===== */
static size_t fsize(const char *path) {
  struct stat st; if (stat(path, &st) != 0) return 0; return (size_t)st.st_size;
}

static int is_all_zero(const unsigned char *p, size_t n) {
  const unsigned long long *q = (const unsigned long long*)p;
  while (n >= sizeof(*q)) { if (*q++) return 0; n -= sizeof(*q); }
  p = (const unsigned char*)q;
  while (n--) if (*p++) return 0;
  return 1;
}

static double now_secs(void) {
#if defined(CLOCK_MONOTONIC)
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
#else
  /* very old fallback */
  return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}

/* ===== Compression job ===== */
typedef struct {
  int fd;
  size_t offset, len;
  int prefer_algo;   /* 0=auto, else fixed */
  int level;
  int idx;

  /* pools */
  bufpool_t *in_pool;
  bufpool_t *out_pool;
  size_t     out_cap;

  /* Results */
  unsigned char *in_buf;
  unsigned char *comp;
  size_t comp_len;
  int out_algo;
  double secs;
  int ok;
} c_job_t;

static size_t try_algo_zstd(const unsigned char *in, size_t in_len, int level, int nb_workers, unsigned char *out, size_t out_cap) {
  /* Use simple single-shot if you haven’t wired ctx helpers: */
  return zstd_compress(out, out_cap, in, in_len, level);
}

static size_t try_algo_lz4(const unsigned char *in, size_t in_len, unsigned char *out, size_t out_cap) {
#ifdef HAVE_LZ4
  return lz4_compress_fast(out, out_cap, in, in_len);
#else
  (void)in; (void)in_len; (void)out; (void)out_cap; return 0;
#endif
}

static size_t try_algo_snappy(const unsigned char *in, size_t in_len, unsigned char *out, size_t out_cap) {
#ifdef HAVE_SNAPPY
  return snappy_compress(out, out_cap, in, in_len);
#else
  (void)in; (void)in_len; (void)out; (void)out_cap; return 0;
#endif
}

static void do_compress(void *arg) {
  c_job_t *j = (c_job_t*)arg;

  j->in_buf = (unsigned char*)pool_acquire(j->in_pool);
  unsigned char *out  = (unsigned char*)pool_acquire(j->out_pool);
  if (!j->in_buf || !out) { j->ok=0; return; }

  ssize_t r = wc_pread(j->fd, j->in_buf, j->len, (uint64_t)j->offset);
  if (r != (ssize_t)j->len) { j->ok=0; return; }

  /* ZERO fast path */
  if (is_all_zero(j->in_buf, j->len)) {
    j->comp = NULL; j->comp_len = 0; j->out_algo = WARP_ALGO_ZERO; j->secs = 0.0; j->ok=1;
    return;
  }

  double best_score = -1e300;
  size_t best_len   = 0;
  int    best_algo  = WARP_ALGO_COPY;

  /* candidate selection */
  int candidates[4]; int ccount = 0;
  if (j->prefer_algo == 0) {
    candidates[ccount++] = WARP_ALGO_ZSTD;
#ifdef HAVE_LZ4
    candidates[ccount++] = WARP_ALGO_LZ4;
#endif
#ifdef HAVE_SNAPPY
    candidates[ccount++] = WARP_ALGO_SNAPPY;
#endif
  } else {
    candidates[ccount++] = j->prefer_algo;
  }

  /* compress and score (throughput only here; warm-up phase will balance) */
  for (int k=0;k<ccount;k++) {
    int algo = candidates[k];
    size_t got = 0;
    double t0 = now_secs();
    if (algo == WARP_ALGO_ZSTD)        got = try_algo_zstd  (j->in_buf, j->len, j->level, /*nb_workers*/0, out, j->out_cap);
    else if (algo == WARP_ALGO_LZ4)    got = try_algo_lz4   (j->in_buf, j->len, out, j->out_cap);
    else if (algo == WARP_ALGO_SNAPPY) got = try_algo_snappy(j->in_buf, j->len, out, j->out_cap);
    else if (algo == WARP_ALGO_COPY) { memcpy(out, j->in_buf, j->len); got = j->len; }
    double secs = now_secs() - t0;
    if (!got) continue;

    double mbps = secs > 0 ? (j->len / (1024.0*1024.0)) / secs : 0.0;
    if (mbps > best_score) { best_score = mbps; best_len = got; best_algo = algo; j->secs = secs; }
  }

  if (best_len == 0 || best_len >= j->len - (j->len>>6)) {
    /* COPY fallback */
    memcpy(out, j->in_buf, j->len);
    best_algo = WARP_ALGO_COPY;
    best_len  = j->len;
    j->secs   = 0.0;
  }

  j->comp     = out;
  j->comp_len = best_len;
  j->out_algo = best_algo;
  j->ok       = 1;
}

/* ===== Decompression job ===== */
typedef struct {
  int fd;
  int idx;
  warp_chunk_t ent;
  bufpool_t  *out_pool;
  unsigned char *buf;
  int ok;
} d_job_t;

static void do_decompress(void *arg) {
  d_job_t *j = (d_job_t*)arg;
  j->buf = (unsigned char*)pool_acquire(j->out_pool);
  if (!j->buf) { j->ok=0; return; }

  if (j->ent.algo == WARP_ALGO_ZERO) {
    memset(j->buf, 0, j->ent.orig_len);
    j->ok = 1; return;
  }
  unsigned char *comp = (unsigned char*)malloc(j->ent.comp_len);
  if (!comp) { j->ok=0; return; }
  ssize_t r = wc_pread(j->fd, comp, j->ent.comp_len, j->ent.offset);
  if (r != (ssize_t)j->ent.comp_len) { free(comp); j->ok=0; return; }

  size_t got = 0;
  switch (j->ent.algo) {
    case WARP_ALGO_COPY:   memcpy(j->buf, comp, j->ent.orig_len); got = j->ent.orig_len; break;
    case WARP_ALGO_ZSTD:   got = zstd_decompress  (j->buf, j->ent.orig_len, comp, j->ent.comp_len); break;
    case WARP_ALGO_LZ4:    got = lz4_decompress   (j->buf, j->ent.orig_len, comp, j->ent.comp_len); break;
    case WARP_ALGO_SNAPPY: got = snappy_decompress(j->buf, j->ent.orig_len, comp, j->ent.comp_len); break;
    default: got = 0; break;
  }
  free(comp);
  if (got != j->ent.orig_len) { j->ok=0; return; }
  j->ok=1;
}

/* ===== AUTO scoring ===== */
static int score_pick_algo(int mode, size_t in_len, size_t z_len, double z_mbps,
                           size_t l_len, double l_mbps, size_t s_len, double s_mbps) {
  double best = -1e300; int best_algo = WARP_ALGO_ZSTD;
  struct { int algo; size_t clen; double mbps; } cands[3] = {
    { WARP_ALGO_ZSTD,   z_len, z_mbps },
    { WARP_ALGO_LZ4,    l_len, l_mbps },
    { WARP_ALGO_SNAPPY, s_len, s_mbps }
  };
  for (int i=0;i<3;i++) if (cands[i].clen>0) {
    double ratio = (double)cands[i].clen / (double)in_len;
    double s = 0.0;
    if (mode == WARP_AUTO_THROUGHPUT) s = cands[i].mbps;
    else if (mode == WARP_AUTO_RATIO) s = (1.0 - ratio) * 1000.0;
    else s = cands[i].mbps * (1.0 + 3.0 * (1.0 - ratio));
    if (s > best) { best = s; best_algo = cands[i].algo; }
  }
  return best_algo;
}

/* ===== API ===== */
int warp_compress_file(const char *in_path, const char *out_path, const warp_opts_t *opt) {
  const int threads   = opt->threads > 0 ? opt->threads : 1;
  const int level     = opt->level   > 0 ? opt->level   : 1;
  const int prefer    = opt->algo; /* 0=auto */
  const int do_idx    = opt->do_index;
  const int do_chk    = opt->chk_kind;

  size_t total = fsize(in_path);
  if (!total) { fprintf(stderr,"input not found or empty\n"); return 1; }

  uint32_t chunk = opt->chunk_bytes > 0 ? (uint32_t)opt->chunk_bytes : warp_pick_chunk_size(total);
  uint32_t n = (uint32_t)((total + chunk - 1) / chunk);

  int fd_in = open(in_path, O_RDONLY);
  if (fd_in < 0) { perror("open in"); return 1; }
  wc_advise_sequential(fd_in);

  FILE *fout = fopen(out_path, "wb+");
  if (!fout) { perror("fopen out"); close(fd_in); return 1; }
  int fd_out = fileno(fout);
  (void)fd_out; /* currently not used directly here */

  /* output buffer capacity (max of codec bounds) */
  size_t out_cap = chunk;
  size_t zcap = zstd_max_compressed_size(chunk); if (zcap > out_cap) out_cap = zcap;
#ifdef HAVE_LZ4
  size_t lcap = lz4_max_compressed_size(chunk); if (lcap > out_cap) out_cap = lcap;
#endif
#ifdef HAVE_SNAPPY
  size_t scap = snappy_max_compressed_size(chunk); if (scap > out_cap) out_cap = scap;
#endif

  /* pools sized ~2x threads for overlap */
  bufpool_t *in_pool  = pool_create(chunk, threads*2);
  bufpool_t *out_pool = pool_create(out_cap, threads*2);
  if (!in_pool || !out_pool) {
    fprintf(stderr,"pool OOM\n");
    if (in_pool)  pool_destroy(in_pool);
    if (out_pool) pool_destroy(out_pool);
    fclose(fout); close(fd_in);
    return 1;
  }

  warp_header_t hdr = { .magic=WARP_MAGIC, .version=WARP_VER,
                        .base_algo = prefer ? prefer : WARP_ALGO_ZSTD,
                        .flags=0, .chunk_size=chunk, .chunk_count=n,
                        .orig_size=total, .comp_size=0 };

  if (fwrite(&hdr, sizeof(hdr), 1, fout) != 1) { perror("write hdr"); goto fail; }
  warp_chunk_t *table = (warp_chunk_t*)calloc(n, sizeof(*table));
  if (!table) { goto fail; }
  if (fwrite(table, sizeof(*table), n, fout) != n) { perror("write table"); free(table); goto fail; }

  /* ---------- Compress all chunks (simple pass; you can keep warm-up logic if desired) ---------- */
  tp_t *tp = tp_create(threads);
  c_job_t *jobs = (c_job_t*)calloc(n, sizeof(*jobs));
  for (uint32_t i=0;i<n;i++) {
    size_t off = (size_t)i * chunk;
    size_t len = (off + chunk <= total) ? chunk : (total - off);
    jobs[i] = (c_job_t){ .fd=fd_in, .offset=off, .len=len,
                         .prefer_algo=prefer, .level=level, .idx=(int)i,
                         .in_pool=in_pool, .out_pool=out_pool, .out_cap=out_cap };
    tp_submit(tp, do_compress, &jobs[i]);
  }
  tp_barrier(tp);

  /* Write payloads and fill table */
  for (uint32_t i=0;i<n;i++) {
    if (!jobs[i].ok) { fprintf(stderr,"compress chunk %u failed\n", i); free(jobs); free(table); tp_destroy(tp); goto fail; }
    long my_off = ftell(fout);
    table[i].orig_len = (uint32_t)jobs[i].len;
    table[i].comp_len = (uint32_t)jobs[i].comp_len;
    table[i].offset   = (uint64_t)my_off;
    table[i].algo     = (uint8_t)jobs[i].out_algo;

    if (jobs[i].out_algo != WARP_ALGO_ZERO) {
      if (fwrite(jobs[i].comp, 1, jobs[i].comp_len, fout) != jobs[i].comp_len) {
        perror("write payload");
        free(jobs); free(table); tp_destroy(tp); goto fail;
      }
      hdr.comp_size += jobs[i].comp_len;
      pool_release(out_pool, jobs[i].comp);
    }
    pool_release(in_pool, jobs[i].in_buf);
  }
  free(jobs);
  tp_destroy(tp);

  /* Patch header+table */
  fseek(fout, sizeof(hdr), SEEK_SET);
  fwrite(table, sizeof(*table), n, fout);
  fseek(fout, 0, SEEK_SET);
  fwrite(&hdr, sizeof(hdr), 1, fout);

  /* Trailers (index + checksum + footer) */
  uint64_t wix_off = 0, chk_off = 0;
  if (do_idx) {
    wix_off = (uint64_t)ftell(fout);
    wix_header_t wh = { .magic = WIX_MAGIC, .count = n };
    fwrite(&wh, sizeof(wh), 1, fout);
    for (uint32_t i=0;i<n;i++) {
      wix_entry_v1_t e = { .payload_off = table[i].offset, .orig_len=table[i].orig_len, .comp_len=table[i].comp_len, .algo=table[i].algo };
      fwrite(&e, sizeof(e), 1, fout);
    }
    uint32_t crc0 = 0; fwrite(&crc0, 4, 1, fout);
  }

#ifdef HAVE_XXHASH
  if (do_chk == WARP_CHK_XXH64) {
    chk_off = (uint64_t)ftell(fout);
    wchk_header_t ch = { .magic = WCHK_MAGIC, .kind = WARP_CHK_XXH64, .dlen = 8, ._rsv={0,0} };
    fwrite(&ch, sizeof(ch), 1, fout);
    /* stream original to compute xxh64 */
    XXH64_state_t* st = XXH64_createState();
    XXH64_reset(st, 0);
    unsigned char *buf = (unsigned char*)malloc(1<<20);
    int fd2 = open(in_path, O_RDONLY);
    for (;;) {
      ssize_t r2 = read(fd2, buf, 1<<20);
      if (r2 < 0) { perror("read chk"); break; }
      if (r2 == 0) break;
      XXH64_update(st, buf, (size_t)r2);
    }
    close(fd2);
    free(buf);
    unsigned long long d = XXH64_digest(st);
    XXH64_freeState(st);
    fwrite(&d, 8, 1, fout);
  }
#else
  (void)do_chk;
#endif

  {
    wftr_footer_t ft = { .magic = WFTR_MAGIC, .wix_off = wix_off, .chk_off = chk_off };
    fwrite(&ft, sizeof(ft), 1, fout);
  }

  if (opt->verbose) fprintf(stderr,"compressed %zu -> %llu bytes in %u chunks\n",
                            total, (unsigned long long)hdr.comp_size, n);

  free(table);
  pool_destroy(in_pool);
  pool_destroy(out_pool);
  fclose(fout);
  close(fd_in);
  return 0;

fail:
  pool_destroy(in_pool);
  pool_destroy(out_pool);
  fclose(fout);
  close(fd_in);
  return 2;
}

int warp_decompress_file(const char *in_path, const char *out_path, const warp_opts_t *opt) {
  int fd_in = open(in_path, O_RDONLY);
  if (fd_in < 0) { perror("open in"); return 1; }
  FILE *fin = fdopen(fd_in, "rb");
  if (!fin) { perror("fdopen"); close(fd_in); return 1; }

  warp_header_t hdr;
  if (fread(&hdr, sizeof(hdr), 1, fin) != 1) { fprintf(stderr,"bad header\n"); fclose(fin); return 2; }
  if (hdr.magic != WARP_MAGIC || hdr.version != WARP_VER) { fprintf(stderr,"bad magic/version\n"); fclose(fin); return 2; }

  warp_chunk_t *table = (warp_chunk_t*)malloc(sizeof(*table) * hdr.chunk_count);
  if (!table) { fclose(fin); return 3; }
  if (fread(table, sizeof(*table), hdr.chunk_count, fin) != hdr.chunk_count) {
    fprintf(stderr,"bad table\n"); free(table); fclose(fin); return 2;
  }

  /* output */
  FILE *fout = fopen(out_path, "wb+");
  if (!fout) { perror("fopen out"); free(table); fclose(fin); return 1; }
  /* FIX: call the correct function name (wc_ftruncate_file) */
  (void)wc_ftruncate_file(fout, hdr.orig_size);

  /* pool for decode outputs */
  bufpool_t *out_pool = pool_create(hdr.chunk_size, opt->threads>0?opt->threads*2:2);
  if (!out_pool) { fprintf(stderr,"pool OOM\n"); free(table); fclose(fout); fclose(fin); return 1; }

  tp_t *tp = tp_create(opt->threads > 0 ? opt->threads : 1);
  d_job_t *jobs = (d_job_t*)calloc(hdr.chunk_count, sizeof(*jobs));
  for (uint32_t i=0;i<hdr.chunk_count;i++) {
    jobs[i] = (d_job_t){ .fd=fd_in, .idx=(int)i, .ent=table[i], .out_pool=out_pool };
    tp_submit(tp, do_decompress, &jobs[i]);
  }
  tp_barrier(tp);

#ifdef HAVE_XXHASH
  XXH64_state_t* st = NULL;
  wftr_footer_t ft;
  long endpos;
  fseek(fin, 0, SEEK_END); endpos = ftell(fin);
  fseek(fin, endpos - (long)sizeof(ft), SEEK_SET);
  if (fread(&ft, sizeof(ft), 1, fin) != 1 || ft.magic != WFTR_MAGIC) { ft.chk_off=0; }
  if (opt->verify && ft.chk_off) { st = XXH64_createState(); XXH64_reset(st, 0); }
#endif

  /* ordered write to exact offsets */
  uint64_t off = 0;
  int fout_fd = fileno(fout);
  for (uint32_t i=0;i<hdr.chunk_count;i++) {
    if (!jobs[i].ok) {
      fprintf(stderr,"decompress chunk %u failed\n", i);
      free(jobs); pool_destroy(out_pool); free(table); fclose(fout); fclose(fin);
      return 2;
    }
    wc_pwrite(fout_fd, jobs[i].buf, table[i].orig_len, off);
#ifdef HAVE_XXHASH
    if (st) XXH64_update(st, jobs[i].buf, table[i].orig_len);
#endif
    off += table[i].orig_len;
    pool_release(out_pool, jobs[i].buf);
  }

#ifdef HAVE_XXHASH
  if (st) {
    unsigned long long have = XXH64_digest(st);
    XXH64_freeState(st);
    /* read expected */
    wftr_footer_t ft2; fseek(fin, endpos - (long)sizeof(ft2), SEEK_SET); fread(&ft2, sizeof(ft2), 1, fin);
    fseek(fin, (long)ft2.chk_off, SEEK_SET);
    wchk_header_t ch; fread(&ch, sizeof(ch), 1, fin);
    unsigned long long want=0; fread(&want, 8, 1, fin);
    if (!(ch.magic==WCHK_MAGIC && ch.kind==WARP_CHK_XXH64 && ch.dlen==8 && want==have)) {
      fprintf(stderr,"checksum mismatch\n");
    }
  }
#endif

  free(jobs); pool_destroy(out_pool);
  free(table);
  fclose(fout);
  fclose(fin);
  return 0;
}




