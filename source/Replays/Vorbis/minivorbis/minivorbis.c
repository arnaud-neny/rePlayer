extern void* myAlloc(size_t);
extern void* myCalloc(size_t);
extern void* myRealloc(void*, size_t);
extern void myFree(void*);

#define OGG_IMPL
#define VORBIS_IMPL
#define _CRT_SECURE_NO_WARNINGS
#include "minivorbis.h"