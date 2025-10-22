// src/container.c
#include "container.h"
#include "threadpool.h"
#include "codecs.h"
#include "util.h"
#include "bufpool.h"

#ifdef HAVE_XXHASH
#  include <xxhash.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

/* ---------- tiny helpers ---------- */

static size_t fsize(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) return 0;
  return (size_t)st.st_size;
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
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
#else
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
#endif
}

/* ---------- per-chunk jobs ---------- */

typedef struct {
  int fd;
  size_t offset, len;
  int prefer_algo;   /* 0=auto, else fixed WARP_ALGO_* */
  int level;
  uint32_t idx;

  bufpool_t *in_pool;
  bufpool_t *out_pool;
  size_t     out_cap;

  /* results */
  unsigned char *in_buf;
  unsigned char *comp;
  size_t comp_len;
  int out_algo;
  double secs;
  int ok;
} c_job_t;

typedef struct {
  int fd;
  uint32_t idx;
  warp_chunk_t ent;
  bufpool_t *out_pool;
  unsigned char *buf;
  int ok;
} d_job_t;

/* ---------- codec trials ---------- */

static size_t try_algo_zstd(const unsigned char *in, size_t in_len, int level,
                            unsigned char *out, size_t out_cap) {
  return zstd_compress(out, out_cap, in, in_len, level);
}

static size_t try_algo_lz4(const unsigned char *in, size_t in_len,
                           unsigned char *out, size_t out_cap) {
  return wc_lz4_compress(out, out_cap, in, in_len);
}

static size_t try_algo_snappy(const unsigned char *in, size_t in_len,
                              unsigned char *out, size_t out_cap) {
  return wc_snappy_compress(out, out_cap, in, in_len);
}

/* ---------- worker bodies ---------- */

static void do_compress(void *arg) {
  c_job_t *j = (c_job_t*)arg;

  j->in_buf = (unsigned char*)pool_acquire(j->in_pool);
  unsigned char *out = (unsigned char*)pool_acquire(j->out_pool);
  if (!j->in_buf || !out) { j->ok = 0; return; }

  ssize_t r = wc_pread(j->fd, j->in_buf, j->len, (uint64_t)j->offset);
  if (r != (ssize_t)j->len) { j->ok = 0; return; }

  /* ZERO fast-path */
  if (is_all_zero(j->in_buf, j->len)) {
    j->comp = NULL;
    j->comp_len = 0;
    j->out_algo = WARP_ALGO_ZERO;
    j->secs = 0.0;
    j->ok = 1;
    return;
  }

  /* candidate set */
  int cands[4]; int cc = 0;
  if (j->prefer_algo == 0) {
    cands[cc++] = WARP_ALGO_ZSTD;
    cands[cc++] = WARP_ALGO_LZ4;
    cands[cc++] = WARP_ALGO_SNAPPY;
  } else {
    cands[cc++] = j->prefer_algo;
  }

  double best_score = -1e300;
  size_t best_len = 0;
  int    best_algo = WARP_ALGO_COPY;
  double best_secs = 0.0;

  for (int k = 0; k < cc; k++) {
    int algo = cands[k];
    size_t got = 0;
    double t0 = now_secs();
    if (algo == WARP_ALGO_ZSTD)       got = try_algo_zstd  (j->in_buf, j->len, j->level, out, j->out_cap);
    else if (algo == WARP_ALGO_LZ4)   got = try_algo_lz4   (j->in_buf, j->len, out, j->out_cap);
    else if (algo == WARP_ALGO_SNAPPY)got = try_algo_snappy(j->in_buf, j->len, out, j->out_cap);
    else if (algo == WARP_ALGO_COPY)  { memcpy(out, j->in_buf, j->len); got = j->len; }
    double dt = now_secs() - t0;
    if (!got) continue;
    double mbps = dt > 0 ? (j->len / (1024.0 * 1024.0)) / dt : 0.0;
    if (mbps > best_score) {
      best_score = mbps;
      best_len   = got;
      best_algo  = algo;
      best_secs  = dt;
    }
  }

  /* COPY fallback if not much gain or all failed */
  if (best_len == 0 || best_len >= j->len - (j->len >> 6)) {
    memcpy(out, j->in_buf, j->len);
    best_algo = WARP_ALGO_COPY;
    best_len  = j->len;
    best_secs = 0.0;
  }

  j->comp     = out;
  j->comp_len = best_len;
  j->out_algo = best_algo;
  j->secs     = best_secs;
  j->ok       = 1;
}

