typedef struct Loader
{
    struct Loader* (*Open)(struct Loader* loader, const char* filename);
    void (*Close)(struct Loader* loader);
    int (*Read)(struct Loader* loader, void* buf, size_t size);
    const char* (*GetName)(struct Loader* loader);
} Loader;
