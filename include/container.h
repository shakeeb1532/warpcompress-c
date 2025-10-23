#ifndef WARPC_CONTAINER_H
#define WARPC_CONTAINER_H

#include <stdint.h>

#define WARPC_MAGIC   0x57525033u /* "WRP3" */
#define WARPC_VERSION 3u

typedef enum {
  CODEC_ZSTD = 1,
  CODEC_LZ4  = 2
} warpc_codec;

typedef struct {
  uint32_t magic;         /* WARPC_MAGIC */
  uint16_t version;       /* WARPC_VERSION */
  uint16_t codec;         /* warpc_codec */
  uint32_t chunk_size_k;  /* chunk size in KiB (e.g., 16384 for 16 MiB) */
  uint64_t orig_size;     /* original file size */
} __attribute__((packed)) warpc_header;

/* Each chunk is: [u64 uncompressed][u64 compressed][bytes...] */

#endif

