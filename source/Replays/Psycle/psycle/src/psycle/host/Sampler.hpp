///\file
///\brief interface file for psycle::host::Sampler.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "Machine.hpp"
#include "XMInstrument.hpp"
#include <psycle/helpers/filter.hpp>
#include <psycle/helpers/resampler.hpp>
namespace psycle
{
	namespace host
	{
		class CGearTracker; // forward declaration
		class Instrument;

		#define SAMPLER_MAX_POLYPHONY		16
		#define SAMPLER_DEFAULT_POLYPHONY	8

		#define SAMPLER_CMD_NONE			0x00
		#define SAMPLER_CMD_PORTAUP			0x01
		#define SAMPLER_CMD_PORTADOWN		0x02
		#define SAMPLER_CMD_PORTA2NOTE		0x03
		#define SAMPLER_CMD_PANNING			0x08
		#define SAMPLER_CMD_OFFSET			0x09
		#define SAMPLER_CMD_VOLUME			0x0C
		#define SAMPLER_CMD_RETRIG			0x15
		#define SAMPLER_CMD_EXTENDED		0x0E
		#define SAMPLER_CMD_EXT_NOTEOFF		0xC0
		#define SAMPLER_CMD_EXT_NOTEDELAY	0xD0

		typedef enum
		{
			ENV_OFF = 0,
			ENV_ATTACK = 1,
			ENV_DECAY = 2,
			ENV_SUSTAIN = 3,
			ENV_RELEASE = 4,
			ENV_FASTRELEASE = 5
		}
		EnvelopeStage;

		class WaveDataController
		{
		public:
			WaveDataController();
			inline void WaveDataController::Work(helpers::dsp::resampler::work_func_type pResamplerWork, float& left_output, float& right_output );	
			inline void RampVolume();
			inline bool PostWork();
			void SetSpeed(double speeddouble);

			const XMInstrument::WaveData<int16_t>* wave;

			ULARGE_INTEGER _pos;
			int64_t _speed;
			float _vol; // 0..1 value of this voice volume,
			float _pan;
			float _lVolDest;
			float _rVolDest;
			float _lVolCurr;
			float _rVolCurr;
			void* resampler_data;
		};

		class Envelope
		{
		public:
			Envelope() : sratefactor(1.f) {}
			inline void TickEnvelope(int decaysamples);
			void UpdateSRate(float samplerate) {
				sratefactor = 44100.0f/samplerate;
			}
			EnvelopeStage _stage;
			float _value;
			float _step;
			float _attack;
			float _decay;
			float _sustain;
			float _release;
			float sratefactor;
		};

		class Voice
		{
		public:
			Voice();
			~Voice();
			void Init();
			void NoteOff();
			void NoteOffFast();
			void NewLine();
			void Work(int numsamples, helpers::dsp::resampler::work_func_type pResamplerWork, float* pSamplesL, float* pSamplesR);
			int Tick(PatternEntry* pData, int channelNum, dsp::resampler& resampler, int baseC, std::vector<PatternEntry>&multicmdMem);
			void PerformFxOld(dsp::resampler& resampler);
			void PerformFxNew(dsp::resampler& resampler);
			static inline int alteRand(int x)
			{
				return (x*rand())/32768;
			}

			WaveDataController controller;
			Envelope _envelope;
			Envelope _filterEnv;
			dsp::Filter _filter;
			Instrument* inst;
			int _instrument;
			int _channel;

			int _sampleCounter;		//Amount of samples since line Tick on this voice.
			int _triggerNoteOff;   //Amount of samples previous to do a delayed noteoff
			int _triggerNoteDelay;  //Amount of samples previous to do a delayed noteon (Also used for retrig)
			int _cutoff;
			float _coModify;
			int64_t _effPortaSpeed;
			// Line memory for command being executed{
			int effCmd;  //running command (like porta or retrig).
			int effVal;  //value related to the running command (like porta or retrig)
			//}
			// retrig {
			int effretTicks; // Number of ticks remaining for retrig
			float effretVol; // volume change amount
			int effretMode;  // volume change mode (multipler or sum)
			// } retrig
		};

