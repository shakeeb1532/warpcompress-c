#include "warpc/codecs.h"
#include <stddef.h>
#include <stdint.h>

#ifdef HAVE_ZSTD
#include <zstd.h>
#endif

static int zstd_bound(size_t src_size, size_t* out_bound) {
#ifdef HAVE_ZSTD
  size_t b = ZSTD_compressBound(src_size);
  *out_bound = b;
  return 0;
#else
  (void)src_size; (void)out_bound;
  return -1;
#endif
}

static size_t zstd_compress(const void* src, size_t src_size, void* dst, size_t dst_cap, int level) {
#ifdef HAVE_ZSTD
  size_t n = ZSTD_compress(dst, dst_cap, src, src_size, level);
  if (ZSTD_isError(n)) return 0;
  return n;
#else
  (void)src;(void)src_size;(void)dst;(void)dst_cap;(void)level;
  return 0;
#endif
}

static size_t zstd_decompress(const void* src, size_t src_size, void* dst, size_t dst_cap) {
#ifdef HAVE_ZSTD
  size_t n = ZSTD_decompress(dst, dst_cap, src, src_size);
  if (ZSTD_isError(n)) return 0;
  return n;
#else
  (void)src;(void)src_size;(void)dst;(void)dst_cap;
  return 0;
#endif
}

static const codec_vtable ZSTD_VT = {
  .name = "zstd",
  .compress_bound = zstd_bound,
  .compress = zstd_compress,
  .decompress = zstd_decompress
};

/* Public registry helpers (shared across codec units) */
extern const codec_vtable* __warpc_register_codec(const codec_vtable* vt, int id);
__attribute__((constructor)) static void reg(void) { __warpc_register_codec(&ZSTD_VT, 1); }
