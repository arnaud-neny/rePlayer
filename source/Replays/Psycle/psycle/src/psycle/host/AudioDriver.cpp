///\file
///\brief implementation file for psycle::host::AudioDriver.

#include <psycle/host/detail/project.private.hpp>
#include "AudioDriver.hpp"
#include <ks.h>
#include <KsMedia.h>
#include <psycle/helpers/math.hpp>
namespace psycle
{
	namespace host
	{
#if !defined SHORT_MIN
		#define SHORT_MIN -32768
#endif
#if !defined SHORT_MAX
		#define SHORT_MAX 32767
#endif

		AudioDriverInfo SilentSettings::info_("Silent");

		// returns random value between 0 and 1
		// i got the magic numbers from csound so they should be ok but 
		// I haven't checked them myself
		inline double frand()
		{
			static long stat = 0x16BA2118;
			stat = (stat * 1103515245 + 12345) & 0x7fffffff;
			return (double)stat * (1.0 / 0x7fffffff);
		}
		AudioDriverInfo::AudioDriverInfo(char const *info):_psName(info) {
		}

		AudioDriverSettings::AudioDriverSettings()
		{
		}
		AudioDriverSettings::AudioDriverSettings(const AudioDriverSettings& othersettings)
		{
			operator=(othersettings);
		}
		AudioDriverSettings& AudioDriverSettings::operator=(const AudioDriverSettings& othersettings)
		{
			samplesPerSec_ = othersettings.samplesPerSec_;
			channelMode_ = othersettings.channelMode_;
			blockCount_ = othersettings.blockCount_;
			validBitDepth_ = othersettings.validBitDepth_;
			bitDepth_ = othersettings.bitDepth_;
			frameBytes_ = othersettings.frameBytes_;
			blockFrames_ = othersettings.blockFrames_;
			return *this;
		}

		bool AudioDriverSettings::operator!=(AudioDriverSettings const & othersettings) {
			return
				samplesPerSec_ != othersettings.samplesPerSec_ ||
				channelMode_ != othersettings.channelMode_ ||
				blockCount_ != othersettings.blockCount_ ||
				validBitDepth_ != othersettings.validBitDepth_ ||
				bitDepth_ != othersettings.bitDepth_ ||
				frameBytes_ != othersettings.frameBytes_ ||
				blockFrames_ != othersettings.blockFrames_;
		}
		void AudioDriverSettings::SetDefaultSettings(bool include_others)
		{
			samplesPerSec_ = 44100;
			channelMode_ = stereo;
			blockCount_ = 0;
			dither_ = true;
			setValidBitDepth(16);
			setBlockFrames(0);
		};


