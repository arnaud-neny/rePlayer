#ifndef LOGTABLE_H_
#define LOGTABLE_H_

#include "../normalize.h"

typedef struct LOG_TABLE_ LOG_TABLE;
struct LOG_TABLE_ {
    uint32_t log_bits;
    uint32_t lin_bits;
    uint32_t log_lin_bits;
    const uint32_t *lineartbl;
    const uint32_t *logtbl;
};

PROTECTED int32_t LogToLinear(LOG_TABLE *tbl, uint32_t l, uint32_t sft);
PROTECTED uint32_t LinearToLog(LOG_TABLE *tbl, int32_t l);

#endif
