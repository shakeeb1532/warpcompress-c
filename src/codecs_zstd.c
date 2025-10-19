#include "codecs.h"
#include <zstd.h>

size_t zstd_max_compressed_size(size_t src_size) {
  return ZSTD_compressBound(src_size);
}

size_t zstd_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz, int level) {
  size_t r = ZSTD_compress(dst, dst_cap, src, src_sz, level);
  return ZSTD_isError(r) ? 0 : r;
}

size_t zstd_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz) {
  size_t r = ZSTD_decompress(dst, dst_cap, src, src_sz);
  return ZSTD_isError(r) ? 0 : r;
}
