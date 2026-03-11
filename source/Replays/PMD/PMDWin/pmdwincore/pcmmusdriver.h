//=============================================================================
//		PCM Music Driver Interface Class : IPCMMUSICDRIVER
//=============================================================================

#ifndef PCMMUSICDRIVER_H
#define PCMMUSICDRIVER_H

#include "comsupport.h"


//=============================================================================
// IPCMMUSICDRIVER : 音源ドライバの基本的なインターフェイスを定義したクラス
//=============================================================================

struct IPCMMUSICDRIVER : public pmd_IUnknown {
	virtual bool init(TCHAR *path) = 0;
	virtual int music_load(TCHAR *filename) = 0;
	virtual int music_load2(uint8_t *musdata, int size) = 0;
	virtual TCHAR* getmusicfilename(TCHAR *dest) = 0;
	virtual void music_start(void) = 0;
	virtual void music_stop(void) = 0;
	virtual int getloopcount(void) = 0;
	virtual bool getlength(TCHAR *filename, int *length, int *loop) = 0;
	virtual int getpos(void) = 0;
	virtual void setpos(int pos) = 0;
	virtual int getpcmdata(int16_t *buf, int nsamples) = 0;
};


//=============================================================================
// IFMPMD : WinFMP, PMDWin に共通なインターフェイスを定義したクラス
//=============================================================================
struct IFMPMD : public IPCMMUSICDRIVER {
	virtual bool loadrhythmsample(TCHAR *path) = 0;
	virtual bool setpcmdir(TCHAR **path) = 0;
	virtual void setpcmrate(int rate) = 0;
	virtual void setppzrate(int rate) = 0;
	virtual void setfmcalc55k(bool flag) = 0;
	virtual void setppzinterpolation(bool ip) = 0;
	virtual void setfmwait(int nsec) = 0;
	virtual void setssgwait(int nsec) = 0;
	virtual void setrhythmwait(int nsec) = 0;
	virtual void setadpcmwait(int nsec) = 0;
	virtual void fadeout(int speed) = 0;
	virtual void fadeout2(int speed) = 0;
	virtual bool getlength2(TCHAR *filename, int *length, int *loop) = 0;
	virtual int getpos2(void) = 0;
	virtual void setpos2(int pos) = 0;
	virtual TCHAR* getpcmfilename(TCHAR *dest) = 0;
	virtual TCHAR* getppzfilename(TCHAR *dest, int bufnum) = 0;
};


#endif	// PCMMUSICDRIVER_H