		/// sampler.
		class Sampler : public Machine
		{
			static const uint32_t SAMPLERVERSION = 0x00000002;
			friend CGearTracker;
		public:
			Sampler(int index);
			virtual ~Sampler();
			virtual void Init(void);
			virtual void NewLine();
			virtual void PostNewLine();
			virtual void Tick(int channel, PatternEntry* pData);
			virtual void Stop(void);
			virtual int GenerateAudioInTicks(int startSample,  int numSamples);
			virtual bool Load(RiffFile* pFile); //old fileformat
			virtual bool LoadSpecificChunk(RiffFile* pFile, int version);
			virtual void SaveSpecificChunk(RiffFile* pFile);
			virtual const char* const GetName(void) const { return _psName; }
			virtual float GetAudioRange() const { return 32768.0f; }
			virtual void SetSampleRate(int sr);
			virtual bool NeedsAuxColumn() { return true; }
			virtual const char* AuxColumnName(int idx) const;
			virtual int NumAuxColumnIndexes();
			virtual bool playsTrack(const int track) const;

			void StopInstrument(int insIdx);
			void ChangeResamplerQuality(helpers::dsp::resampler::quality::type quality) ;
			void DefaultC4(bool correct) {
				baseC = correct? notecommands::middleC : 48;
			}
			bool isDefaultC4() const {
				return baseC == notecommands::middleC;
			}
			void LinearSlide(bool correct) {
				linearslide = correct;
			}
			bool isLinearSlide() {
				return linearslide;
			}

		protected:
			int GetFreeVoice() const;
			int GetCurrentVoice(int track) const;
			void EnablePerformFx();

			static char* _psName;
			unsigned char lastInstrument[MAX_TRACKS];
			int _numVoices;
			Voice _voices[SAMPLER_MAX_POLYPHONY];
			psycle::helpers::dsp::cubic_resampler _resampler;
			std::vector<PatternEntry> multicmdMem;
			int baseC;
			bool linearslide;
		};


		// Inline
		/////////////////////////////////
		void WaveDataController::Work(helpers::dsp::resampler::work_func_type pResamplerWork, float& left_output, float& right_output )
		{
			left_output = pResamplerWork(wave->pWaveDataL() + _pos.HighPart,
				_pos.HighPart, _pos.LowPart, wave->WaveLength(), resampler_data);
			if (wave->IsWaveStereo())
			{
				right_output = pResamplerWork(wave->pWaveDataR() + _pos.HighPart,
					_pos.HighPart, _pos.LowPart, wave->WaveLength(), resampler_data);
			}
		}
		void WaveDataController::RampVolume()
		{
			// calculate volume  (volume ramped)
			if(_lVolCurr>_lVolDest) {
				_lVolCurr-=0.005f;
				if(_lVolCurr<_lVolDest)	_lVolCurr=_lVolDest;
			}
			else if(_lVolCurr<_lVolDest) {
				_lVolCurr+=0.005f;
				if(_lVolCurr>_lVolDest)	_lVolCurr=_lVolDest;
			}
			if(_rVolCurr>_rVolDest) {
				_rVolCurr-=0.005f;
				if(_rVolCurr<_rVolDest) _rVolCurr=_rVolDest;
			}
			else if(_rVolCurr<_rVolDest) {
				_rVolCurr+=0.005f;
				if(_rVolCurr>_rVolDest)	_rVolCurr=_rVolDest;
			}
		}

		bool WaveDataController::PostWork()
		{
			_pos.QuadPart += _speed;

			// Loop handler
			//
			if ((wave->WaveLoopType() == XMInstrument::WaveData<>::LoopType::NORMAL) 
				&& (_pos.HighPart >= wave->WaveLoopEnd()))
			{
				_pos.HighPart -= (wave->WaveLoopEnd() - wave->WaveLoopStart());
			}
			return (_pos.HighPart < wave->WaveLength());
		}

		
		void Envelope::TickEnvelope(int decaysamples)
		{
			switch (_stage)
			{
			case ENV_ATTACK:
				_value += _step;
				if (_value > 1.0f)
				{
					_value = 1.0f;
					_stage = ENV_DECAY;
					_step = ((1.0f - _sustain)/decaysamples)*sratefactor;
				}
				break;
			case ENV_DECAY:
				_value -= _step;
				if (_value < _sustain)
				{
					_value = _sustain;
					_stage = ENV_SUSTAIN;
				}
				break;
			case ENV_RELEASE:
			case ENV_FASTRELEASE:
				_value -= _step;
				if (_value <= 0)
				{
					_value = 0;
					_stage = ENV_OFF;
				}
				break;
			default:break;
			}
		}
	}
}
