#pragma once
#include <stddef.h>
#include <stdint.h>
#include <zstd.h>

/* --- ZSTD (required) --- */
typedef struct {
  ZSTD_CCtx *cctx;
} zstd_ctx_t;

zstd_ctx_t* zstd_ctx_create(int level, int nb_workers);
void        zstd_ctx_free  (zstd_ctx_t* c);
size_t      zstd_compress_ctx(zstd_ctx_t* c, void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t      zstd_compress  (void *dst, size_t dst_cap, const void *src, size_t src_sz, int level);
size_t      zstd_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t      zstd_max_compressed_size(size_t src_sz);

/* --- LZ4 (optional) --- */
#ifdef HAVE_LZ4
size_t lz4_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t lz4_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t lz4_max_compressed_size(size_t src_sz);
#endif

/* --- Snappy (optional) --- */
#ifdef HAVE_SNAPPY
size_t wc_snappy_compress  (void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t wc_snappy_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t snappy_max_compressed_size(size_t src_sz);
#endif

