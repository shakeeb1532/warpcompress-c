// include/container.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* ========= Container constants ========= */

/* 'WARP' */
#define WARP_MAGIC   ((uint32_t)0x57415250u)
/* container format version */
#define WARP_VER     ((uint16_t)0x0001u)

/* 'WIX1' (index block) */
#define WIX_MAGIC    ((uint32_t)0x57495831u)
/* 'WCHK' (checksum block) */
#define WCHK_MAGIC   ((uint32_t)0x5743484Bu)
/* 'WFTR' (trailer/footer) */
#define WFTR_MAGIC   ((uint32_t)0x57465452u)

/* Algorithms stored per chunk */
typedef enum {
  WARP_ALGO_COPY   = 1,   /* stored verbatim */
  WARP_ALGO_ZERO   = 2,   /* all zeros (no payload) */
  WARP_ALGO_ZSTD   = 3,
  WARP_ALGO_LZ4    = 4,
  WARP_ALGO_SNAPPY = 5
} warp_algo_t;

/* Auto mode policy (for warm-up scoring) */
typedef enum {
  WARP_AUTO_THROUGHPUT = 0,
  WARP_AUTO_BALANCED   = 1,
  WARP_AUTO_RATIO      = 2
} warp_auto_mode_t;

/* Checksum kinds (optional trailer) */
typedef enum {
  WARP_CHK_NONE  = 0,
  WARP_CHK_XXH64 = 1
} warp_chk_kind_t;


/* ========= On-disk structures ========= */

/* File header written at the beginning of the .warp file.
   After this header, a flat chunk table of chunk_count entries follows. */
typedef struct {
  uint32_t  magic;        /* WARP_MAGIC */
  uint16_t  version;      /* WARP_VER   */
  uint8_t   base_algo;    /* hint for readers, not required */
  uint8_t   flags;        /* reserved for future */
  uint32_t  chunk_size;   /* nominal chunk size in bytes */
  uint32_t  chunk_count;  /* number of chunks in the table */
  uint64_t  orig_size;    /* original (uncompressed) total size */
  uint64_t  comp_size;    /* total compressed payload bytes (not including header/table/index) */
} warp_header_t;

/* One table entry per chunk.  Payload is at 'offset' unless algo==ZERO (no payload). */
typedef struct {
  uint64_t  offset;       /* file offset where chunk payload starts */
  uint32_t  orig_len;     /* bytes after decompress */
  uint32_t  comp_len;     /* bytes stored (0 if ZERO) */
  uint8_t   algo;         /* warp_algo_t */
  uint8_t   _pad[7];      /* reserved/padding for 16-byte alignment */
} warp_chunk_t;

/* Optional index block written near end of file (if enabled). */
typedef struct {
  uint32_t magic;         /* WIX_MAGIC */
  uint32_t count;         /* number of entries (== chunk_count) */
} wix_header_t;

typedef struct {
  uint64_t payload_off;   /* same as chunk.offset */
  uint32_t orig_len;      /* same as chunk.orig_len */
  uint32_t comp_len;      /* same as chunk.comp_len */
  uint8_t  algo;          /* same as chunk.algo */
  uint8_t  _rsv[7];
} wix_entry_v1_t;

/* Optional checksum block header (followed by digest bytes).
   For XXH64, dlen==8 and 8 bytes of digest follow. */
typedef struct {
  uint32_t magic;         /* WCHK_MAGIC */
  uint32_t kind;          /* warp_chk_kind_t */
  uint32_t dlen;          /* digest length in bytes */
  uint32_t _rsv[2];
} wchk_header_t;

/* Final fixed footer that points to optional sections. */
typedef struct {
  uint32_t magic;         /* WFTR_MAGIC */
  uint32_t _rsv;          /* reserved */
  uint64_t wix_off;       /* 0 if no index */
  uint64_t chk_off;       /* 0 if no checksum block */
} wftr_footer_t;


/* ========= Public options & API ========= */

/* Runtime options controlling compression/decompression. */
typedef struct {
  /* general */
  int       threads;        /* worker threads (>=1) */
  int       level;          /* codec level hint (e.g., zstd) */
  int       algo;           /* 0 = AUTO (use policy), else fixed warp_algo_t */

  /* policy & chunking */
  uint32_t  chunk_bytes;    /* override chunk size (0 = auto policy) */
  int       auto_mode;      /* warp_auto_mode_t */
  int       auto_lock;      /* warm-up sample count before locking algo (0=default) */

  /* trailers */
  int       do_index;       /* write index at end */
  int       chk_kind;       /* warp_chk_kind_t */
  int       verify;         /* verify output against checksum on decode */

  /* diagnostics */
  int       verbose;        /* extra logs */
} warp_opts_t;

/* Default initialiser helper (use by-value). */
static inline warp_opts_t warp_opts_default(void) {
  warp_opts_t o;
  o.threads    = 1;
  o.level      = 1;
  o.algo       = 0;                 /* AUTO */
  o.chunk_bytes= 0;                 /* auto */
  o.auto_mode  = WARP_AUTO_BALANCED;
  o.auto_lock  = 4;
  o.do_index   = 1;
  o.chk_kind   = WARP_CHK_NONE;
  o.verify     = 0;
  o.verbose    = 0;
  return o;
}

/* High-level file APIs implemented in container.c */
int warp_compress_file   (const char *in_path, const char *out_path, const warp_opts_t *opt);
int warp_decompress_file (const char *in_path, const char *out_path, const warp_opts_t *opt);

#ifdef __cplusplus
} /* extern "C" */
#endif
