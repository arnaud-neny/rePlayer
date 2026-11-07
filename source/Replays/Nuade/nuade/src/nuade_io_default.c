// NuadeIo backend for local files. The only file-system code in the framework and the players.

#include <nuade/io.h>

#include "nuade_base.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(PLATFORM_WINDOWS)
#include <io.h>
#else
#include <dirent.h>
#endif

static bool default_exists(struct NuadeIoPrivate* self, const char* url) {
    (void)self;
    struct stat st;
    return url != nullptr && stat(url, &st) == 0;
}

static NuadeIoReadResult default_read(struct NuadeIoPrivate* self, const char* url) {
    (void)self;
    NuadeIoReadResult result = { 0 };
    if (url == nullptr) {
        return result;
    }

    FILE* f = fopen(url, "rb");
    if (f == nullptr) {
        return result;
    }

    long size = -1;
    if (fseek(f, 0, SEEK_END) == 0) {
        size = ftell(f);
    }
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return result;
    }

    // One spare byte so an empty file still gets a non-null buffer.
    u8* data = (u8*)malloc((size_t)size + 1);
    if (data == nullptr) {
        fclose(f);
        return result;
    }

    const size_t read_count = fread(data, 1, (size_t)size, f);
    fclose(f);
    if (read_count != (size_t)size) {
        free(data);
        return result;
    }

    result.data = data;
    result.size = (u64)size;
    return result;
}

static void default_free(struct NuadeIoPrivate* self, void* memory) {
    (void)self;
    free(memory);
}

static bool is_dot_entry(const char* name) {
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

static bool default_list_dir(struct NuadeIoPrivate* self, const char* url, NuadeIoDirVisitor visit, void* ctx) {
    (void)self;
    if (url == nullptr || visit == nullptr) {
        return false;
    }

#if defined(PLATFORM_WINDOWS)
    char pattern[1024];
    const int n = snprintf(pattern, sizeof(pattern), "%s\\*", url);
    if (n < 0 || (size_t)n >= sizeof(pattern)) {
        return false;
    }
    struct _finddata_t fd;
    const intptr_t handle = _findfirst(pattern, &fd);
    if (handle == -1) {
        return false;
    }
    do {
        if (!is_dot_entry(fd.name)) {
            visit(fd.name, (fd.attrib & _A_SUBDIR) != 0, ctx);
        }
    } while (_findnext(handle, &fd) == 0);
    _findclose(handle);
    return true;
#else
    DIR* dir = opendir(url);
    if (dir == nullptr) {
        return false;
    }
    const struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (is_dot_entry(entry->d_name)) {
            continue;
        }
        // stat, not dirent's d_type: d_type is not POSIX
        char path[1024];
        struct stat st;
        const int n = snprintf(path, sizeof(path), "%s/%s", url, entry->d_name);
        const bool is_dir = n > 0 && (size_t)n < sizeof(path) && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
        visit(entry->d_name, is_dir, ctx);
    }
    closedir(dir);
    return true;
#endif
}

static const NuadeIo s_default_io = {
    .api_version = NUADE_IO_API_VERSION,
    .private_data = nullptr,
    .exists = default_exists,
    .read_url_to_memory = default_read,
    .free_url_to_memory = default_free,
    .list_dir = default_list_dir,
};

const NuadeIo* nuade_io_default(void) {
    return &s_default_io;
}
