
// Path helpers for song and external file urls. '/' is a separator everywhere, '\' only on Windows. Urls with
// "://" are treated as absolute.

#pragma once

#include <stdbool.h>
#include <stddef.h>

// The part after the last separator ("dir/song.smus" -> "song.smus"). Points into `path`.
const char* nuade_path_basename(const char* path);

// Everything before the last separator ("a/b/song" -> "a/b", "/song" -> "/"). False, with `out` empty, when
// `path` has no separator or `out` is too small.
bool nuade_path_parent(const char* path, char* out, size_t out_cap);

// "dir" + "name" -> "dir/name"; no separator is added after an empty `dir` or one that already ends in one.
// False, with `out` empty, if `out` is too small.
bool nuade_path_join(const char* dir, const char* name, char* out, size_t out_cap);

// Resolve `relative` against the directory part of `base_url` ("dir/song.smus" + "Instruments/a.instr" ->
// "dir/Instruments/a.instr"). An absolute `relative` is copied as is. False, with `out` empty, if `out` is too
// small.
bool nuade_path_resolve(const char* base_url, const char* relative, char* out, size_t out_cap);

// Does the file name carry the extension `ext` (no dot, lowercase), either as a suffix ("song.smus") or as a
// modland-style prefix ("smus.song")? Case-insensitive.
bool nuade_path_has_ext(const char* path, const char* ext);
