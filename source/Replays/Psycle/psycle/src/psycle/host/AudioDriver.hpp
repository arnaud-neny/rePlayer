///\file
///\brief interface file for psycle::host::AudioDriver.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include <MMReg.h>

namespace psycle
{
	namespace host
	{
		class ConfigStorage;
		class AudioDriver;

		class AudioDriverEvent : public CEvent
		{
		public:
			AudioDriverEvent() : CEvent(FALSE, TRUE) {}
		};

		typedef float* (*AUDIODRIVERWORKFN)(void* context, int numSamples);
		typedef enum {
			no_mode = -1,
			mono_mix = 0,
			mono_left,
			mono_right,
			stereo
		} channel_mode;
		class AudioDriverInfo
		{
		public:
			//Don't make it longer than 32 chars (WaveIn machine saves this name)
			AudioDriverInfo(char const *info);
			char const *_psName;
		};


	/// holds the info about sample rate, bit depth, etc
	class AudioDriverSettings {
	public:
		AudioDriverSettings();
		AudioDriverSettings(const AudioDriverSettings& othersettings);
		AudioDriverSettings& operator=(const AudioDriverSettings& othersettings);
		virtual bool operator!=(AudioDriverSettings const &);
		virtual bool operator==(AudioDriverSettings const & other) { return !((*this) != other); }

		virtual void SetDefaultSettings(bool include_others=true);
		virtual void Load(ConfigStorage &) = 0;
		virtual void Save(ConfigStorage &) = 0;

		virtual const AudioDriverInfo& GetInfo() const = 0;
		virtual AudioDriver* NewDriver() = 0;

	///\name getter/setter for sample rate
	///\{
		public:
			inline unsigned int samplesPerSec() const { return samplesPerSec_; }
			void setSamplesPerSec(unsigned int value) { samplesPerSec_ = value; }
		private:
			unsigned int samplesPerSec_;
	///\}

	///\name getter/setter for sample bit depth, per-channel.. bitDepth is the container size. validBitDepth is the actual number of bits used.
	/// Valid values:  8/8, 16/16 , 32/24 (int) and 32/32 (float)
	///\{
		public:
			inline bool dither() const { return dither_; }
			inline unsigned int validBitDepth() const { return validBitDepth_; }
			inline unsigned int bitDepth() const { return bitDepth_; }
			/// getter for number of bytes per sample (counting all channels).
			inline unsigned int frameBytes() const { return frameBytes_; }
			void setValidBitDepth(unsigned int value) {
				validBitDepth_ = value;
				bitDepth_ = (value == 24) ? 32 : value;
				frameBytes_ = (channelMode_ == stereo) ? (bitDepth_ >> 2) : (bitDepth_ >> 3);
			}
			void setDither(bool dither) { dither_ = dither; }
		private:
			bool dither_;
			unsigned int validBitDepth_;
			unsigned int bitDepth_;
			unsigned int frameBytes_;
	///\}

	///\name getter/setter for channel mode
	///\{
		public:
			inline unsigned int numChannels() const { return (channelMode_ == stereo) ? 2 : 1; }
			inline channel_mode channelMode() const { return channelMode_; }
			void setChannelMode(channel_mode value) {
				channelMode_ = value;
				frameBytes_ = (channelMode_ == stereo) ? (bitDepth_ >> 2) : (bitDepth_ >> 3);
			}
		private:
			channel_mode channelMode_;
	///\}

	///\name getter/setter for the audio block size (in samples comprising all channels)
	///\{
		public:
			inline unsigned int blockFrames() const { return blockFrames_; }
			void setBlockFrames(unsigned int value) {
				blockFrames_ = value;
				assert(blockFrames_ < MAX_SAMPLES_WORKFN);
			}
			///\name getter/setter for the audio block size (in bytes)
			inline unsigned int blockBytes() const { return blockFrames_ * frameBytes_; }
			void setBlockBytes(unsigned int value) { setBlockFrames(value / frameBytes_);	}
			/// getter for the whole buffer size (in bytes).
			inline unsigned int totalBufferBytes() const { return blockFrames_ * frameBytes_ * blockCount_; }
		private:
			unsigned int blockFrames_;
	///\}

	///\name getter/setter for number of blocks.
	///\{
		public:
			inline unsigned int blockCount() const { return blockCount_; }
			void setBlockCount(unsigned int value) { blockCount_ = value; }
		private:
			unsigned int blockCount_;
	///\}
	};

		/// output device interface.
		class AudioDriver
		{
		protected:
			AudioDriver(){}
		public:
			virtual ~AudioDriver() {};

			virtual AudioDriverSettings& settings() const = 0;

