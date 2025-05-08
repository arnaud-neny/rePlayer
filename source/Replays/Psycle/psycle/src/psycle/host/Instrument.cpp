///\file
///\brief implementation file for psycle::host::Instrument.

#include <psycle/host/detail/project.private.hpp>
#include "Instrument.hpp"
#include <psycle/helpers/datacompression.hpp>
#include <psycle/helpers/value_mapper.hpp>
#include "Zap.hpp"
namespace psycle
{
	namespace host
	{
		Instrument::Instrument()
		{
			// clear everythingout
			Init();
		}

		Instrument::~Instrument()
		{
		}

		void Instrument::Init()
		{
			sampler_to_use=-1;
			_LOCKINST=false;

			// Reset envelope
			ENV_AT = 1;
			ENV_DT = 1;
			ENV_SL = 100;
			ENV_RT = 220;
			
			ENV_F_AT = 1;
			ENV_F_DT = 16500;
			ENV_F_SL = 64;
			ENV_F_RT = 16500;
			
			ENV_F_CO = 91;
			ENV_F_RQ = 11;
			ENV_F_EA = 0;
			ENV_F_TP = dsp::F_NONE;
			
			_loop = false;
			_lines = 16;
			
			_NNA = 0; // NNA set to Note Cut [Fast Release]
			
			_RPAN = false;
			_RCUT = false;
			_RRES = false;
		}

		void Instrument::CopyXMInstrument(const XMInstrument& instr, float ticktomillis)
		{
			Init();
			if (instr.AmpEnvelope().IsEnabled()) {
				SetVolEnvFromEnvelope(instr.AmpEnvelope(), ticktomillis);
			}
			if (instr.FilterEnvelope().IsEnabled()) {
				SetFilterEnvFromEnvelope(instr.FilterEnvelope(), ticktomillis);
			}
			ENV_F_TP = instr.FilterType();
			ENV_F_CO = instr.FilterCutoff();
			ENV_F_RQ = instr.FilterResonance();
			if (instr.Lines() > 0 ) {
				_lines = instr.Lines();
				_loop = true;
			}
			 if (instr.NNA() == XMInstrument::NewNoteAction::NOTEOFF) {
				_NNA = 1;
			}
			else if (instr.NNA() == XMInstrument::NewNoteAction::CONTINUE) {
				_NNA = 2;
			}
			else {
				_NNA = 0;
			}
			_RCUT = (instr.RandomCutoff() >= 0.5f);
			_RPAN = (instr.RandomPanning() >= 0.5f);
			_RRES = (instr.RandomResonance() >= 0.5f);
		}

		void Instrument::SetVolEnvFromEnvelope(const XMInstrument::Envelope& penvin, float ticktomillis)
		{
			XMInstrument::Envelope penv(penvin);
			// Adapting the point based volume envelope, to the ADSR based one in sampler instrument.
			penv.SetAdsr(true,false);
			SetEnvInternal(penv, ticktomillis, ENV_AT, ENV_DT, ENV_SL, ENV_RT);
		}
		void Instrument::SetFilterEnvFromEnvelope(const XMInstrument::Envelope& penvin, float ticktomillis)
		{
			XMInstrument::Envelope penv(penvin);
			// Adapting the point based volume envelope, to the ADSR based one in sampler instrument.
			penv.SetAdsr(true,false);
			SetEnvInternal(penv, ticktomillis, ENV_F_AT, ENV_F_DT, ENV_F_SL, ENV_F_RT);
			ENV_F_EA = static_cast<int>( (penv.GetValue(1)-penv.GetValue(0))*128.f );
		}
		void Instrument::SetEnvInternal(const XMInstrument::Envelope& penv, float ticktomillis, int &attack, int& decay, int& sustain, int& release){

			float conversion=1.0f;
			if (penv.Mode() == XMInstrument::Envelope::Mode::TICK) {
				conversion = ticktomillis;
			}
			//Truncate to 220 samples boundaries, and ensure it is not zero.
			// ATTACK TIME
			float tmp = penv.GetTime(1)*44.1f*conversion;
			tmp = (tmp/220.f)*220.f; if (tmp <=0.f) tmp=1.f; 
			attack = static_cast<int>(tmp);

			// DECAY TIME
			tmp = penv.GetTime(2)*44.1f*conversion;
			tmp = (tmp/220.f)*220.f; if (tmp <=0.f) tmp=1.f; 
			decay = static_cast<int>(tmp);
			
			// SUSTAIN LEVEL
			sustain = static_cast<int>(penv.GetValue(2)*100.f);

			// RELEASE TIME
			tmp = penv.GetTime(3)*44.1f*conversion;
			tmp = (tmp/220.f)*220.f; if (tmp <=0.f) tmp=1.f; 
			release=static_cast<int>(tmp);
		}