static void do_decompress(void *arg) {
  d_job_t *j = (d_job_t*)arg;

  j->buf = (unsigned char*)pool_acquire(j->out_pool);
  if (!j->buf) { j->ok = 0; return; }

  if (j->ent.algo == WARP_ALGO_ZERO) {
    memset(j->buf, 0, j->ent.orig_len);
    j->ok = 1; return;
  }

  unsigned char *comp = (unsigned char*)malloc(j->ent.comp_len);
  if (!comp) { j->ok = 0; return; }
  ssize_t r = wc_pread(j->fd, comp, j->ent.comp_len, j->ent.offset);
  if (r != (ssize_t)j->ent.comp_len) { free(comp); j->ok = 0; return; }

  size_t got = 0;
  switch (j->ent.algo) {
    case WARP_ALGO_COPY:   memcpy(j->buf, comp, j->ent.orig_len); got = j->ent.orig_len; break;
    case WARP_ALGO_ZSTD:   got = zstd_decompress(j->buf, j->ent.orig_len, comp, j->ent.comp_len); break;
    case WARP_ALGO_LZ4:    got = wc_lz4_decompress(j->buf, j->ent.orig_len, comp, j->ent.comp_len); break;
    case WARP_ALGO_SNAPPY: got = wc_snappy_decompress(j->buf, j->ent.orig_len, comp, j->ent.comp_len); break;
    default: got = 0; break;
  }
  free(comp);

  if (got != j->ent.orig_len) { j->ok = 0; return; }
  j->ok = 1;
}

/* ---------- simple policy combiner (warm-up) ---------- */

static int score_pick_algo(int mode, size_t in_len,
                           size_t z_len, double z_mbps,
                           size_t l_len, double l_mbps,
                           size_t s_len, double s_mbps)
{
  double best = -1e300; int best_algo = WARP_ALGO_ZSTD;
  struct { int algo; size_t clen; double mbps; } cands[3] = {
    { WARP_ALGO_ZSTD,   z_len, z_mbps },
    { WARP_ALGO_LZ4,    l_len, l_mbps },
    { WARP_ALGO_SNAPPY, s_len, s_mbps }
  };
  for (int i = 0; i < 3; i++) if (cands[i].clen > 0) {
    double ratio = (double)cands[i].clen / (double)in_len;
    double s = 0.0;
    if (mode == WARP_AUTO_THROUGHPUT)      s = cands[i].mbps;
    else if (mode == WARP_AUTO_RATIO)      s = (1.0 - ratio) * 1000.0;
    else /* balanced */                    s = cands[i].mbps * (1.0 + 3.0 * (1.0 - ratio));
    if (s > best) { best = s; best_algo = cands[i].algo; }
  }
  return best_algo;
}

/* ---------- public API ---------- */

