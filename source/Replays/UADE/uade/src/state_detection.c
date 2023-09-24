/*
 * A work-in-progress state detection module. Disabled by default, unless
 * amigasrc/score/config.i: "UADE_STATE_DETECTION equ 1" is set.
 */
#include "sysconfig.h"
#include "sysdeps.h"
#include "memory.h"
#include "state_detection.h"

#include "frontends/common/md5.h"
#include "frontends/common/md5.c"

#include <string.h>

#include "zakalwe/array.h"
#include "zakalwe/map.h"


#define NUM_MEMORY_BYTES (2 << 20)

static int binary_cmp(const void *x, const void *y)
{
        return memcmp(x, y, 16);
}

Z_MAP_PROTOTYPE(binary_key_map, void *, int, binary_cmp)
Z_ARRAY_PROTOTYPE(index_array, unsigned int)

static struct binary_key_map *m_state_map;
static struct index_array m_index_array;

struct digest {
	unsigned char digest[16];
};

static uae_u32 m_addr;
static uae_u32 m_size;
static int m_frame;
static unsigned char *m_memory;
static unsigned int *m_memory_mod_counter;
static struct digest last_digest;

static void print_digest(const unsigned char digest[16])
{
	size_t i;
	for (i = 0; i < 16; i++) {
		fprintf(stderr, "%.2x ", digest[i]);
	}
}

void uade_state_detection_init(const uae_u32 addr, const uae_u32 size)
{
	if (!valid_address(addr, size)) {
		fprintf(stderr, "uadecore: state_detection invalid address "
			"range 0x%x 0x%x\n", addr, size);
		return;
	}
	m_addr = addr;
	m_size = size;

	fprintf(stderr, "TODO: Free all keys of m_state_map(), or add a "
		"helper into libzakalwe.");
	binary_key_map_free(m_state_map);

	m_state_map = binary_key_map_create();

	m_memory = z_malloc_or_die(m_size);
	memcpy(m_memory, get_real_address(m_addr), m_size);

	m_memory_mod_counter = z_calloc_or_die(m_size,
					       sizeof(m_memory_mod_counter[0]));
	m_frame = -1;
	index_array_init(&m_index_array);

	last_digest = (struct digest) {};
}

int uade_state_detection_step(void)
{
	unsigned char *addr = get_real_address(m_addr);
	uade_MD5_CTX ctx;
	struct digest digest;

	m_frame++;

	fprintf(stderr, "t %f\n", m_frame / 50.0);

	for (unsigned int i = 0; i < m_size; i++) {
		if (addr[i] != m_memory[i]) {
			//printf("change %x: %x -> %x\n", m_addr + i, m_memory[i],
			//       addr[i]);
			m_memory[i] = addr[i];
			m_memory_mod_counter[i]++;
		}
	}

	if (m_frame == 50) {
		memset(m_memory_mod_counter, 0,
		       sizeof(m_memory_mod_counter[0]) * m_size);
	} else if (m_frame == 1050) {
		for (unsigned int i = 0; i < m_size; i++) {
			if (m_memory_mod_counter[i] >= 2 &&
			    m_memory_mod_counter[i] < 200) {
				fprintf(stderr, "%x changed %u times\n",
					m_addr + i, m_memory_mod_counter[i]);
				z_assert(index_array_append(&m_index_array, i));
			}
		}
		fprintf(stderr, "ind len %zu\n",
			index_array_len(&m_index_array));
	}

	if (m_frame >= 1050) {
		struct index_array_iter iter;

		uade_MD5Init(&ctx);

		z_array_for_each(index_array, &m_index_array, &iter)
			uade_MD5Update(&ctx, &addr[iter.value], 1);

		// uade_MD5Update(&ctx, addr, m_size);
		uade_MD5Final(digest.digest, &ctx);

		// Only lookup digest if it has changed
		int digest_changed = (memcmp(digest.digest, last_digest.digest,
					     sizeof(digest.digest)) != 0);
		if (digest_changed &&
		    binary_key_map_has(m_state_map, digest.digest)) {
			fprintf(stderr, "state detection end: ");
			print_digest(digest.digest);
			fprintf(stderr, "\n");
			return 1;
			//return 1;
		}
		last_digest = digest;
	}

	void *key = z_malloc_or_die(sizeof(digest.digest));
	memcpy(key, digest.digest, sizeof(digest.digest));
	z_assert(binary_key_map_set(m_state_map, key, 1));

	return 0;
}
