#include "codecs.h"
#include <stdlib.h>
#include <string.h>

zstd_ctx_t* zstd_ctx_create(int level, int nb_workers) {
  zstd_ctx_t *c = (zstd_ctx_t*)malloc(sizeof(*c));
  if (!c) return NULL;
  c->cctx = ZSTD_createCCtx();
  if (!c->cctx) { free(c); return NULL; }
  ZSTD_CCtx_setParameter(c->cctx, ZSTD_c_compressionLevel, level);
  ZSTD_CCtx_setParameter(c->cctx, ZSTD_c_nbWorkers, nb_workers > 0 ? nb_workers : 0);
  return c;
}

void zstd_ctx_free(zstd_ctx_t* c) {
  if (!c) return;
  if (c->cctx) ZSTD_freeCCtx(c->cctx);
  free(c);
}

size_t zstd_compress_ctx(zstd_ctx_t* c, void *dst, size_t dst_cap, const void *src, size_t src_sz) {
  size_t r = ZSTD_compress2(c->cctx, dst, dst_cap, src, src_sz);
  if (ZSTD_isError(r)) return 0;
  return r;
}

size_t zstd_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz, int level) {
  size_t r = ZSTD_compress(dst, dst_cap, src, src_sz, level);
  if (ZSTD_isError(r)) return 0;
  return r;
}

size_t zstd_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
  size_t r = ZSTD_decompress(dst, dst_cap, src, src_sz);
  if (ZSTD_isError(r)) return 0;
  return r;
}

size_t zstd_max_compressed_size(size_t src_sz) {
  return ZSTD_compressBound(src_sz);
}

