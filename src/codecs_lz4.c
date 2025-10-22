#include "codecs.h"

#ifdef HAVE_LZ4
#  include <lz4.h>
#endif

size_t lz4_max_compressed_size(size_t src_sz) {
#ifdef HAVE_LZ4
  /* LZ4 only exposes int-based bound; clamp if needed */
  if (src_sz > (size_t)0x7fffffff) src_sz = 0x7fffffff;
  return (size_t)LZ4_compressBound((int)src_sz);
#else
  (void)src_sz;
  return 0;
#endif
}

size_t wc_lz4_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
#ifdef HAVE_LZ4
  if (src_sz > (size_t)0x7fffffff || dst_cap > (size_t)0x7fffffff) return 0;
  int n = LZ4_compress_default((const char*)src, (char*)dst, (int)src_sz, (int)dst_cap);
  return n > 0 ? (size_t)n : 0;
#else
  (void)dst; (void)dst_cap; (void)src; (void)src_sz;
  return 0;
#endif
}

size_t wc_lz4_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
#ifdef HAVE_LZ4
  if (src_sz > (size_t)0x7fffffff || dst_cap > (size_t)0x7fffffff) return 0;
  int n = LZ4_decompress_safe((const char*)src, (char*)dst, (int)src_sz, (int)dst_cap);
  return n >= 0 ? (size_t)n : 0;
#else
  (void)dst; (void)dst_cap; (void)src; (void)src_sz;
  return 0;
#endif
}

