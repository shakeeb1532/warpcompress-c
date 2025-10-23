cmake_minimum_required(VERSION 3.16)

project(warpc C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Large-file I/O (makes off_t 64-bit on 32-bit toolchains)
if (NOT MSVC)
  add_compile_definitions(_FILE_OFFSET_BITS=64)
endif()

# Sources — pick up all .c in src/
file(GLOB WARPC_SOURCES CONFIGURE_DEPENDS
  "src/*.c"
)

add_executable(warpc ${WARPC_SOURCES})

# Zstd (required for now; you can make it optional later)
find_path(ZSTD_INCLUDE_DIR zstd.h)
find_library(ZSTD_LIBRARY NAMES zstd)

if (NOT ZSTD_INCLUDE_DIR OR NOT ZSTD_LIBRARY)
  message(FATAL_ERROR "libzstd not found.\n"
    "Ubuntu: sudo apt-get update && sudo apt-get install -y libzstd-dev\n"
    "macOS : brew install zstd")
else()
  target_include_directories(warpc PRIVATE ${ZSTD_INCLUDE_DIR})
  target_link_libraries(warpc PRIVATE ${ZSTD_LIBRARY})
  # Enable real zstd code paths in codecs_zstd.c
  target_compile_definitions(warpc PRIVATE HAVE_ZSTD=1)
endif()

# Threads (Linux needs explicit pthreads, macOS links via libSystem)
if(UNIX AND NOT APPLE)
  find_package(Threads REQUIRED)
  target_link_libraries(warpc PRIVATE Threads::Threads)
endif()

# Warnings
if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(warpc PRIVATE -Wall -Wextra -Wconversion -Wpointer-arith -Wshadow -Wformat=2)
endif()

# Install target (optional)
install(TARGETS warpc RUNTIME DESTINATION bin)

