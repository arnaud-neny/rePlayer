
// Case-insensitive lookup of external files (instruments, samples) in a list of directories, built once from
// NuadeIo directory listings. Songs name their external files without regard to case, so exact names often miss.

#pragma once

#include <nuade/io.h>

typedef struct NuadeFileIndex NuadeFileIndex;

// Index the files of `dirs`, searched in order: when a name occurs in several directories (in any case), the
// first one wins. Dot-files are skipped. Directories that cannot be listed add nothing. An io without list_dir
// still resolves names, but only by their exact spelling (probed with `exists`). 0 when out of memory.
NuadeFileIndex* nuade_file_index_create(const NuadeIo* io, const char* const* dirs, size_t dir_count);

void nuade_file_index_destroy(NuadeFileIndex* index);

// Full url of the file called `name` (any case; exact case without list_dir), or 0. Valid until the index is destroyed.
const char* nuade_file_index_find(NuadeFileIndex* index, const char* name);
