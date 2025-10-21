#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>

ssize_t wc_pread (int fd, void *buf, size_t len, uint64_t off);
ssize_t wc_pwrite(int fd, const void *buf, size_t len, uint64_t off);
int     wc_truncate_file(FILE *f, uint64_t size);

