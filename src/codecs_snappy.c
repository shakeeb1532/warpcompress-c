#include "codecs.h"
#ifdef HAVE_SNAPPY
#include <snappy-c.h>

size_t snappy_max_compressed_size(size_t src_size) {
  return snappy_max_compressed_length(src_size);
}
size_t snappy_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
  size_t out_len = dst_cap;
  snappy_status st = snappy_compress((const char*)src, src_sz, (char*)dst, &out_len);
  return st == SNAPPY_OK ? out_len : 0;
}
size_t snappy_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
  size_t out_len = dst_cap;
  snappy_status st = snappy_uncompress((const char*)src, src_sz, (char*)dst, &out_len);
  return st == SNAPPY_OK ? out_len : 0;
}
#else
size_t snappy_max_compressed_size(size_t s){(void)s;return 0;}
size_t snappy_compress(void *a,size_t b,const void*c,size_t d){(void)a;(void)b;(void)c;(void)d;return 0;}
size_t snappy_decompress(void *a,size_t b,const void*c,size_t d){(void)a;(void)b;(void)c;(void)d;return 0;}
#endif
