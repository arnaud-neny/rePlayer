// Case-insensitive lookup of external files over NuadeIo directory listings.

#include "nuade_file_index.h"

#include "nuade_base.h"
#include "nuade_path.h"
#include "nuade_utils.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct NuadeFileEntry {
    char* key; // lowercase file name
    char* url;
} NuadeFileEntry;

struct NuadeFileIndex {
    NuadeFileEntry* entries;
    size_t count;
    size_t capacity;
    bool out_of_memory;
    // Set when the io cannot list directories: lookups then probe `dirs` for the exact name.
    const NuadeIo* probe_io;
    char** dirs;
    size_t dir_count;
};

typedef struct ListCtx {
    NuadeFileIndex* index;
    const char* dir;
} ListCtx;

static bool equals_nocase(const char* lower_key, const char* name) {
    for (; *lower_key != '\0'; lower_key++, name++) {
        if (tolower((unsigned char)*name) != *lower_key) {
            return false;
        }
    }
    return *name == '\0';
}

static bool add_entry(NuadeFileIndex* index, const char* dir, const char* name) {
    if (index->count == index->capacity) {
        const size_t capacity = index->capacity ? index->capacity * 2 : 256;
        NuadeFileEntry* entries = (NuadeFileEntry*)realloc(index->entries, capacity * sizeof(*entries));
        if (entries == nullptr) {
            return false;
        }
        index->entries = entries;
        index->capacity = capacity;
    }

    const size_t url_cap = strlen(dir) + strlen(name) + 2;
    NuadeFileEntry entry = { nuade_strdup(name), (char*)malloc(url_cap) };
    if (entry.key == nullptr || entry.url == nullptr) {
        free(entry.key);
        free(entry.url);
        return false;
    }
    // Probed entries keep the spelling that was found: other cases may name different files.
    for (char* c = entry.key; index->probe_io == nullptr && *c != '\0'; c++) {
        *c = (char)tolower((unsigned char)*c);
    }
    nuade_path_join(dir, name, entry.url, url_cap);
    index->entries[index->count++] = entry;
    return true;
}

static const char* find_cached(const NuadeFileIndex* index, const char* name);

static void visit_entry(const char* name, bool is_dir, void* ctx) {
    ListCtx* list = (ListCtx*)ctx;
    if (is_dir || name[0] == '.' || list->index->out_of_memory || find_cached(list->index, name) != nullptr) {
        return;
    }
    if (!add_entry(list->index, list->dir, name)) {
        list->index->out_of_memory = true;
    }
}

NuadeFileIndex* nuade_file_index_create(const NuadeIo* io, const char* const* dirs, size_t dir_count) {
    NuadeFileIndex* index = (NuadeFileIndex*)calloc(1, sizeof(NuadeFileIndex));
    if (index == nullptr || io == nullptr) {
        return index;
    }
    if (io->list_dir == nullptr) {
        if (io->exists == nullptr || dir_count == 0) {
            return index;
        }
        index->dirs = (char**)calloc(dir_count, sizeof(char*));
        if (index->dirs == nullptr) {
            free(index);
            return nullptr;
        }
        index->probe_io = io;
        index->dir_count = dir_count;
        for (size_t i = 0; i < dir_count; i++) {
            index->dirs[i] = nuade_strdup(dirs[i]);
            if (index->dirs[i] == nullptr) {
                nuade_file_index_destroy(index);
                return nullptr;
            }
        }
        return index;
    }
    for (size_t i = 0; i < dir_count; i++) {
        ListCtx ctx = { index, dirs[i] };
        NuadeIo_list_dir(io, dirs[i], visit_entry, &ctx);
    }
    if (index->out_of_memory) {
        nuade_file_index_destroy(index);
        return nullptr;
    }
    return index;
}

void nuade_file_index_destroy(NuadeFileIndex* index) {
    if (index == nullptr) {
        return;
    }
    for (size_t i = 0; i < index->count; i++) {
        free(index->entries[i].key);
        free(index->entries[i].url);
    }
    for (size_t i = 0; i < index->dir_count; i++) {
        free(index->dirs[i]);
    }
    free(index->dirs);
    free(index->entries);
    free(index);
}

static const char* find_cached(const NuadeFileIndex* index, const char* name) {
    for (size_t i = 0; i < index->count; i++) {
        const bool match = index->probe_io != nullptr ? strcmp(index->entries[i].key, name) == 0
                                                      : equals_nocase(index->entries[i].key, name);
        if (match) {
            return index->entries[i].url;
        }
    }
    return nullptr;
}

// Without directory listings only the exact name can be tried; a hit is cached so the url stays valid.
static const char* probe_exact(NuadeFileIndex* index, const char* name) {
    for (size_t i = 0; i < index->dir_count; i++) {
        char url[4096];
        nuade_path_join(index->dirs[i], name, url, sizeof(url));
        if (!NuadeIo_exists(index->probe_io, url)) {
            continue;
        }
        if (!add_entry(index, index->dirs[i], name)) {
            return nullptr;
        }
        return index->entries[index->count - 1].url;
    }
    return nullptr;
}

const char* nuade_file_index_find(NuadeFileIndex* index, const char* name) {
    if (index == nullptr || name == nullptr || name[0] == '\0') {
        return nullptr;
    }
    const char* cached = find_cached(index, name);
    if (cached != nullptr || index->probe_io == nullptr) {
        return cached;
    }
    return probe_exact(index, name);
}
