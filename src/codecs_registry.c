#include "warpc/codecs.h"
#include <string.h>

#define MAX_CODECS 8
static const codec_vtable* g_codecs[MAX_CODECS];
static int g_codec_ids[MAX_CODECS];
static int g_codec_count = 0;

const codec_vtable* __warpc_register_codec(const codec_vtable* vt, int id) {
  if (g_codec_count < MAX_CODECS) {
    g_codecs[g_codec_count] = vt;
    g_codec_ids[g_codec_count] = id;
    g_codec_count++;
  }
  return vt;
}

const codec_vtable* warpc_get_codec_by_name(const char* name) {
  for (int i = 0; i < g_codec_count; ++i) {
    if (g_codecs[i] && strcmp(g_codecs[i]->name, name) == 0) return g_codecs[i];
  }
  return NULL;
}

const codec_vtable* warpc_get_codec_by_id(int id) {
  for (int i = 0; i < g_codec_count; ++i) {
    if (g_codecs[i] && g_codec_ids[i] == id) return g_codecs[i];
  }
  return NULL;
}

int warpc_codec_id_from_name(const char* name) {
  const codec_vtable* vt = warpc_get_codec_by_name(name);
  if (!vt) return 0;
  for (int i = 0; i < g_codec_count; ++i)
    if (g_codecs[i] == vt) return g_codec_ids[i];
  return 0;
}

const char* warpc_codec_name_from_id(int id) {
  const codec_vtable* vt = warpc_get_codec_by_id(id);
  return vt ? vt->name : "unknown";
}
