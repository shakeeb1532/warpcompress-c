#ifndef CODECS_H
#define CODECS_H

#include <stddef.h> // for size_t

typedef struct {
    ZSTD_CCtx *cctx;
} zstd_ctx_t;

zstd_ctx_t* zstd_ctx_create(int level, int nb_workers);
void zstd_ctx_free(zstd_ctx_t* ctx);
size_t zstd_compress_ctx(zstd_ctx_t* ctx, void *dst, size_t dst_cap, const void *src, size_t src_sz);

// Existing functions
size_t zstd_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz, int level);
size_t lz4_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz, int level);
size_t snappy_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz, int level);

#endif
