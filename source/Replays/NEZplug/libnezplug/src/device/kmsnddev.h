/* libnezp by Mamiya */

#ifndef KMSNDDEV_H__
#define KMSNDDEV_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "../normalize.h"

typedef struct {
	void *ctx;
	void (*release)(void *ctx);
	void (*reset)(void *ctx, uint32_t clock, uint32_t freq);
	void (*synth)(void *ctx, int32_t *p);
	void (*volume)(void *ctx, int32_t v);
	void (*write)(void *ctx, uint32_t a, uint32_t v);
	uint32_t (*read)(void *ctx, uint32_t a);
	void (*setinst)(void *ctx, uint32_t n, void *p, uint32_t l);
} KMIF_SOUND_DEVICE;

#ifdef __cplusplus
}
#endif
#endif /* KMSNDDEV_H__ */


