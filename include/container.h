#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* Magic constants */
#define WARP_MAGIC  ((uint32_t)0x57415250u)  /* 'WARP' */
#define WARP_VER    ((uint16_t)0x0001u)
#define WIX_MAGIC   ((uint32_t)0x57495831u)  /* 'WIX1' */
#define WCHK_MAGIC  ((uint32_t)0x5743484Bu)  /* 'WCHK' */
#define WFTR_MAGIC  ((uint32_t)0x57465452u)  /* 'WFTR' */

/* Algorithms */
typedef enum {
  WARP_ALGO_COPY   = 1,
  WARP_ALGO_ZERO   = 2,
  WARP_ALGO_ZSTD   = 3,
  WARP_ALGO_LZ4    = 4,
  WARP_ALGO_SNAPPY = 5
} warp_algo_t;

/* Auto policy */
typedef enum {
  WARP_AUTO_THROUGHPUT = 0,
  WARP_AUTO_BALANCED   = 1,
  WARP_AUTO_RATIO      = 2
} warp_auto_mode_t;

/* Checksum kind */
typedef enum {
  WARP_CHK_NONE  = 0,
  WARP_CHK_XXH64 = 1
} warp_chk_kind_t;

/* Header */
typedef struct {
  uint32_t  magic;
  uint16_t  version;
  uint8_t   base_algo;
  uint8_t   flags;
  uint32_t  chunk_size;
  uint32_t  chunk_count;
  uint64_t  orig_size;
  uint64_t  comp_size;
} warp_header_t;

/* Chunk table entry */
typedef struct {
  uint64_t  offset;
  uint32_t  orig_len;
  uint32_t  comp_len;
  uint8_t   algo;
  uint8_t   _pad[7];
} warp_chunk_t;

/* Index header + entry */
typedef struct {
  uint32_t magic;
  uint32_t count;
} wix_header_t;

typedef struct {
  uint64_t payload_off;
  uint32_t orig_len;
  uint32_t comp_len;
  uint8_t  algo;
  uint8_t  _rsv[7];
} wix_entry_v1_t;

/* Checksum header */
typedef struct {
  uint32_t magic;
  uint32_t kind;   /* warp_chk_kind_t */
  uint32_t dlen;
  uint32_t _rsv[2];
} wchk_header_t;

/* Footer */
typedef struct {
  uint32_t magic;
  uint32_t _rsv;
  uint64_t wix_off;
  uint64_t chk_off;
} wftr_footer_t;

/* Options and API */
typedef struct {
  int       threads;
  int       level;
  int       algo;           /* 0 = AUTO, else warp_algo_t */
  uint32_t  chunk_bytes;    /* 0 = auto policy */
  int       auto_mode;      /* warp_auto_mode_t */
  int       auto_lock;      /* warm-up sample count */
  int       do_index;
  int       chk_kind;       /* warp_chk_kind_t */
  int       verify;
  int       verbose;
} warp_opts_t;

static inline warp_opts_t warp_opts_default(void) {
  warp_opts_t o;
  o.threads = 1; o.level = 1; o.algo = 0;
  o.chunk_bytes = 0; o.auto_mode = WARP_AUTO_BALANCED; o.auto_lock = 4;
  o.do_index = 1; o.chk_kind = WARP_CHK_NONE; o.verify = 0; o.verbose = 0;
  return o;
}

int warp_compress_file   (const char *in_path, const char *out_path, const warp_opts_t *opt);
int warp_decompress_file (const char *in_path, const char *out_path, const warp_opts_t *opt);

#ifdef __cplusplus
} /* extern "C" */
#endif
