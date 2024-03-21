
#ifndef __AUDIO_H__
#define __AUDIO_H__

void ws_audio_init(void);
void ws_audio_reset(void);
void ws_audio_done(void);
void ws_audio_update(short *buffer, int length);
void ws_audio_port_write(uint8_t port,uint8_t value);
uint8_t ws_audio_port_read(uint8_t port);
void ws_audio_process(void);
void ws_audio_sounddma(void);
extern unsigned long WaveAdrs;

#endif
