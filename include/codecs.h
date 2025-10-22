#ifndef CODECS_H
#define CODECS_H

#include <stddef.h>
#include <stdint.h>

/* ---------- Zstd ---------- */
size_t zstd_compress   (void *dst, size_t dst_cap, const void *src, size_t src_sz, int level);
size_t zstd_decompress (void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t zstd_max_compressed_size(size_t src_sz);

/* ---------- LZ4 (prefixed wrappers) ---------- */
size_t wc_lz4_compress   (void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t wc_lz4_decompress (void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t lz4_max_compressed_size(size_t src_sz);

/* ---------- Snappy (prefixed wrappers) ---------- */
size_t wc_snappy_compress   (void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t wc_snappy_decompress (void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t snappy_max_compressed_size(size_t src_sz);

#endif /* CODECS_H */
