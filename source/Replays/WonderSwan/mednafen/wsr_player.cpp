//mednafen
#include "wswan/wswan.h"
#include "wswan/v30mz.h"
#include "wswan/gfx.h"
#include "wswan/memory.h"
#include "wswan/sound.h"
//#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include "../wsr_player_common/wsr_player_intf.h"
#include "../wsr_player_common/wsr_player_common.h"
//#include "wsr_player.h"

namespace rePlayer::mednafen
{
	static uint32 g_channel_mute = 0;
	static uint8  g_first_song_number = 0;
	static WSRPlayerSoundBuf g_buffer = { 0, 0, 0, 0 };

	void InitWSR(void)
	{

	}

	int LoadWSR(const void* pFile, unsigned Size)
	{
		if (Size <= 0x20 || !pFile) return 0;
		if (ReadLE32((const uint8_t*)pFile + Size - 0x20) != WSR_HEADER_MAGIC)
			return 0;

		uint32_t buflen = (4096 << 2);
		g_buffer.ptr = (uint8_t*)malloc(buflen);
		if (!g_buffer.ptr)
		{
			return 0;
		}
		g_buffer.len = buflen;
		g_buffer.fil = 0;
		g_buffer.cur = 0;

		try
		{
			MDFN_IEN_WSWAN::Load((const uint8_t*)pFile, Size);
		}
		catch (...)
		{
			free(g_buffer.ptr);
			g_buffer.ptr = 0;
			g_buffer.len = 0;
			return 0;
		}

		g_first_song_number = *((const uint8_t*)pFile + Size - 0x20 + 0x05);
		InitWSR();

		return 1;
	}


	void CloseWSR()
	{
		MDFN_IEN_WSWAN::CloseGame();

		if (g_buffer.ptr)
		{
			free(g_buffer.ptr);
			g_buffer.ptr = 0;
		}
		g_buffer.len = 0;
		g_buffer.fil = 0;
		g_buffer.cur = 0;
	}


	int GetFirstSong(void)
	{
		return g_first_song_number;
	}


	unsigned SetFrequency(unsigned int Freq)
	{
		if (Freq < 12000 || Freq > 192000)
			Freq = 44100;
		MDFN_IEN_WSWAN::WSwan_SetSoundRate(Freq);
		return Freq;
	}

	void SetChannelMuting(unsigned int Mute)
	{
		g_channel_mute = Mute;
		MDFN_IEN_WSWAN::WSwan_SetChannelMute(Mute);
		return;
	}

	unsigned int GetChannelMuting(void)
	{
		return g_channel_mute;
	}

	void ResetWSR(unsigned SongNo)
	{
		MDFN_IEN_WSWAN::Reset(SongNo & 0xff);
		g_buffer.cur = 0;
		g_buffer.fil = 0;
	}

	void FlushBufferWSR(const short* finalWave, unsigned long length)
	{
		if (length > g_buffer.len - g_buffer.fil)
		{
			length = g_buffer.len - g_buffer.fil;
		}
		if (length > 0)
		{
			memcpy(g_buffer.ptr + g_buffer.fil, finalWave, length);
			g_buffer.fil += length;
		}
	}

	//refferd to viogsf's drvimpl.cpp. thanks the author.
	int UpdateWSR(void* pBuf, unsigned Buflen, unsigned Samples)
	{
		int ret = 0;
		uint32_t bytes = Samples << 2;
		uint8_t* des = (uint8_t*)pBuf;

		if (g_buffer.ptr == 0 || pBuf == 0)
			return 0;

		while (bytes > 0)
		{
			uint32_t remain = g_buffer.fil - g_buffer.cur;
			while (remain == 0)
			{
				g_buffer.cur = 0;
				g_buffer.fil = 0;

				while (!MDFN_IEN_WSWAN::wsExecuteLine())
				{

				}

				g_buffer.fil = (MDFN_IEN_WSWAN::WSwan_SoundFlush((int16*)(g_buffer.ptr), (g_buffer.len >> 2))) << 2;
				MDFN_IEN_WSWAN::v30mz_timestamp = 0;

				remain = g_buffer.fil - g_buffer.cur;
			}
			uint32_t len = remain;
			if (len > bytes)
			{
				len = bytes;
			}
			memcpy(des, g_buffer.ptr + g_buffer.cur, len);
			bytes -= len;
			des += len;
			g_buffer.cur += len;
			ret += len;
		}
		return ret;

	}


	WSRPlayerApi g_wsr_player_api =
	{
		LoadWSR,
		GetFirstSong,
		SetFrequency,
		SetChannelMuting,
		GetChannelMuting,
		ResetWSR,
		CloseWSR,
		UpdateWSR
	};
}
// namespace rePlayer::mednafen