		void AudioDriver::PrepareWaveFormat(WAVEFORMATEXTENSIBLE& wf, int channels, int sampleRate, int bits, int validBits)
		{
			// Set up wave format structure. 
			ZeroMemory( &wf, sizeof(WAVEFORMATEXTENSIBLE) );
			wf.Format.nChannels = channels;
			wf.Format.wBitsPerSample = bits;
			wf.Format.nSamplesPerSec = sampleRate;
			wf.Format.nBlockAlign = wf.Format.nChannels * wf.Format.wBitsPerSample / 8;
			wf.Format.nAvgBytesPerSec = wf.Format.nSamplesPerSec * wf.Format.nBlockAlign;

			if(bits <= 16) {
				wf.Format.wFormatTag = WAVE_FORMAT_PCM;
				wf.Format.cbSize = 0;
			}
			else {
				wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
				wf.Format.cbSize = 0x16;
				wf.Samples.wValidBitsPerSample  = validBits;
				if(channels == 2) {
					wf.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
				}
				if(validBits ==32) {
					wf.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
				}
				else {
					wf.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
				}
			}
		}
		//In -> -32768.0..32768.0 , out -32768..32767
		void AudioDriver::Quantize16WithDither(float *pin, int *piout, int c)
		{
			double const d2i = (1.5 * (1 << 26) * (1 << 26));
			do
			{
				double res = ((double)pin[1] + frand()) + d2i;
				int r = *(int *)&res;

				if (r < SHORT_MIN)
				{
					r = SHORT_MIN;
				}
				else if (r > SHORT_MAX)
				{
					r = SHORT_MAX;
				}
				res = ((double)pin[0] + frand()) + d2i;
				int l = *(int *)&res;

				if (l < SHORT_MIN)
				{
					l = SHORT_MIN;
				}
				else if (l > SHORT_MAX)
				{
					l = SHORT_MAX;
				}
				*piout++ = (r << 16) | static_cast<uint16_t>(l);
				pin += 2;
			}
			while(--c);
		}
		//In -> -32768.0..32768.0 , out -32768..32767
		void AudioDriver::Quantize16(float *pin, int *piout, int c)
		{
			do
			{
				int r = helpers::math::round<int,float>(pin[1]);
				if (r < SHORT_MIN)
				{
					r = SHORT_MIN;
				}
				else if (r > SHORT_MAX)
				{
					r = SHORT_MAX;
				}

				int l = helpers::math::round<int,float>(pin[0]);
				if (l < SHORT_MIN)
				{
					l = SHORT_MIN;
				}
				else if (l > SHORT_MAX)
				{
					l = SHORT_MAX;
				}
				*piout++ = (r << 16) | static_cast<uint16_t>(l);
				pin += 2;
			}
			while(--c);
		}
		//In -> -8388608.0..8388608.0, out  -2147483648.0 to 2147483648.0
		void AudioDriver::Quantize24in32Bit(float *pin, int *piout, int c)
		{
			// TODO Don't really know why, but the -100 is what made the clipping work correctly.
			int const max((1u << ((sizeof(int32_t) << 3) - 1)) - 100);
			int const min(-max - 1);
			for(int i = 0; i < c; ++i) {
				*piout++ = psycle::helpers::math::rint<int32_t>(psycle::helpers::math::clip(float(min), (*pin++) * 65536.0f, float(max)));
				*piout++ = psycle::helpers::math::rint<int32_t>(psycle::helpers::math::clip(float(min), (*pin++) * 65536.0f, float(max)));
			}
		}
		//In -> -8388608.0..8388608.0 in 4 bytes, out -8388608..8388608, aligned to 3 bytes, Big endian
		void AudioDriver::Quantize24BE(float *pin, int *piout, int c)
		{
            unsigned char *cptr = (unsigned char *) piout;
			for(int i = 0; i < c; ++i) {
				int outval = psycle::helpers::math::rint_clip<int32_t, 24>((*pin++) * 256.0f);
				*cptr++ = (unsigned char) ((outval >> 16) & 0xFF);
				*cptr++ = (unsigned char) ((outval >> 8) & 0xFF);
				*cptr++ = (unsigned char) (outval & 0xFF);

				outval = psycle::helpers::math::rint_clip<int32_t, 24>((*pin++) * 256.0f);
				*cptr++ = (unsigned char) ((outval >> 16) & 0xFF);
				*cptr++ = (unsigned char) ((outval >> 8) & 0xFF);
				*cptr++ = (unsigned char) (outval & 0xFF);
			}
		}
		//In -> -8388608.0..8388608.0 in 4 bytes, out -8388608..8388608, aligned to 3 bytes, little endian
		void AudioDriver::Quantize24LE(float *pin, int *piout, int c)
		{
            unsigned char *cptr = (unsigned char *) piout;
			for(int i = 0; i < c; ++i) {
				int outval = psycle::helpers::math::rint_clip<int32_t, 24>((*pin++) * 256.0f);
				*cptr++ = (unsigned char) (outval & 0xFF);
				*cptr++ = (unsigned char) ((outval >> 8) & 0xFF);
				*cptr++ = (unsigned char) ((outval >> 16) & 0xFF);

				outval = psycle::helpers::math::rint_clip<int32_t, 24>((*pin++) * 256.0f);
				*cptr++ = (unsigned char) (outval & 0xFF);
				*cptr++ = (unsigned char) ((outval >> 8) & 0xFF);
				*cptr++ = (unsigned char) ((outval >> 16) & 0xFF);
			}
		}
		//In -> -32768..32767 stereo interlaced, out -32768.0..32767.0 stereo deinterlaced
		void AudioDriver::DeQuantize16AndDeinterlace(short int *pin, float *poutleft,float *poutright,int c)
		{
			do
			{
				*poutleft++ = *pin++;
				*poutright++ = *pin++;
			}
			while(--c);
		}
		//In -> -2147483648..2147483647 stereo interlaced, out -32768.0..32767.0 stereo deinterlaced
		void AudioDriver::DeQuantize32AndDeinterlace(int *pin, float *poutleft,float *poutright,int c)
		{
			do
			{
				*poutleft++ = (*pin++)*0.0000152587890625;
				*poutright++ = (*pin++)*0.0000152587890625;
			}
			while(--c);
		}
		//In -> -1.0..1.0 stereo interlaced, out -32768.0..32767.0 stereo deinterlaced
		void AudioDriver::DeinterlaceFloat(float *pin, float *poutleft,float *poutright,int c)
		{
			do
			{
				*poutleft++ = (*pin++)*32768.f;
				*poutright++ = (*pin++)*32768.f;
			}
			while(--c);
		}
	
		AudioDriver* SilentSettings::NewDriver() {
			return new SilentDriver(this);
		}
	}
}
