#ifndef _UADE_UNIXSUPPORT_H_
#define _UADE_UNIXSUPPORT_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <uade/uadeipc.h>

#if 0
#define DebugPrint(fmt, ...) { char txt[256]; sprintf(txt, fmt, ## __VA_ARGS__); OutputDebugStringA(txt); }

#define uade_debug(state, fmt, ...) do { if ((state) == NULL || uade_is_verbose(state)) { DebugPrint(fmt, ## __VA_ARGS__); } } while (0)
#define uade_die(fmt, ...) do { DebugPrint("uade: " fmt, ## __VA_ARGS__); assert(0); } while(0)
#define uade_die_error(fmt, ...) do { DebugPrint("uade: " fmt ": %s\n", ## __VA_ARGS__, strerror(errno)); assert(0); } while(0)
#define uade_error(fmt, ...) do { \
    DebugPrint("%s:%d: %s: " fmt, __FILE__, __LINE__, __func__, ## __VA_ARGS__); \
    assert(0); \
  } while (0)
#define uade_info(fmt, ...) do { DebugPrint("uade info: " fmt, ## __VA_ARGS__); } while(0)
#define uade_warning(fmt, ...) do { DebugPrint("uade warning: " fmt, ## __VA_ARGS__); } while(0)
#else
#define DebugPrint(fmt, ...)

#define uade_debug(state, fmt, ...)
#define uade_die(fmt, ...)
#define uade_die_error(fmt, ...)
#define uade_error(fmt, ...)
#define uade_info(fmt, ...)
#define uade_warning(fmt, ...)
#endif


char* uade_dirname(char* dst, char* src, size_t maxlen);
int uade_find_amiga_file(char* realname, size_t maxlen, const char* aname, const char* playerdir);

void uade_arch_kill_and_wait_uadecore(struct uade_ipc* ipc, /*pid_t * uadepid*/void** userdata);
int uade_arch_spawn(struct uade_ipc* ipc, /*pid_t * uadepid*/void** userdata, const char* uadename);

int uade_filesize(size_t * size, const char* pathname);

FILE* uade_fopen(const char*, const char*);
#define fopen(a,b) uade_fopen(a, b)
int uade_fseek(FILE*, int, int);
#define fseek(a,b,c) uade_fseek(a, b, c)
int uade_fread(char*, int, int, FILE*);
#define fread(a,b,c,d) uade_fread(a, b, c, d)
int uade_fwrite(const char*, int, int, FILE*);
#define fwrite(a,b,c,d) uade_fwrite(a, b, c, d)
int uade_ftell(FILE*);
#define ftell(a) uade_ftell(a)
int uade_fclose(FILE*);
#define fclose(a) uade_fclose(a)
char* uade_fgets(char*, int, FILE*);
#define fgets(a, b, c) uade_fgets(a, b, c)
int uade_feof(FILE*);
#define feof(a) uade_feof(a)

#endif
