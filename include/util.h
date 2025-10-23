#ifndef WARPC_UTIL_H
#define WARPC_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h> /* off_t */

int  read_all(int fd, void* buf, size_t n);
int  write_all(int fd, const void* buf, size_t n);
int  pread_all(int fd, void* buf, size_t n, off_t off);
int  pwrite_all(int fd, const void* buf, size_t n, off_t off);
int  file_stat_size(const char* path, uint64_t* out);
int  file_open_rd(const char* path);
int  file_open_wr(const char* path);
int  file_open_trunc(const char* path);

/* Fast 64-bit FNV-1a for --verify */
uint64_t fnv1a64_update(uint64_t h, const void* data, size_t n);
uint64_t fnv1a64_file(const char* path, size_t chunk);

#endif

