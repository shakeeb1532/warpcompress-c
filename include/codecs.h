#ifndef CODECS_H
#define CODECS_H
#include <stddef.h>
#include <stdint.h>

size_t zstd_max_compressed_size(size_t src_size);
size_t zstd_compress(void *dst, size_t dst_cap, const void *src, size_t src_sz, int level);
size_t zstd_decompress(void *dst, size_t dst_cap, const void *src, size_t src_sz);

#endif
