#ifndef STDIO_WRAP_H
#define STDIO_WRAP_H

#include <stddef.h>
#include <ImGui/stb_sprintf.h>
typedef struct {
    char* buf;
    int pos;
    int size;
} mem_out;
typedef mem_out FILE;

FILE* replayer_fopen(const char* filename, const char* mode);
int replayer_fread(void* buffer, size_t size, size_t count, FILE* stream);
int replayer_fwrite(void* buffer, size_t size, size_t count, FILE* stream);
int replayer_fseek(FILE* stream, long offset, int origin);
int replayer_fclose(FILE* stream);
int replayer_ftell(FILE* stream);
int replayer_fputc(unsigned char c, FILE* stream);
int replayer_fprintf(void* const, char const* const, ...);
int replayer_fgetc(FILE* stream);

#define SEEK_SET (0)
#define SEEK_CUR (1)
#define SEEK_END (2)

#define fopen(...) replayer_fopen(__VA_ARGS__)
#define fread(...) replayer_fread(__VA_ARGS__)
#define fwrite(...)	replayer_fwrite(__VA_ARGS__)
#define fseek(...) replayer_fseek(__VA_ARGS__)
#define fclose(...) replayer_fclose(__VA_ARGS__)
#define ftell(...) replayer_ftell(__VA_ARGS__)
#define fputc(...) replayer_fputc(__VA_ARGS__)
#define fgetc(...) replayer_fgetc(__VA_ARGS__)
#define fflush(...)
#define ferror(...) 0
#define feof(...) 0

#define fprintf(...) replayer_fprintf(__VA_ARGS__)
#define snprintf(...) stbsp_snprintf(__VA_ARGS__)
#define vsnprintf(...) stbsp_vsnprintf(__VA_ARGS__)
#define stdin  (0)
#define stdout (1)
#define stderr (2)
#define EOF (-1)
#define BUFSIZ  512

#endif
