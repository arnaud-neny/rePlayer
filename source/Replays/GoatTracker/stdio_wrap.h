#ifndef STDIO_WRAP_H
#define STDIO_WRAP_H

#include <stddef.h>
typedef void FILE;

FILE* replayer_fopen(const char* filename, const char* mode);
int replayer_fread(void* buffer, size_t size, size_t count, FILE* stream);
int replayer_fwrite(void* buffer, size_t size, size_t count, FILE* stream);
int replayer_fseek(FILE* stream, long offset, int origin);
void replayer_fclose(FILE* stream);
int replayer_fprintf(void* const, char const* const, ...);

#define SEEK_SET (0)
#define SEEK_CUR (1)
#define SEEK_END (2)

#define fopen(...) replayer_fopen(__VA_ARGS__)
#define fread(...) replayer_fread(__VA_ARGS__)
#define fwrite(...)	replayer_fwrite(__VA_ARGS__)
#define fseek(...) replayer_fseek(__VA_ARGS__)
#define fclose(...) replayer_fclose(__VA_ARGS__)

#define fprintf(...) replayer_fprintf(__VA_ARGS__)
#define stdin  (0)
#define stdout (1)
#define stderr (2)
#define EOF (-1)
#endif
