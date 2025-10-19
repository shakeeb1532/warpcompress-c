#ifndef WARP_H
#define WARP_H

#include <stdint.h>
#include <stddef.h>

#define WARP_MAGIC 0x50524157u /* 'WARP' little-endian */
#define WARP_VER   3

/* Algorithms (extensible) */
enum {
  WARP_ALGO_ZSTD = 1,
  WARP_ALGO_LZ4  = 2, /* future */
  WARP_ALGO_SNAPPY = 3 /* future */
};

typedef struct {
  uint32_t magic;      /* WARP_MAGIC */
  uint8_t  version;    /* WARP_VER */
  uint8_t  algo;       /* WARP_ALGO_* (default/base) */
  uint16_t flags;      /* reserved */
  uint32_t chunk_size; /* MiB-scale chunks, last chunk may be smaller */
  uint32_t chunk_count;
  uint64_t orig_size;
  uint64_t comp_size;  /* total payload bytes that follow the table */
} warp_header_t;

/* One entry per chunk, preceding payloads */
typedef struct {
  uint32_t orig_len;
  uint32_t comp_len;
  uint64_t offset;     /* payload offset from start of file */
} warp_chunk_t;

/* Policy: choose chunk size based on file size (bytes). */
static inline uint32_t warp_pick_chunk_size(size_t bytes) {
  /* Practical defaults tuned for throughput. */
  if (bytes <= (size_t)256<<20)  return 1<<20;   /* 1 MiB */
  if (bytes <= (size_t)1<<30)    return 2<<20;   /* 2 MiB */
  if (bytes <= (size_t)5<<30)    return 8<<20;   /* 8 MiB */
  if (bytes <= (size_t)10<<30)   return 16<<20;  /* 16 MiB */
  if (bytes <= (size_t)50<<30)   return 32<<20;  /* 32 MiB */
  if (bytes <= (size_t)100<<30)  return 64<<20;  /* 64 MiB */
  if (bytes <= (size_t)500<<30)  return 128<<20; /* 128 MiB */
  return 256<<20; /* 256 MiB for really large inputs */
}

int warp_compress_file(const char *in_path, const char *out_path,
                       int algo, int level, int threads, int verbose);

int warp_decompress_file(const char *in_path, const char *out_path,
                         int threads, int verbose);

#endif
