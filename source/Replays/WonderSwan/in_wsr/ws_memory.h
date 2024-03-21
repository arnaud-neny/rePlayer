
#ifndef __MEMORY_H__
#define __MEMORY_H__

extern uint8_t	*ws_staticRam;
extern uint8_t	*ws_internalRam;

void	ws_memory_init(uint8_t *rom, uint32_t romSize);
void	ws_memory_reset(void);
void	ws_memory_done(void);

#endif