int warp_compress_file(const char *in_path, const char *out_path, const warp_opts_t *opt) {
  const int threads   = opt->threads > 0 ? opt->threads : 1;
  const int level     = opt->level   > 0 ? opt->level   : 1;
  const int prefer    = opt->algo; /* 0=auto */
  const int do_idx    = opt->do_index;
  const int do_chk    = opt->chk_kind;
  const int auto_mode = opt->auto_mode;
  const int warmup    = (opt->auto_lock > 0 ? opt->auto_lock : 4);

  size_t total = fsize(in_path);
  if (!total) { fprintf(stderr, "input not found or empty\n"); return 1; }

  uint32_t chunk = opt->chunk_bytes ? (uint32_t)opt->chunk_bytes : warp_pick_chunk_size(total);
  uint32_t n = (uint32_t)((total + chunk - 1) / chunk);

  int fd_in = open(in_path, O_RDONLY);
  if (fd_in < 0) { perror("open in"); return 1; }
  wc_advise_sequential(fd_in);

  FILE *fout = fopen(out_path, "wb+");
  if (!fout) { perror("fopen out"); close(fd_in); return 1; }

  /* output cap = max bound among codecs for a single chunk */
  size_t out_cap = chunk;
  size_t zcap = zstd_max_compressed_size(chunk); if (zcap > out_cap) out_cap = zcap;
  size_t lcap = lz4_max_compressed_size(chunk);  if (lcap > out_cap) out_cap = lcap;
  size_t scap = snappy_max_compressed_size(chunk); if (scap > out_cap) out_cap = scap;

  bufpool_t *in_pool  = pool_create(chunk,   threads*2);
  bufpool_t *out_pool = pool_create(out_cap, threads*2);
  if (!in_pool || !out_pool) {
    fprintf(stderr, "pool OOM\n");
    if (in_pool)  pool_destroy(in_pool);
    if (out_pool) pool_destroy(out_pool);
    fclose(fout); close(fd_in);
    return 1;
  }

  warp_header_t hdr;
  memset(&hdr, 0, sizeof(hdr));
  hdr.magic       = WARP_MAGIC;
  hdr.version     = WARP_VER;
  hdr.base_algo   = prefer ? prefer : WARP_ALGO_ZSTD;
  hdr.flags       = 0;
  hdr.chunk_size  = chunk;
  hdr.chunk_count = n;
  hdr.orig_size   = total;
  hdr.comp_size   = 0;

  if (fwrite(&hdr, sizeof(hdr), 1, fout) != 1) { perror("write hdr"); fclose(fout); close(fd_in); pool_destroy(in_pool); pool_destroy(out_pool); return 2; }

  warp_chunk_t *table = (warp_chunk_t*)calloc(n, sizeof(*table));
  if (!table) { fclose(fout); close(fd_in); pool_destroy(in_pool); pool_destroy(out_pool); return 3; }

  if (fwrite(table, sizeof(*table), n, fout) != n) { perror("write table"); free(table); fclose(fout); close(fd_in); pool_destroy(in_pool); pool_destroy(out_pool); return 2; }

  /* warm-up */
  int locked_algo = prefer;
  int warm_n = (prefer == 0) ? (warmup < (int)n ? warmup : (int)n) : 0;

  long payload_pos = ftell(fout);

  if (warm_n > 0) {
    tp_t *tp = tp_create(threads);
    c_job_t *jobs = (c_job_t*)calloc(warm_n, sizeof(*jobs));
    for (int i = 0; i < warm_n; i++) {
      size_t off = (size_t)i * chunk;
      size_t len = (off + chunk <= total) ? chunk : (total - off);
      jobs[i].fd = fd_in; jobs[i].offset = off; jobs[i].len = len;
      jobs[i].prefer_algo = 0; jobs[i].level = level; jobs[i].idx = (uint32_t)i;
      jobs[i].in_pool = in_pool; jobs[i].out_pool = out_pool; jobs[i].out_cap = out_cap;
      tp_submit(tp, do_compress, &jobs[i]);
    }
    tp_barrier(tp);

    double z_mbps=0,z_cnt=0, z_ratio=0;
    double l_mbps=0,l_cnt=0, l_ratio=0;
    double s_mbps=0,s_cnt=0, s_ratio=0;

    for (int i = 0; i < warm_n; i++) {
      if (!jobs[i].ok) { fprintf(stderr, "warm-up chunk %d failed\n", i);
        free(jobs); free(table); fclose(fout); close(fd_in); pool_destroy(in_pool); pool_destroy(out_pool); return 2; }
      double mbps  = (jobs[i].secs>0) ? (jobs[i].len/(1024.0*1024.0))/jobs[i].secs : 0.0;
      double ratio = (double)jobs[i].comp_len / (double)jobs[i].len;
      if (jobs[i].out_algo == WARP_ALGO_ZSTD)   { z_mbps += mbps; z_cnt++; z_ratio += ratio; }
      if (jobs[i].out_algo == WARP_ALGO_LZ4)    { l_mbps += mbps; l_cnt++; l_ratio += ratio; }
      if (jobs[i].out_algo == WARP_ALGO_SNAPPY) { s_mbps += mbps; s_cnt++; s_ratio += ratio; }
    }
    if (z_cnt>0) { z_mbps/=z_cnt; z_ratio/=z_cnt; }
    if (l_cnt>0) { l_mbps/=l_cnt; l_ratio/=l_cnt; }
    if (s_cnt>0) { s_mbps/=s_cnt; s_ratio/=s_cnt; }

    locked_algo = score_pick_algo(auto_mode, chunk,
                                  (size_t)(z_ratio*chunk), z_mbps,
                                  (size_t)(l_ratio*chunk), l_mbps,
                                  (size_t)(s_ratio*chunk), s_mbps);

    /* write warm chunks sequentially */
    fseek(fout, payload_pos, SEEK_SET);
    size_t written_here = 0;
    for (int i = 0; i < warm_n; i++) {
      long my_off = (long)payload_pos + (long)written_here;
      table[i].orig_len = (uint32_t)jobs[i].len;
      table[i].comp_len = (uint32_t)jobs[i].comp_len;
      table[i].offset   = (uint64_t)my_off;
      table[i].algo     = (uint8_t)jobs[i].out_algo;

      if (jobs[i].out_algo != WARP_ALGO_ZERO) {
        if (fwrite(jobs[i].comp, jobs[i].comp_len, 1, fout) != 1) { perror("fwrite"); }
        written_here += jobs[i].comp_len;
        pool_release(out_pool, jobs[i].comp);
      }
      pool_release(in_pool, jobs[i].in_buf);
    }
    hdr.comp_size += written_here;
    payload_pos += (long)written_here;

    free(jobs);
    tp_destroy(tp);
  }

  /* rest with locked algo */
  if (warm_n < (int)n) {
    int rest = (int)n - warm_n;
    tp_t *tp2 = tp_create(threads);
    c_job_t *jobs2 = (c_job_t*)calloc(rest, sizeof(*jobs2));
    for (int j = 0; j < rest; j++) {
      uint32_t i = (uint32_t)(warm_n + j);
      size_t off = (size_t)i * chunk;
      size_t len = (off + chunk <= total) ? chunk : (total - off);
      jobs2[j].fd = fd_in; jobs2[j].offset = off; jobs2[j].len = len;
      jobs2[j].prefer_algo = (locked_algo ? locked_algo : WARP_ALGO_ZSTD);
      jobs2[j].level = level; jobs2[j].idx = i;
      jobs2[j].in_pool = in_pool; jobs2[j].out_pool = out_pool; jobs2[j].out_cap = out_cap;
      tp_submit(tp2, do_compress, &jobs2[j]);
    }
    tp_barrier(tp2);

    fseek(fout, payload_pos, SEEK_SET);
    size_t written_here = 0;

    for (int j = 0; j < rest; j++) {
      uint32_t i = (uint32_t)(warm_n + j);
      if (!jobs2[j].ok) {
        fprintf(stderr, "compress chunk %u failed\n", i);
        free(jobs2); free(table); fclose(fout); close(fd_in); pool_destroy(in_pool); pool_destroy(out_pool);
        return 2;
      }
      long my_off = (long)payload_pos + (long)written_here;
      table[i].orig_len = (uint32_t)jobs2[j].len;
      table[i].comp_len = (uint32_t)jobs2[j].comp_len;
      table[i].offset   = (uint64_t)my_off;
      table[i].algo     = (uint8_t)jobs2[j].out_algo;

      if (jobs2[j].out_algo != WARP_ALGO_ZERO) {
        if (fwrite(jobs2[j].comp, jobs2[j].comp_len, 1, fout) != 1) { perror("fwrite"); }
        written_here += jobs2[j].comp_len;
        pool_release(out_pool, jobs2[j].comp);
      }
      pool_release(in_pool, jobs2[j].in_buf);
    }
    hdr.comp_size += written_here;
    payload_pos += (long)written_here;

    free(jobs2);
    tp_destroy(tp2);
  }

  /* patch table + header */
  fseek(fout, sizeof(hdr), SEEK_SET);
  fwrite(table, sizeof(*table), n, fout);
  fseek(fout, 0, SEEK_SET);
  fwrite(&hdr, sizeof(hdr), 1, fout);
  fseek(fout, 0, SEEK_END);

  /* optional trailers: index + checksum + footer */
  uint64_t wix_off = 0, chk_off = 0;

  if (do_idx) {
    wix_off = (uint64_t)ftell(fout);
    wix_header_t wh = { WIX_MAGIC, n };
    fwrite(&wh, sizeof(wh), 1, fout);
    for (uint32_t i = 0; i < n; i++) {
      wix_entry_v1_t e;
      e.payload_off = table[i].offset;
      e.orig_len    = table[i].orig_len;
      e.comp_len    = table[i].comp_len;
      e.algo        = table[i].algo;
      memset(e._pad, 0, sizeof(e._pad));
      fwrite(&e, sizeof(e), 1, fout);
    }
    uint32_t crc0 = 0;
    fwrite(&crc0, 4, 1, fout);
  }

#ifdef HAVE_XXHASH
  if (do_chk == WARP_CHK_XXH64) {
    chk_off = (uint64_t)ftell(fout);
    wchk_header_t ch = { WCHK_MAGIC, WARP_CHK_XXH64, 8, {0,0} };
    fwrite(&ch, sizeof(ch), 1, fout);

    /* stream original and compute xxh64 */
    XXH64_state_t* st = XXH64_createState();
    XXH64_reset(st, 0);
    int fd2 = open(in_path, O_RDONLY);
    if (fd2 >= 0) {
      const size_t buf_sz = 1u << 20;
      unsigned char *buf = (unsigned char*)malloc(buf_sz);
      if (buf) {
        for (;;) {
          ssize_t r2 = read(fd2, buf, (unsigned)buf_sz);
          if (r2 < 0) { perror("read chk"); break; }
          if (r2 == 0) break;
          XXH64_update(st, buf, (size_t)r2);
        }
        free(buf);
      }
      close(fd2);
    }
    unsigned long long d = XXH64_digest(st);
    XXH64_freeState(st);
    fwrite(&d, 8, 1, fout);
  }
#else
  (void)do_chk;
#endif

  wftr_footer_t ft = { WFTR_MAGIC, wix_off, chk_off };
  fwrite(&ft, sizeof(ft), 1, fout);

  if (opt->verbose) {
    fprintf(stderr, "compressed %zu -> %llu bytes in %u chunks (locked algo=%d)\n",
            total, (unsigned long long)hdr.comp_size, n,
            locked_algo ? locked_algo : WARP_ALGO_ZSTD);
  }

  free(table);
  pool_destroy(in_pool);
  pool_destroy(out_pool);
  fclose(fout);
  close(fd_in);
  return 0;
}

