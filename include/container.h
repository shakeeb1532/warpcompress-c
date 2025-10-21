#ifndef CONTAINER_H
#define CONTAINER_H

#ifdef __cplusplus
extern "C" {
#endif

/* This header is now a thin wrapper.
   It MUST NOT redefine macros, enums, or structs already in warp.h. */
#include "warp.h"

/* Provide a declaration for the default options helper.
   The implementation is in src/util.c */
warp_opts_t warp_opts_default(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CONTAINER_H */
