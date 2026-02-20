//=============================================================================
//		opnaw : OPNA unit with wait
//			ver 0.05
//=============================================================================

#ifndef OPNAW_H
#define OPNAW_H

#include "portability_fmpmdcore.h"
#include "opna.h"
#include "ifileio.h"


// 一次補間有効時の合成周波数
#define	SOUND_55K				55555
#define	SOUND_55K_2				55466

// wait で計算した分を代入する buffer size(samples)
#define	WAIT_PCM_BUFFER_SIZE	65536

// 線形補間時に計算した分を代入する buffer size(samples)
#define		IP_PCM_BUFFER_SIZE	 2048

// sinc 補間のサンプル数
// #define		NUMOFINTERPOLATION	   32
// #define		NUMOFINTERPOLATION	   64
#define		NUMOFINTERPOLATION	  128
// #define		NUMOFINTERPOLATION	  256
// #define		NUMOFINTERPOLATION	  512


namespace FM {
	
	class OPNAW : public OPNA
	{
	public:
		OPNAW(IFILEIO* pfileio);
		virtual ~OPNAW();
		void	WINAPI setfileio(IFILEIO* pfileio);
		
		bool	Init(uint c, uint r, bool ipflag, const TCHAR* path);
		bool	SetRate(uint c, uint r, bool ipflag = false);
		
		void	SetFMWait(int nsec);				// FM wait(nsec)
		void	SetSSGWait(int nsec);				// SSG wait(nsec)
		void	SetRhythmWait(int nsec);			// Rhythm wait(nsec)
		void	SetADPCMWait(int nsec);				// ADPCM wait(nsec)
		
		int		GetFMWait(void);					// FM wait(nsec)
		int		GetSSGWait(void);					// SSG wait(nsec)
		int		GetRhythmWait(void);				// Rhythm wait(nsec)
		int		GetADPCMWait(void);					// ADPCM wait(nsec)
		
		void	SetReg(uint addr, uint data);		// レジスタ設定
		void	Mix(Sample* buffer, int nsamples);	// 合成
		void	ClearBuffer(void);					// 内部バッファクリア
		
	private:
		Sample	pre_buffer[WAIT_PCM_BUFFER_SIZE * 2];
													// wait 時の合成音の退避
		int		fmwait;								// FM Wait(nsec)
		int		ssgwait;							// SSG Wait(nsec)
		int		rhythmwait;							// Rhythm Wait(nsec)
		int		adpcmwait;							// ADPCM Wait(nsec)
		
		int		fmwaitcount;						// FM Wait(count*1000)
		int		ssgwaitcount;						// SSG Wait(count*1000)
		int		rhythmwaitcount;					// Rhythm Wait(count*1000)
		int		adpcmwaitcount;						// ADPCM Wait(count*1000)
		
		int		read_pos;							// 書き込み位置
		int		write_pos;							// 読み出し位置
		int		count2;								// count 小数部分(*1000)
		
		Sample	ip_buffer[IP_PCM_BUFFER_SIZE * 2];	// 線形補間時のワーク
		uint	rate2;								// 出力周波数
		bool	interpolation2;						// 一次補間フラグ
		int		delta;								// 差分小数部(16384サンプルで分割)
		
		double	delta_double;						// 差分小数部
		
		bool	ffirst;								// データ初回かどうかのフラグ
		double	rest;								// 標本化定理リサンプリング時の前回の残りサンプルデータ位置
		int		write_pos_ip;						// 書き込み位置(ip)
		
		void	_Init(void);						// 初期化(内部処理)
		void	CalcWaitPCM(int waitcount);			// SetReg() wait 時の PCM を計算
		double	Sinc(double x);						// Sinc関数
		void	_Mix(Sample* buffer, int nsamples);	// 合成（一次補間なしVer.)
		double	Fmod2(double x, double y);			// 最小非負剰余
		
		inline int Limit(int v, int max, int min)
		{
			return v > max ? max : (v < min ? min : v);
		}
	};
}

#endif	// OPNAW_H