			//Sets the callback and initializes the info (like which ports there are)
			virtual void Initialize(AUDIODRIVERWORKFN pCallback, void* context) {};
			//Frees all resources related to this audio driver.
			virtual void Reset(void) {};
			//Enables or disables the playback/recording with this driver.
			virtual bool Enable(bool e) { return false; }
			//Initalize has been called.
			virtual bool Initialized(void) const { return true; }
			//Is enabled?
			virtual bool Enabled() const { return false; }
			//Launches the configuration dialog
			virtual void Configure(void) {};
			//Refreshes the playback and capture port lists.
			virtual void RefreshAvailablePorts() {}
			virtual void GetPlaybackPorts(std::vector<std::string> &ports) const { ports.resize(0); }
			virtual void GetCapturePorts(std::vector<std::string> &ports) const { ports.resize(0); }
			virtual void GetReadBuffers(int idx, float **pleft, float **pright,int numsamples) { pleft=0; pright=0; return; }
			virtual bool AddCapturePort(int idx){ return false; };
			virtual bool RemoveCapturePort(int idx){ return false; }
			virtual uint32_t GetWritePosInSamples() const { return 0; }
			virtual uint32_t GetPlayPosInSamples() { return 0; }/*cannot be const, because of the way the directsound method is implemented*/
			virtual uint32_t GetInputLatencyMs() const { return GetInputLatencySamples()*1000 / settings().samplesPerSec(); }
			virtual uint32_t GetInputLatencySamples() const = 0;
			virtual uint32_t GetOutputLatencyMs() const { return GetOutputLatencySamples()*1000 / settings().samplesPerSec(); }
			virtual uint32_t GetOutputLatencySamples() const = 0;

			//amount of buffers.
			int GetNumBuffers() const { return settings().blockCount(); }
			//size of each buffer
			int GetBufferBytes() const { return settings().blockBytes(); }
			//size of each buffer, for a whole sample (counting all channels)
			int GetBufferSamples() const { return settings().blockFrames(); }
			//Size of a whole sample (counting all channels)
			int GetSampleSizeBytes() const { return settings().frameBytes(); }
			//Size of a mono sample in bits. If validBits=32 floats are assumed!
			int GetSampleBits() const { return settings().bitDepth(); }
			//Amount of bits valid inside a mono sample. (left aligned. i.e. lower bits unused). If validBits=32 floats are assumed!
			int GetSampleValidBits() const { return settings().validBitDepth(); }
			int GetSamplesPerSec() const { return settings().samplesPerSec(); }
			channel_mode GetChannelMode() const { return settings().channelMode(); }

			static void PrepareWaveFormat(WAVEFORMATEXTENSIBLE& wf, int channels, int sampleRate, int bits, int validBits);
			//In -> -32768.0..32768.0 , out -32768..32767
			static void Quantize16(float *pin, int *piout, int c);
			//In -> -32768.0..32768.0 , out -32768..32767
			static void Quantize16WithDither(float *pin, int *piout, int c);
			//In -> -8388608.0..8388608.0, out  -2147483648.0 to 2147483648.0
			static void Quantize24in32Bit(float *pin, int *piout, int c);
			//In -> -8388608.0..8388608.0 in 4 bytes, out -8388608..8388608, aligned to 3 bytes, Big endian
			static void Quantize24BE(float *pin, int *piout, int c);
			//In -> -8388608.0..8388608.0 in 4 bytes, out -8388608..8388608, aligned to 3 bytes, little endian
			static void Quantize24LE(float *pin, int *piout, int c);
			//In -> -32768..32767 stereo interlaced, out -32768.0..32767.0 stereo deinterlaced
			static void DeQuantize16AndDeinterlace(short int *pin, float *poutleft,float *poutright,int c);
			//In -> -2147483648..2147483647 stereo interlaced, out -32768.0..32767.0 stereo deinterlaced
			static void DeQuantize32AndDeinterlace(int *pin, float *poutleft,float *poutright,int c);
			//In -> -1.0..1.0 stereo interlaced, out -32768.0..32767.0 stereo deinterlaced
			static void DeinterlaceFloat(float *pin, float *poutleft,float *poutright,int c);
		};

		class SilentSettings : public AudioDriverSettings
		{
		public:
			SilentSettings(){ SetDefaultSettings(); };
			SilentSettings(const SilentSettings& othersettings):AudioDriverSettings(othersettings){};
			virtual AudioDriver* NewDriver();
			virtual AudioDriverInfo& GetInfo() const { return info_; };

			virtual void Load(ConfigStorage &) {};
			virtual void Save(ConfigStorage &) {};
		private:
			static AudioDriverInfo info_;
		};

		class SilentDriver: public AudioDriver
		{
		public:
			SilentDriver(SilentSettings* settings):settings_(settings) {}
			virtual AudioDriverSettings& settings() const { return *settings_; };
			virtual uint32_t GetInputLatencySamples() const { return 0; }
			virtual uint32_t GetOutputLatencySamples() const { return 0; }
		protected:
			SilentSettings* settings_;
		};

	}
}