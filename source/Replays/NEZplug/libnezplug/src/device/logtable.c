#ifndef LOGTABLE_C
#define LOGTABLE_C

#include "../normalize.h"
#include "logtable.h"

PROTECTED uint32_t LinearToLog(LOG_TABLE *tbl,int32_t l)
{
	return (l < 0) ? (tbl->lineartbl[-l] + 1) : tbl->lineartbl[l];
}

PROTECTED int32_t LogToLinear(LOG_TABLE *tbl, uint32_t l, uint32_t sft)
{
	int32_t ret;
	uint32_t ofs;
	l += sft << (tbl->log_bits + 1);
	sft = l >> (tbl->log_bits + 1);
	if (sft >= tbl->log_lin_bits) return 0;
	ofs = (l >> 1) & ((1 << tbl->log_bits) - 1);
	ret = tbl->logtbl[ofs] >> sft;
	return (l & 1) ? -ret : ret;
}

#endif
