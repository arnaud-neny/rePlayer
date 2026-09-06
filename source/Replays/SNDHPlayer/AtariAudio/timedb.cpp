/*--------------------------------------------------------------------
	Atari Audio Library v1.08
	Small & accurate ATARI-ST audio emulation
	Arnaud Carré aka Leonard/Oxygene
	@leonard_coder
--------------------------------------------------------------------*/
#include "timedb.h"

struct TimeDbEntry
{
	uint32_t hash;
	uint32_t frames;
};

#define TIMEDB_ENTRY(HASH,SONGID,FRAMES,FLAGS) { 0x##HASH, FRAMES }
static const TimeDbEntry sDatabase[] =
{
	// timedb.inc.h database by Benjamin Gerard & SNDH Community
	#include "external/timedb.inc.h"
};

static uint32_t hashInternal(const uint8_t* d, uint32_t size, uint32_t hashIn)
{
	uint32_t hash = hashIn;
	for (uint32_t i=0;i<size;i++)
	{
		hash += *d++;
		hash += hash << 10;
		hash ^= hash >> 6;
	}
	return hash;
}

static uint32_t sc68Hash(const void* data, uint32_t size)
{
	if (size < 32)
		return 0;

	// weird hash calculation due to original sc68 parsing 32bytes header, seek back to 0, and read complete file
	uint32_t hash = hashInternal((const uint8_t *)data, 32, 0);
	return hashInternal((const uint8_t *)data, size, hash);
}

int timedbSearch(const void* data, uint32_t size, uint32_t* framesArray, int framesArraySize)
{
	const uint32_t hash = sc68Hash(data, size);
	static const int kDatabaseLen = sizeof(sDatabase) / sizeof(sDatabase[0]);
	for (int i = 0; i < kDatabaseLen; i++)
	{
		if (sDatabase[i].hash == hash)
		{
			int songCount = 0;
			// now we walk all subsongs
			while ((i < kDatabaseLen) && (sDatabase[i].hash == hash))
			{
				framesArray[songCount] = sDatabase[i].frames;
				songCount++;
				i++;
				if (songCount >= framesArraySize)
					break;
			}
			return songCount;
		}
		else if (hash < sDatabase[i].hash)
			break;
	}
	return 0;
}
