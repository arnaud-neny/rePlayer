
// nuade IO API: every file a player reads (the song by URL, external sample and instrument files, instrument
// directories) goes through a NuadeIo the host can replace, so songs can come from zips, archives, the network or
// memory.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUADE_IO_API_VERSION 1

// Call a NuadeIo entry with its private data.
#define NuadeIo_exists(io, url) (io)->exists((io)->private_data, url)
#define NuadeIo_read_url_to_memory(io, url) (io)->read_url_to_memory((io)->private_data, url)
#define NuadeIo_free_url_to_memory(io, memory) (io)->free_url_to_memory((io)->private_data, memory)
#define NuadeIo_list_dir(io, url, visit, ctx) (io)->list_dir((io)->private_data, url, visit, ctx)

typedef struct NuadeIoReadResult {
    uint8_t* data; // 0 on failure
    uint64_t size;
} NuadeIoReadResult;

// Called once per directory entry by list_dir. `name` is the entry name only, not a path.
typedef void (*NuadeIoDirVisitor)(const char* name, bool is_dir, void* ctx);

struct NuadeIoPrivate;

typedef struct NuadeIo {
    uint64_t api_version; // NUADE_IO_API_VERSION
    struct NuadeIoPrivate* private_data;
    // Does the url exist?
    bool (*exists)(struct NuadeIoPrivate* self, const char* url);
    // Load the whole file. The caller owns the memory until it passes it to free_url_to_memory.
    NuadeIoReadResult (*read_url_to_memory)(struct NuadeIoPrivate* self, const char* url);
    // Free memory returned by read_url_to_memory (only that).
    void (*free_url_to_memory)(struct NuadeIoPrivate* self, void* memory);
    // Call visit for each entry of a directory; false if it cannot be listed. May be 0 when the backend cannot
    // list at all; players then only try exact names.
    bool (*list_dir)(struct NuadeIoPrivate* self, const char* url, NuadeIoDirVisitor visit, void* ctx);
} NuadeIo;

// Local filesystem. Static, never freed.
const NuadeIo* nuade_io_default(void);

#ifdef __cplusplus
}
#endif
