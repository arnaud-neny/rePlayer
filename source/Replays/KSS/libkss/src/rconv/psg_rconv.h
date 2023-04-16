#ifndef _PSG_RCONV_H_
#define _PSG_RCONV_H_

#include "emu2149/emu2149.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct __PSG_RateConv {
    double timer;
    double f_ratio;
    int16_t *sinc_table;
    int16_t *buf;

    double f_out;
    double f_inp;
    double out_time;

    uint8_t quality;

    PSG *psg;
  } PSG_RateConv;

  PSG_RateConv *PSG_RateConv_new(void);
  void PSG_RateConv_reset(PSG_RateConv *conv, PSG * psg, uint32_t f_out);
  void PSG_RateConv_setQuality(PSG_RateConv *conv, uint8_t quality);
  int16_t PSG_RateConv_calc(PSG_RateConv *conv);
  void PSG_RateConv_delete(PSG_RateConv *conv);

#ifdef __cplusplus
}
#endif

#endif
