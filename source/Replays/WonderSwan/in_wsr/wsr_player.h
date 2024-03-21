#ifndef INC_WSR_PLAYER_H
#define INC_WSR_PLAYER_H


//extern int SampleRate;
//extern uint8_t *ROM;
//extern int ROMSize;
//extern int ROMBank;


void Update_SampleData(void);
void ws_timer_reset(void);
void ws_timer_count(int Cycles);
void ws_timer_set(int no, int timer);
void ws_timer_update(void);
int ws_timer_min(int Cycles);

#endif