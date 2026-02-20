//=============================================================================
//		PCM Music Driver Interface Class : IPCMMUSICDRIVER
//=============================================================================

#ifndef PCMMUSICDRIVER_H
#define PCMMUSICDRIVER_H

#include "comsupport.h"


//=============================================================================
// IPCMMUSICDRIVER : 音源ドライバの基本的なインターフェイスを定義したクラス
//=============================================================================

struct IPCMMUSICDRIVER : public IUnknown {
	virtual bool WINAPI init(TCHAR *path) = 0;
	virtual int WINAPI music_load(TCHAR *filename) = 0;
	virtual int WINAPI music_load2(uint8_t *musdata, int size) = 0;
	virtual TCHAR* WINAPI getmusicfilename(TCHAR *dest) = 0;
	virtual void WINAPI music_start(void) = 0;
	virtual void WINAPI music_stop(void) = 0;
	virtual int WINAPI getloopcount(void) = 0;
	virtual bool WINAPI getlength(TCHAR *filename, int *length, int *loop) = 0;
	virtual int WINAPI getpos(void) = 0;
	virtual void WINAPI setpos(int pos) = 0;
	virtual int WINAPI getpcmdata(int16_t *buf, int nsamples) = 0;
};


//=============================================================================
// IFMPMD : WinFMP, PMDWin に共通なインターフェイスを定義したクラス
//=============================================================================
struct IFMPMD : public IPCMMUSICDRIVER {
	virtual bool WINAPI loadrhythmsample(TCHAR *path) = 0;
	virtual bool WINAPI setpcmdir(TCHAR **path) = 0;
	virtual void WINAPI setpcmrate(int rate) = 0;
	virtual void WINAPI setppzrate(int rate) = 0;
	virtual void WINAPI setfmcalc55k(bool flag) = 0;
	virtual void WINAPI setppzinterpolation(bool ip) = 0;
	virtual void WINAPI setfmwait(int nsec) = 0;
	virtual void WINAPI setssgwait(int nsec) = 0;
	virtual void WINAPI setrhythmwait(int nsec) = 0;
	virtual void WINAPI setadpcmwait(int nsec) = 0;
	virtual void WINAPI fadeout(int speed) = 0;
	virtual void WINAPI fadeout2(int speed) = 0;
	virtual bool WINAPI getlength2(TCHAR *filename, int *length, int *loop) = 0;
	virtual int WINAPI getpos2(void) = 0;
	virtual void WINAPI setpos2(int pos) = 0;
	virtual TCHAR* WINAPI getpcmfilename(TCHAR *dest) = 0;
	virtual TCHAR* WINAPI getppzfilename(TCHAR *dest, int bufnum) = 0;
};


#endif	// PCMMUSICDRIVER_H
