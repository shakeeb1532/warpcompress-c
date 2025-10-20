#pragma once
#include <stddef.h>
#include <stdint.h>

/* Zstd (required) */
size_t zstd_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz, int level);
size_t zstd_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz);

/* LZ4 (optional; compiled when HAVE_LZ4=1) */
#ifdef HAVE_LZ4
size_t lz4_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t lz4_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz);
#endif

/* Snappy (optional; compiled when HAVE_SNAPPY=1)
   NOTE: names are prefixed with wc_ to avoid colliding with snappy-c.h API. */
#ifdef HAVE_SNAPPY
size_t wc_snappy_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t wc_snappy_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz);
#endif
