#include "warpc/codecs.h"
#include <stddef.h>
#include <stdint.h>

#ifdef HAVE_LZ4
#include <lz4.h>
#endif

static int lz4_bound(size_t src_size, size_t* out_bound) {
#ifdef HAVE_LZ4
  *out_bound = (size_t)LZ4_compressBound((int)src_size);
  return 0;
#else
  (void)src_size; (void)out_bound;
  return -1;
#endif
}

static size_t lz4_compress(const void* src, size_t src_size, void* dst, size_t dst_cap, int level) {
#ifdef HAVE_LZ4
  (void)level; /* simple path: ignore level here */
  int n = LZ4_compress_default((const char*)src, (char*)dst, (int)src_size, (int)dst_cap);
  if (n <= 0) return 0;
  return (size_t)n;
#else
  (void)src;(void)src_size;(void)dst;(void)dst_cap;(void)level;
  return 0;
#endif
}

static size_t lz4_decompress(const void* src, size_t src_size, void* dst, size_t dst_cap) {
#ifdef HAVE_LZ4
  int n = LZ4_decompress_safe((const char*)src, (char*)dst, (int)src_size, (int)dst_cap);
  if (n < 0) return 0;
  return (size_t)n;
#else
  (void)src;(void)src_size;(void)dst;(void)dst_cap;
  return 0;
#endif
}

static const codec_vtable LZ4_VT = {
  .name = "lz4",
  .compress_bound = lz4_bound,
  .compress = lz4_compress,
  .decompress = lz4_decompress
};

extern const codec_vtable* __warpc_register_codec(const codec_vtable* vt, int id);
__attribute__((constructor)) static void reg(void) { __warpc_register_codec(&LZ4_VT, 2); }

