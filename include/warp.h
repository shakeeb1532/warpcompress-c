#ifndef WARP_H
#define WARP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* Container v3 */
#define WARP_MAGIC 0x50524157u /* 'WARP' little-endian */
#define WARP_VER   3

/* Algorithms */
enum {
  WARP_ALGO_ZSTD   = 1,
  WARP_ALGO_LZ4    = 2,
  WARP_ALGO_SNAPPY = 3,
  WARP_ALGO_COPY   = 4, /* raw/no compression */
  WARP_ALGO_ZERO   = 5  /* virtual zero run (no payload) */
};

/* Checksum kinds (trailers) */
enum {
  WARP_CHK_NONE  = 0,
  WARP_CHK_XXH64 = 1
};

/* Auto target modes */
enum {
  WARP_AUTO_THROUGHPUT = 0,
  WARP_AUTO_BALANCED   = 1,
  WARP_AUTO_RATIO      = 2
};

/* Header at file start */
typedef struct {
  uint32_t magic;       /* WARP_MAGIC */
  uint8_t  version;     /* WARP_VER */
  uint8_t  base_algo;   /* default/base algo used */
  uint16_t flags;       /* reserved */
  uint32_t chunk_size;  /* preferred chunk size */
  uint32_t chunk_count; /* number of chunks */
  uint64_t orig_size;   /* original total bytes */
  uint64_t comp_size;   /* total payload bytes */
} warp_header_t;

/* Table entry per chunk (includes algo) */
typedef struct {
  uint32_t orig_len;
  uint32_t comp_len;
  uint64_t offset;
  uint8_t  algo;
  uint8_t  _pad[7];
} warp_chunk_t;

/* Trailers */
#define WIX_MAGIC  0x31584957u /* "WIX1" */
#define WCHK_MAGIC 0x4B484357u /* "WCHK" */
#define WFTR_MAGIC 0x52544657u /* "WFTR" */

typedef struct {
  uint32_t magic;  /* WIX_MAGIC */
  uint32_t count;  /* entries */
} wix_header_t;

typedef struct {
  uint64_t payload_off;
  uint32_t orig_len;
  uint32_t comp_len;
  uint8_t  algo;
  uint8_t  _pad[7];
} wix_entry_v1_t;

typedef struct {
  uint32_t magic; /* WCHK_MAGIC */
  uint8_t  kind;  /* WARP_CHK_* */
  uint8_t  dlen;  /* digest length */
  uint8_t  _rsv[2];
} wchk_header_t;

/* Footer: add a reserved u32 so designated or positional init works */
typedef struct {
  uint32_t magic;   /* WFTR_MAGIC */
  uint32_t _rsv;    /* reserved (must be written as 0) */
  uint64_t wix_off; /* 0 if absent */
  uint64_t chk_off; /* 0 if absent */
} wftr_footer_t;

/* CLI options */
typedef struct {
  int algo;        /* 0=auto, else explicit WARP_ALGO_* */
  int auto_mode;   /* WARP_AUTO_* */
  int auto_lock;   /* warm-up chunks before locking algo (default 4) */
  int level;       /* codec level (zstd) */
  int threads;     /* worker threads */
  int chunk_bytes; /* 0 = auto policy */
  int do_index;    /* write WIX */
  int chk_kind;    /* WARP_CHK_* */
  int verify;      /* verify on decompress */
  int verbose;
} warp_opts_t;

/* Chunk policy (adaptive): declaration only; implemented in src/util.c */
uint32_t warp_pick_chunk_size(size_t bytes);

/* High-level API */
int warp_compress_file  (const char *in_path, const char *out_path, const warp_opts_t *opt);
int warp_decompress_file(const char *in_path, const char *out_path, const warp_opts_t *opt);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* WARP_H */
