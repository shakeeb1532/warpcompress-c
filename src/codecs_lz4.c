#include "codecs.h"
#ifdef HAVE_LZ4
#include <lz4.h>

size_t lz4_max_compressed_size(size_t src_size) {
  return (size_t)LZ4_compressBound((int)src_size);
}
size_t lz4_compress_fast(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
  int n = LZ4_compress_fast((const char*)src, (char*)dst, (int)src_sz, (int)dst_cap, 1);
  return n <= 0 ? 0 : (size_t)n;
}
size_t lz4_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
  int n = LZ4_decompress_safe((const char*)src, (char*)dst, (int)src_sz, (int)dst_cap);
  return n < 0 ? 0 : (size_t)n;
}
#else
size_t lz4_max_compressed_size(size_t s){(void)s;return 0;}
size_t lz4_compress_fast(void *a,size_t b,const void*c,size_t d){(void)a;(void)b;(void)c;(void)d;return 0;}
size_t lz4_decompress(void *a,size_t b,const void*c,size_t d){(void)a;(void)b;(void)c;(void)d;return 0;}
#endif