int warp_decompress_file(const char *in_path, const char *out_path, const warp_opts_t *opt) {
  int fd_in = open(in_path, O_RDONLY);
  if (fd_in < 0) { perror("open in"); return 1; }
  FILE *fin = fdopen(fd_in, "rb");
  if (!fin) { perror("fdopen"); close(fd_in); return 1; }

  warp_header_t hdr;
  if (fread(&hdr, sizeof(hdr), 1, fin) != 1) { fprintf(stderr, "bad header\n"); fclose(fin); return 2; }
  if (hdr.magic != WARP_MAGIC || hdr.version != WARP_VER) { fprintf(stderr, "bad magic/version\n"); fclose(fin); return 2; }

  warp_chunk_t *table = (warp_chunk_t*)malloc(sizeof(*table) * hdr.chunk_count);
  if (!table) { fclose(fin); return 3; }
  if (fread(table, sizeof(*table), hdr.chunk_count, fin) != hdr.chunk_count) {
    fprintf(stderr, "bad table\n"); free(table); fclose(fin); return 2;
  }

  FILE *fout = fopen(out_path, "wb+");
  if (!fout) { perror("fopen out"); free(table); fclose(fin); return 1; }
  (void)wc_ftruncate_file(fout, hdr.orig_size); /* best-effort */

  bufpool_t *out_pool = pool_create(hdr.chunk_size, opt->threads>0 ? opt->threads*2 : 2);
  if (!out_pool) { fprintf(stderr, "pool OOM\n"); free(table); fclose(fout); fclose(fin); return 1; }

  tp_t *tp = tp_create(opt->threads > 0 ? opt->threads : 1);
  d_job_t *jobs = (d_job_t*)calloc(hdr.chunk_count, sizeof(*jobs));
  for (uint32_t i = 0; i < hdr.chunk_count; i++) {
    jobs[i].fd  = fd_in;
    jobs[i].idx = i;
    jobs[i].ent = table[i];
    jobs[i].out_pool = out_pool;
    tp_submit(tp, do_decompress, &jobs[i]);
  }
  tp_barrier(tp);

#ifdef HAVE_XXHASH
  XXH64_state_t* st = NULL;
  wftr_footer_t ft;
  long endpos;
  fseek(fin, 0, SEEK_END); endpos = ftell(fin);
  fseek(fin, endpos - (long)sizeof(ft), SEEK_SET);
  if (fread(&ft, sizeof(ft), 1, fin) != 1 || ft.magic != WFTR_MAGIC) { ft.chk_off = 0; }
  if (opt->verify && ft.chk_off) { st = XXH64_createState(); XXH64_reset(st, 0); }
#endif

  /* ordered writes (pwrite) */
  uint64_t off = 0;
  const int out_fd = fileno(fout);
  for (uint32_t i = 0; i < hdr.chunk_count; i++) {
    if (!jobs[i].ok) {
      fprintf(stderr, "decompress chunk %u failed\n", i);
      free(jobs); pool_destroy(out_pool); free(table); fclose(fout); fclose(fin); return 2;
    }
    wc_pwrite(out_fd, jobs[i].buf, table[i].orig_len, off);
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
    wftr_footer_t ft2;
    fseek(fin, endpos - (long)sizeof(ft2), SEEK_SET);
    fread(&ft2, sizeof(ft2), 1, fin);
    fseek(fin, (long)ft2.chk_off, SEEK_SET);
    wchk_header_t ch; fread(&ch, sizeof(ch), 1, fin);
    unsigned long long want = 0; fread(&want, 8, 1, fin);
    if (!(ch.magic==WCHK_MAGIC && ch.kind==WARP_CHK_XXH64 && ch.dlen==8 && want==have)) {
      fprintf(stderr, "checksum mismatch\n");
    }
  }
#endif

  free(jobs);
  pool_destroy(out_pool);
  free(table);
  fclose(fout);
  fclose(fin);
  return 0;
}





