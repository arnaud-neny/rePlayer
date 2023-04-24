#ifndef LOG_TABLE_H
#define LOG_TABLE_H

#include "../device/logtable.h"

typedef struct log_table_12_8_30_s {
  uint32_t lineartbl[257];
  uint32_t logtbl[4096];
} log_table_12_8_30_t;

PROTECTED_VAR /*const */LOG_TABLE log_table_12_8_30;

typedef struct log_table_12_7_30_s {
  uint32_t lineartbl[129];
  uint32_t logtbl[4096];
} log_table_12_7_30_t;

PROTECTED_VAR /*const */LOG_TABLE log_table_12_7_30;

#endif