		void Instrument::LoadFileChunk(RiffFile* pFile,int version,SampleList& samples, int instrmIdx, bool fullopen)
		{
			Init();
			pFile->Read(_loop);
			pFile->Read(_lines);
			pFile->Read(_NNA);

			pFile->Read(ENV_AT);
			pFile->Read(ENV_DT);
			pFile->Read(ENV_SL);
			pFile->Read(ENV_RT);
			
			pFile->Read(ENV_F_AT);
			pFile->Read(ENV_F_DT);
			pFile->Read(ENV_F_SL);
			pFile->Read(ENV_F_RT);

			pFile->Read(ENV_F_CO);
			pFile->Read(ENV_F_RQ);
			pFile->Read(ENV_F_EA);
			int val;
			pFile->Read(val);
			ENV_F_TP = static_cast<helpers::dsp::FilterType>(val);

			int pan=128;
			pFile->Read(pan);
			pFile->Read(_RPAN);
			pFile->Read(_RCUT);
			pFile->Read(_RRES);
			char instrum_name[32];
			pFile->ReadString(instrum_name,sizeof(instrum_name));

			// now we have to read waves

			int numwaves;
			pFile->Read(&numwaves, sizeof(numwaves));
			for (int i = 0; i < numwaves; i++)
			{
				LoadWaveSubChunk(pFile, samples, instrmIdx, pan, instrum_name, fullopen, i );
			}

			if ((version & 0xFF) >= 1) 
			{ //revision 1 or greater
				pFile->Read(sampler_to_use);
				pFile->Read(_LOCKINST);
			}

			//Ensure validity of values read
			if (sampler_to_use < 0 || sampler_to_use >= MAX_BUSES) { _LOCKINST=false; sampler_to_use = -1; }
			//Truncate to 220 samples boundaries, and ensure it is not zero.
			ENV_AT = (ENV_AT/220)*220; if (ENV_AT <=0) ENV_AT=1;
			ENV_DT = (ENV_DT/220)*220; if (ENV_DT <=0) ENV_DT=1;
			if (ENV_RT == 16) ENV_RT = 220;
			else { ENV_RT = (ENV_RT/220)*220; if (ENV_RT <=0) ENV_RT=1; }
			ENV_F_AT = (ENV_F_AT/220)*220; if (ENV_F_AT <=0) ENV_F_AT=1;
			ENV_F_DT = (ENV_F_DT/220)*220; if (ENV_F_DT <=0) ENV_F_DT=1;
			ENV_F_RT = (ENV_F_RT/220)*220; if (ENV_F_RT <=0) ENV_F_RT=1;
		}
		void Instrument::LoadWaveSubChunk(RiffFile* pFile,SampleList& samples, int instrIdx, int pan, char * instrum_name, bool fullopen, int loadIdx)
		{
			char Header[8];
			UINT version;
			UINT size;

			pFile->Read(&Header,4);
			Header[4] = 0;
			pFile->Read(&version,sizeof(version));
			pFile->Read(&size,sizeof(size));

			//fileformat supports several waves, but sampler only supports one.
			if (strcmp(Header,"WAVE")==0 && version <= CURRENT_FILE_VERSION_WAVE || loadIdx == 0)
			{
				XMInstrument::WaveData<> wave;
				wave.PanFactor(value_mapper::map_256_1(pan));
				wave.WaveSampleRate(44100);
				//This index represented the index position of this wave for the instrument. 0 to 15.
				UINT legacyindex;
				pFile->Read(&legacyindex,sizeof(legacyindex));
				pFile->Read(wave.m_WaveLength);
				unsigned short waveVolume = 0;
				pFile->Read(waveVolume);
				wave.WaveGlobVolume(waveVolume*0.01f);
				pFile->Read(wave.m_WaveLoopStart);
				pFile->Read(wave.m_WaveLoopEnd);
				
				int tmp = 0;
				pFile->Read(tmp);
				wave.WaveTune(tmp);
				pFile->Read(tmp);
				//Current sampler uses 100 cents. Older used +-256
				tmp = static_cast<int>((float)tmp/2.56f);
				wave.WaveFineTune(tmp);
				bool doloop = false;;
				pFile->Read(doloop);
				wave.WaveLoopType(doloop?XMInstrument::WaveData<>::LoopType::NORMAL:XMInstrument::WaveData<>::LoopType::DO_NOT);
				pFile->Read(wave.m_WaveStereo);
				char dummy[32];
				//Old sample name, never used.
				pFile->ReadString(dummy,sizeof(dummy));
				wave.WaveName(instrum_name);
				
				UINT packedsize;
				pFile->Read(&packedsize,sizeof(packedsize));
				byte* pData;
				
				if ( fullopen )
				{
					pData = new byte[packedsize+4];// +4 to avoid any attempt at buffer overflow by the code
					pFile->Read(pData,packedsize);
					DataCompression::SoundDesquash(pData,&wave.m_pWaveDataL);
					zapArray(pData);
				}
				else
				{
					pFile->Skip(packedsize);
					wave.m_pWaveDataL=new signed short[2];
				}

				if (wave.m_WaveStereo)
				{
					pFile->Read(&packedsize,sizeof(packedsize));
					if ( fullopen )
					{
						pData = new byte[packedsize+4]; // +4 to avoid any attempt at buffer overflow by the code
						pFile->Read(pData,packedsize);
						DataCompression::SoundDesquash(pData,&wave.m_pWaveDataR);
						zapArray(pData);
					}
					else
					{
						pFile->Skip(packedsize);
						wave.m_pWaveDataR =new signed short[2];
					}
				}
				samples.SetSample(wave, instrIdx);
			}
			else
			{
				pFile->Skip(size);
			}
		}

		void Instrument::SaveFileChunk(RiffFile* pFile)
		{
			pFile->Write(_loop);
			pFile->Write(_lines);
			pFile->Write(_NNA);

			pFile->Write(ENV_AT);
			pFile->Write(ENV_DT);
			pFile->Write(ENV_SL);
			pFile->Write(ENV_RT);
			
			pFile->Write(ENV_F_AT);
			pFile->Write(ENV_F_DT);
			pFile->Write(ENV_F_SL);
			pFile->Write(ENV_F_RT);

			pFile->Write(ENV_F_CO);
			pFile->Write(ENV_F_RQ);
			pFile->Write(ENV_F_EA);
			pFile->Write(ENV_F_TP);

			//No longer saving pan in version 2
			int legacypan = 128;
			pFile->Write(legacypan);

			pFile->Write(_RPAN);
			pFile->Write(_RCUT);
			pFile->Write(_RRES);

			//No longer saving name in version 2
			char legacyname ='\0';
			pFile->Write(legacyname);

			//No longer saving wave subchunk in version 2
			int legacynumwaves = 0;
			pFile->Write(legacynumwaves);

			pFile->Write(sampler_to_use);
			pFile->Write(_LOCKINST);
		}
	}
}
