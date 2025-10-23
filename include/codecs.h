#ifndef WARPC_CODECS_H
#define WARPC_CODECS_H

#include <stddef.h>

typedef struct {
  const char* name;
  int    (*compress_bound)(size_t src_size, size_t* out_bound);
  size_t (*compress)(const void* src, size_t src_size, void* dst, size_t dst_cap, int level);
  size_t (*decompress)(const void* src, size_t src_size, void* dst, size_t dst_cap);
} codec_vtable;

const codec_vtable* warpc_get_codec_by_name(const char* name);
const codec_vtable* warpc_get_codec_by_id(int id);
int                 warpc_codec_id_from_name(const char* name);
const char*         warpc_codec_name_from_id(int id);

/* Defaults */
#define WARPC_DEFAULT_CHUNK_KIB 16384 /* 16 MiB */
#define WARPC_DEFAULT_LEVEL_ZSTD 3
#define WARPC_DEFAULT_LEVEL_LZ4  0

#endif

