#ifndef CODECS_H
#define CODECS_H
#include <stddef.h>
#include <stdint.h>

/* -------- ZSTD (required) -------- */
typedef struct zstd_ctx zstd_ctx_t;
zstd_ctx_t* zstd_ctx_create(int level, int nb_workers);
void        zstd_ctx_free(zstd_ctx_t*);
size_t      zstd_compress_ctx(zstd_ctx_t*, void *dst, size_t dst_cap, const void *src, size_t src_sz);

size_t zstd_max_compressed_size(size_t src_size);
size_t zstd_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz);

/* -------- LZ4 (optional) -------- */
size_t lz4_max_compressed_size(size_t src_size);
size_t lz4_compress_fast(void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t lz4_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz);

/* -------- Snappy (optional) -------- */
size_t snappy_max_compressed_size(size_t src_size);
size_t snappy_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz);
size_t snappy_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz);

#endif

