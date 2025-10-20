cmake_minimum_required(VERSION 3.16)
project(warpcompress_c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Release)

# -------- Required: Zstd --------
find_path(ZSTD_INCLUDE_DIR zstd.h PATH_SUFFIXES include)
find_library(ZSTD_LIBRARY zstd)
if (NOT ZSTD_INCLUDE_DIR OR NOT ZSTD_LIBRARY)
  message(FATAL_ERROR "zstd not found. Install libzstd-dev (Linux) or 'brew install zstd' (macOS).")
endif()
include_directories(${ZSTD_INCLUDE_DIR})

# -------- Optional: LZ4 --------
find_path(LZ4_INCLUDE_DIR lz4.h PATH_SUFFIXES include)
find_library(LZ4_LIBRARY lz4)
if (LZ4_INCLUDE_DIR AND LZ4_LIBRARY)
  add_compile_definitions(HAVE_LZ4=1)
  include_directories(${LZ4_INCLUDE_DIR})
endif()

# -------- Optional: Snappy --------
find_path(SNAPPY_INCLUDE_DIR snappy-c.h PATH_SUFFIXES include)
find_library(SNAPPY_LIBRARY snappy)
if (SNAPPY_INCLUDE_DIR AND SNAPPY_LIBRARY)
  add_compile_definitions(HAVE_SNAPPY=1)
  include_directories(${SNAPPY_INCLUDE_DIR})
endif()

# -------- Optional: xxhash (checksum) --------
find_path(XXHASH_INCLUDE_DIR xxhash.h PATH_SUFFIXES include)
find_library(XXHASH_LIBRARY xxhash)
if (XXHASH_INCLUDE_DIR AND XXHASH_LIBRARY)
  add_compile_definitions(HAVE_XXHASH=1)
  include_directories(${XXHASH_INCLUDE_DIR})
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SRCS
  src/main.c
  src/threadpool.c
  src/codecs_zstd.c
  src/container.c
  src/util.c
)

if (LZ4_LIBRARY)
  list(APPEND SRCS src/codecs_lz4.c)
endif()
if (SNAPPY_LIBRARY)
  list(APPEND SRCS src/codecs_snappy.c)
endif()

add_executable(warpc ${SRCS})

target_link_libraries(warpc ${ZSTD_LIBRARY})
if (LZ4_LIBRARY)
  target_link_libraries(warpc ${LZ4_LIBRARY})
endif()
if (SNAPPY_LIBRARY)
  target_link_libraries(warpc ${SNAPPY_LIBRARY})
endif()
if (XXHASH_LIBRARY)
  target_link_libraries(warpc ${XXHASH_LIBRARY})
endif()

if(APPLE)
  target_compile_options(warpc PRIVATE -O3 -flto=thin -fstrict-aliasing -funroll-loops)
  target_link_options(warpc PRIVATE -flto=thin)
elseif(UNIX)
  target_compile_options(warpc PRIVATE -O3 -flto -fstrict-aliasing -funroll-loops)
  target_link_options(warpc PRIVATE -flto)
endif()
