#ifndef OPL_TABLE_H
#define OPL_TABLE_H
#define SINTBL_BITS 10
#define AMTBL_BITS 8
#define PMTBL_BITS 8
#define PM_SHIFT 9
#define ARTBL_BITS 7
#define ARTBL_SHIFT 20
#define TLLTBL_BITS 8
typedef struct opl_table_s {
  uint32_t sin_table[4][1024];
  uint32_t tll2log_table[256];
  uint32_t ar_tablelog[128];
  uint32_t am_table1[256];
  uint32_t pm_table1[256];
  uint32_t ar_tablepow[128];
  uint32_t am_table2[256];
  uint32_t pm_table2[256];
} opl_table_t;

PROTECTED_VAR /*const */opl_table_t opl_table_i;

#endif
