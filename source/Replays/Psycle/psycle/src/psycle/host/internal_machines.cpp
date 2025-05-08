
#include <psycle/host/detail/project.private.hpp>
#include "internal_machines.hpp"
#include "Configuration.hpp"
#include "Song.hpp"
#include "Player.hpp"
#include "AudioDriver.hpp"
#include "cpu_time_clock.hpp"
#include "PsycleConfig.hpp"
#include <psycle/helpers/math.hpp>

namespace psycle
{
	namespace host
	{

		char* Dummy::_psName = "DummyPlug";
		char* DuplicatorMac::_psName = "Dupe it!";
		char* DuplicatorMac2::_psName = "Split and Dupe it!";
		char* AudioRecorder::_psName = "Recorder";
		char* Mixer::_psName = "Mixer";

		//////////////////////////////////////////////////////////////////////////
		// Dummy
		Dummy::Dummy(int index)
			: wasVST(false)
		{
			_macIndex = index;
			_type = MACH_DUMMY;
			_mode = MACHMODE_FX;
			strncpy(_editName, _psName, sizeof(_editName)-1);
			_editName[sizeof(_editName)-1]='\0';
			InitializeSamplesVector();
		}
		Dummy::Dummy(Machine *mac)
			: Machine(mac)
		{
			_type = MACH_DUMMY;
			_numPars = 0;
			_nCols = 1;
			std::stringstream s;
			s << "X!" << mac->GetEditName();
			SetEditName(s.str());
			InitializeSamplesVector();

			if (mac->_type == MACH_VST || mac->_type == MACH_VSTFX)
				wasVST = true;
			else
				wasVST = false;
		}
		int Dummy::GenerateAudio(int numSamples, bool measure_cpu_usage)
		{
			UpdateVuAndStanbyFlag(numSamples);
			return numSamples;
		}

		// Since Dummy is used by the loader to load broken/missing plugins, 
		// its "LoadSpecificChunk" skips the data of the chunk so that the
		// song loader can continue the sequence.
		bool Dummy::LoadSpecificChunk(RiffFile* pFile, int version)
		{
			UINT size;
			pFile->Read(&size, sizeof size); // size of this part params to load
			pFile->Skip(size);
			return true;
		}
		//////////////////////////////////////////////////////////////////////////
		// NoteDuplicator
		MultiMachine::MultiMachine(int index, int nums)
			: NUMMACHINES(nums), noteOffset(nums, 0), macOutput(nums, 0)
		{			
			bisTicking = false;
			for (int i=0;i<NUMMACHINES;i++)
			{
				macOutput[i]=-1;
				for (int j=0;j<MAX_TRACKS;j++)
				{
					allocatedchans[j][i] = -1;
				}
			}
			for (int i=0;i<MAX_VIRTUALINSTS;i++)
			{
				for (int j=0;j<MAX_TRACKS;j++)
				{
					availablechans[i][j] = true;
				}
			}
			///\todo: multimachine does not need buffers.
			InitializeSamplesVector();
		}
		MultiMachine::~MultiMachine()
		{
		}

		void MultiMachine::Init()
		{
			Machine::Init();
			for (int i=0;i<NUMMACHINES;i++)
			{
				macOutput[i]=-1;
				for (int j=0;j<MAX_TRACKS;j++)
				{
					allocatedchans[j][i] = -1;
				}
			}
			for (int i=0;i<MAX_VIRTUALINSTS;i++)
			{
				for (int j=0;j<MAX_TRACKS;j++)
				{
					availablechans[i][j] = true;
				}
			}
		}
		void MultiMachine::Stop()
		{
			for (int i=0;i<NUMMACHINES;i++)
			{
				for (int j=0;j<MAX_TRACKS;j++)
				{
					allocatedchans[j][i] = -1;
				}
			}
			for (int i=0;i<MAX_VIRTUALINSTS;i++)
			{
				for (int j=0;j<MAX_TRACKS;j++)
				{
					availablechans[i][j] = true;
				}
			}
		}


		void MultiMachine::Tick(int channel,PatternEntry* pData)
		{
			if ( !_mute && !bisTicking) // Prevent possible loops of MultiMachines.
			{
				bisTicking=true;
				for (int i=0;i<NUMMACHINES;i++)
				{
					if (macOutput[i] != -1)
					{
						int alternateinst=-1;
						Machine * pmac = Global::song().GetMachineOfBus(macOutput[i], alternateinst);
						if (pmac != NULL )
						{
							AllocateVoice(channel,i);
							PatternEntry pTemp = *pData;
							CustomTick(channel,i, pTemp);
							if (pTemp._note < notecommands::release && alternateinst != -1) pTemp._inst = alternateinst;
							// this can happen if the parameter is the machine itself.
							if (pmac != this) 
							{
								pmac->Tick(allocatedchans[channel][i],&pTemp);
								if (pTemp._note >= notecommands::release )
								{
									DeallocateVoice(channel,i);
								}
							}
							else
							{
								DeallocateVoice(channel,i);
							}
						}
					}
				}
			}
			bisTicking=false;
		}
		void MultiMachine::AllocateVoice(int channel,int machine)
		{
			// If this channel already has allocated channels, use them.
			if ( allocatedchans[channel][machine] != -1 )
				return;
			// If not, search an available channel
			int j=channel;
			while (j<MAX_TRACKS && !availablechans[macOutput[machine]][j]) j++;
			if ( j == MAX_TRACKS)
			{
				j=0;
				while (j<MAX_TRACKS && !availablechans[macOutput[machine]][j]) j++;
				if (j == MAX_TRACKS)
				{
					j = (unsigned int) (  (double)rand() * MAX_TRACKS /(((double)RAND_MAX) + 1.0 ));
				}
			}
			allocatedchans[channel][machine]=j;
			availablechans[macOutput[machine]][j]=false;
		}
		void MultiMachine::DeallocateVoice(int channel, int machine)
		{
			if ( allocatedchans[channel][machine] == -1 )
				return;
			availablechans[macOutput[machine]][allocatedchans[channel][machine]]= true;
			allocatedchans[channel][machine]=-1;
		}
		bool MultiMachine::playsTrack(const int track) const
		{
			return Machine::playsTrack(track);
		}

		//////////////////////////////////////////////////////////////////////////
		// NoteDuplicator
		DuplicatorMac::DuplicatorMac(int index):
			MultiMachine(index, 8)
		{
			_macIndex = index;
			_numPars = NUMMACHINES*2;
			_nCols = 2;
			_type = MACH_DUPLICATOR;
			_mode = MACHMODE_GENERATOR;
			strncpy(_editName, _psName, sizeof(_editName)-1);
			_editName[sizeof(_editName)-1]='\0';

			for (int i=0;i<NUMMACHINES;i++)
			{
				noteOffset[i]=0;
			}
		}
		void DuplicatorMac::Init()
		{
			MultiMachine::Init();
			for (int i=0;i<NUMMACHINES;i++)
			{
				noteOffset[i]=0;
			}
		}

		void DuplicatorMac::NewLine()
		{

		}

		void DuplicatorMac::CustomTick(int channel,int i, PatternEntry& pData)
		{
			if ( pData._note < notecommands::release )
			{
				int note = pData._note+noteOffset[i];
				if ( note>=notecommands::release) note=119;
				else if (note<0 ) note=0;				
				pData._note = static_cast<uint8_t>(note);
			}
		}
		bool DuplicatorMac::playsTrack(const int track) const
		{
			for (int i=0;i<NUMMACHINES;i++)
			{
				if (macOutput[i] != -1 && allocatedchans[track][i])
				{
					int alternateinst=-1;
					Machine * pmac = Global::song().GetMachineOfBus(macOutput[i], alternateinst);
					if (pmac != NULL) {
						return pmac->playsTrack(allocatedchans[track][i]);
					}
				}
			}
			return false;
		}

		void DuplicatorMac::GetParamName(int numparam,char *name)
		{
			if (numparam >=0 && numparam<NUMMACHINES)
			{
				sprintf(name,"Output Machine %d",numparam);
			} else if (numparam >=NUMMACHINES && numparam<NUMMACHINES*2) {
				sprintf(name,"Transpose %d",numparam-NUMMACHINES);
			}
			else name[0] = '\0';
		}
		void DuplicatorMac::GetParamRange(int numparam,int &minval,int &maxval)
		{
			if ( numparam < NUMMACHINES) { minval = -1; maxval = MAX_VIRTUALINSTS-1;}
			else if ( numparam < NUMMACHINES*2) { minval = -48; maxval = 48; }
		}
		int DuplicatorMac::GetParamValue(int numparam)
		{
			if (numparam >=0 && numparam<NUMMACHINES)
			{
				return macOutput[numparam];
			} else if (numparam >=NUMMACHINES && numparam <NUMMACHINES*2) {
				return noteOffset[numparam-NUMMACHINES];
			}
			else return 0;
		}
		void DuplicatorMac::GetParamValue(int numparam, char *parVal)
		{
			if (numparam >=0 && numparam <NUMMACHINES)
			{
				int alternateinst=-1;
				Machine * pmac = Global::song().GetMachineOfBus(macOutput[numparam], alternateinst);
				if (pmac != NULL) {
					if (alternateinst == -1) {
						sprintf(parVal,"%X -%s",macOutput[numparam],pmac->_editName);
					}
					else {
						sprintf(parVal,"%X -%s",macOutput[numparam], Global::song().GetVirtualMachineName(pmac,alternateinst).c_str());
					}
				}
				else if (macOutput[numparam] != -1) sprintf(parVal,"%X (none)",macOutput[numparam]);
				else sprintf(parVal,"(disabled)");

			} else if (numparam >= NUMMACHINES && numparam <NUMMACHINES*2) {
				if (noteOffset[numparam-NUMMACHINES] > 1 || noteOffset[numparam-NUMMACHINES] < -1)
					sprintf(parVal,"%d Halftones",noteOffset[numparam-NUMMACHINES]);
				else
					sprintf(parVal,"%d Halftone",noteOffset[numparam-NUMMACHINES]);
			}
			else parVal[0] = '\0';
		}
		bool DuplicatorMac::SetParameter(int numparam, int value)
		{
			if (numparam >=0 && numparam<NUMMACHINES)
			{
				macOutput[numparam]=value;
				return true;
			} else if (numparam >=NUMMACHINES && numparam<NUMMACHINES*2) {
				noteOffset[numparam-NUMMACHINES]=value;
				return true;
			}
			else return false;
		}

		int DuplicatorMac::GenerateAudio(int numSamples, bool measure_cpu_usage)
		{
			Standby(true);
			return numSamples;
		}
		bool DuplicatorMac::LoadSpecificChunk(RiffFile* pFile, int version)
		{
			UINT size;
			pFile->Read(&size, sizeof size); // size of this part params to load
			//TODO: endianess
			pFile->Read(&macOutput[0],NUMMACHINES*sizeof(short));
			pFile->Read(&noteOffset[0],NUMMACHINES*sizeof(short));
			return true;
		}

		void DuplicatorMac::SaveSpecificChunk(RiffFile* pFile)
		{
			UINT size = sizeof macOutput+ sizeof noteOffset;
			pFile->Write(&size, sizeof size); // size of this part params to save
			//TODO: endianess
			pFile->Write(&macOutput[0],NUMMACHINES*sizeof(short));
			pFile->Write(&noteOffset[0],NUMMACHINES*sizeof(short));
		}


		//////////////////////////////////////////////////////////////////////////
		// NoteDuplicator2
		DuplicatorMac2::DuplicatorMac2(int index):
			MultiMachine(index, 16), lowKey(16, 0), highKey(16, 119)
		{			
			_macIndex = index;
			_numPars = NUMMACHINES*4;
			_nCols = 4;
			_type = MACH_DUPLICATOR2;
			_mode = MACHMODE_GENERATOR;
			strncpy(_editName, _psName, sizeof(_editName)-1);
			_editName[sizeof(_editName)-1]='\0';
		}		

		void DuplicatorMac2::Tick(int channel,PatternEntry* pData) {
			if ( !_mute && !bisTicking) // Prevent possible loops of MultiMachines.
			{
				bisTicking=true;
				for (int i=0;i<NUMMACHINES;i++)
				{
					int alternateinst=-1;
					Machine * pmac = Global::song().GetMachineOfBus(macOutput[i], alternateinst);
					if (pmac != NULL) {
						PatternEntry pTemp = *pData;						
						int note = pData->_note;
						if (note>=lowKey[i]  && note<=highKey[i])
						{
							AllocateVoice(channel,i);						
							CustomTick(channel,i, pTemp);
							if (alternateinst != -1) pTemp._inst = alternateinst;
						}
						// this can happen if the parameter is the machine itself.
						if (pmac != this) 
						{
							if ((note>=lowKey[i]  && note<=highKey[i]) || (pTemp._note >= notecommands::release && allocatedchans[channel][i]!=-1)) {
								pmac->Tick(allocatedchans[channel][i],&pTemp);
							}
							if (pTemp._note >= notecommands::release)
							{
								DeallocateVoice(channel,i);
							}							
						} else
						{
							DeallocateVoice(channel,i);
						}
					}
				}
			}
			bisTicking=false;
		}

		void DuplicatorMac2::CustomTick(int channel,int i,PatternEntry& pData)
		{		
			if ( pData._note < notecommands::release )
			{
				int note = pData._note+noteOffset[i];
				if ( note>=notecommands::release) note=119;
				else if (note<0 ) note=0;				
				pData._note = static_cast<uint8_t>(note);
			}
		}

		bool DuplicatorMac2::playsTrack(const int track) const
		{
			for (int i=0;i<NUMMACHINES;i++)
			{
				if (macOutput[i] != -1 && allocatedchans[track][i])
				{
					int alternateinst=-1;
					Machine * pmac = Global::song().GetMachineOfBus(macOutput[i], alternateinst);
					if (pmac != NULL) {
						return pmac->playsTrack(allocatedchans[track][i]);
					}
				}
			}
			return false;
		}

		void DuplicatorMac2::GetParamName(int numparam,char *name)
		{
			if (numparam >=0 && numparam<NUMMACHINES)
			{
				sprintf(name,"Output Machine %d",numparam);
			} else if (numparam >=NUMMACHINES && numparam<NUMMACHINES*2) {
				sprintf(name,"Transpose %d",numparam-NUMMACHINES);
			} else if (numparam<NUMMACHINES*3) {
				sprintf(name,"Low Note %d",numparam-2*NUMMACHINES);
			} else if (numparam < NUMMACHINES*4) {
				sprintf(name,"High Note %d",numparam-3*NUMMACHINES);
			}
			else name[0] = '\0';
		}

		void DuplicatorMac2::GetParamRange(int numparam,int &minval,int &maxval)
		{
			if ( numparam < NUMMACHINES) { minval = -1; maxval = MAX_VIRTUALINSTS-1;}
			else if ( numparam < NUMMACHINES*2) { minval = -48; maxval = 48; }
			else { minval = notecommands::c0; maxval = notecommands::b9; }
		}

		int DuplicatorMac2::GetParamValue(int numparam)
		{
			if (numparam >=0 && numparam<NUMMACHINES)
			{
				return macOutput[numparam];
			} else if (numparam >=NUMMACHINES && numparam <NUMMACHINES*2) {
				return noteOffset[numparam-NUMMACHINES];
			} else if (numparam >=NUMMACHINES && numparam <NUMMACHINES*3) {
				return lowKey[numparam-2*NUMMACHINES];
			} else if (numparam >=NUMMACHINES && numparam <NUMMACHINES*4) {
				return highKey[numparam-3*NUMMACHINES];
			}

			else return 0;
		}

		void DuplicatorMac2::GetParamValue(int numparam, char *parVal)
		{
#if 0// !defined WINAMP_PLUGIN
			int a440 = (PsycleGlobal::conf().patView().showA440) ? -12 : 0;
			if (numparam >=0 && numparam <NUMMACHINES)
			{
				int alternateinst=-1;
				Machine * pmac = Global::song().GetMachineOfBus(macOutput[numparam], alternateinst);
				if (pmac != NULL) {
					if (alternateinst == -1) {
						sprintf(parVal,"%X -%s",macOutput[numparam],pmac->_editName);
					}
					else {
						sprintf(parVal,"%X -%s",macOutput[numparam], Global::song().GetVirtualMachineName(pmac,alternateinst).c_str());
					}
				}
				else if (macOutput[numparam] != -1) sprintf(parVal,"%X (none)",macOutput[numparam]);
				else sprintf(parVal,"(disabled)");

			} else if (numparam >= NUMMACHINES && numparam <NUMMACHINES*2) {
				if (noteOffset[numparam-NUMMACHINES] > 1 || noteOffset[numparam-NUMMACHINES] < -1)
					sprintf(parVal,"%d Halftones",noteOffset[numparam-NUMMACHINES]);
				else
					sprintf(parVal,"%d Halftone",noteOffset[numparam-NUMMACHINES]);
			} else if (numparam >= NUMMACHINES && numparam <NUMMACHINES*3) {
				char notes[12][3]={"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};
				sprintf(parVal,"%s%d",notes[(lowKey[numparam-2*NUMMACHINES])%12],(lowKey[numparam-2*NUMMACHINES]+a440)/12);
			} else if (numparam >= NUMMACHINES && numparam <NUMMACHINES*4) {
				char notes[12][3]={"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};
				sprintf(parVal,"%s%d",notes[(highKey[numparam-3*NUMMACHINES])%12],(highKey[numparam-3*NUMMACHINES]+a440)/12);
			}

			else parVal[0] = '\0';
#endif //#if !defined WINAMP_PLUGIN
		}

		bool DuplicatorMac2::SetParameter(int numparam, int value)
		{
			if (numparam >=0 && numparam<NUMMACHINES)
			{
				macOutput[numparam]=value;
				return true;
			} else if (numparam >=NUMMACHINES && numparam<NUMMACHINES*2) {
				noteOffset[numparam-NUMMACHINES]=value;
				return true;
			} else if (numparam >=NUMMACHINES && numparam<NUMMACHINES*3) {
				lowKey[numparam-2*NUMMACHINES]=value;
				return true;
			} if (numparam >=NUMMACHINES && numparam<NUMMACHINES*4) {
				highKey[numparam-3*NUMMACHINES]=value;
				return true;
			}
			else return false;
		}

		int DuplicatorMac2::GenerateAudio(int numSamples, bool measure_cpu_usage)
		{
			Standby(true);
			return numSamples;
		}
		bool DuplicatorMac2::LoadSpecificChunk(RiffFile* pFile, int version)
		{
			UINT size;
			pFile->Read(&size, sizeof size); // size of this part params to load
			//TODO: endianess
			pFile->Read(&macOutput[0],NUMMACHINES*sizeof(short));
			pFile->Read(&noteOffset[0],NUMMACHINES*sizeof(short));
			pFile->Read(&lowKey[0],NUMMACHINES*sizeof(short));
			pFile->Read(&highKey[0],NUMMACHINES*sizeof(short));
			return true;
		}

		void DuplicatorMac2::SaveSpecificChunk(RiffFile* pFile)
		{
			UINT size = sizeof macOutput+ sizeof noteOffset + sizeof lowKey + sizeof highKey;
			pFile->Write(&size, sizeof size); // size of this part params to save
			//TODO: endianess
			pFile->Write(&macOutput[0],NUMMACHINES*sizeof(short));
			pFile->Write(&noteOffset[0],NUMMACHINES*sizeof(short));
			pFile->Write(&lowKey[0],NUMMACHINES*sizeof(short));
			pFile->Write(&highKey[0],NUMMACHINES*sizeof(short));
		}

		//////////////////////////////////////////////////////////////////////////
		// AudioRecorder
		AudioRecorder::AudioRecorder(int index)
			:_initialized(false)
			,_captureidx(0)
			,_gainvol(1.0f)
		{
			_macIndex = index;
			_numPars = 0;
			_type = MACH_RECORDER;
			_mode = MACHMODE_GENERATOR;
			strncpy(_editName, _psName, sizeof(_editName)-1);
			_editName[sizeof(_editName)-1]='\0';
			InitializeSamplesVector();
		}
		AudioRecorder::~AudioRecorder()
		{
			AudioDriver &mydriver = *Global::configuration()._pOutputDriver;
			if (_initialized) mydriver.RemoveCapturePort(_captureidx);
		}
		void AudioRecorder::Init(void)
		{
			Machine::Init();
			if (!_initialized)
			{
				AudioDriver &mydriver = *Global::configuration()._pOutputDriver;
				_initialized = mydriver.AddCapturePort(_captureidx);
				strncpy(drivername,mydriver.settings().GetInfo()._psName,32);
			}
		}
		void AudioRecorder::ChangePort(int newport)
		{
			AudioDriver &mydriver = *Global::configuration()._pOutputDriver;
			if ( _initialized )
			{
				mydriver.Enable(false);
				mydriver.RemoveCapturePort(_captureidx);
				_initialized=false;
			}
			_initialized = mydriver.AddCapturePort(newport);
			_captureidx = newport;
			strncpy(drivername,mydriver.settings().GetInfo()._psName,32);
			mydriver.Enable(true);
		}
		int AudioRecorder::GenerateAudio(int numSamples, bool measure_cpu_usage)
		{
			if (!_mute &&_initialized)
			{
				AudioDriver &mydriver = *Global::configuration()._pOutputDriver;
				const AudioDriverInfo &myinfo = mydriver.settings().GetInfo();
				if(strcmp(myinfo._psName, drivername)) {
					_initialized = false;
					return numSamples;
				}
				mydriver.GetReadBuffers(_captureidx,&pleftorig,&prightorig,numSamples);
				if(pleftorig == NULL) {
					helpers::dsp::Clear(samplesV[0],numSamples);
					helpers::dsp::Clear(samplesV[1],numSamples);
				}
				else {
					helpers::dsp::MovMul(pleftorig,samplesV[0],numSamples,_gainvol);
					helpers::dsp::MovMul(prightorig,samplesV[1],numSamples,_gainvol);
				}
				UpdateVuAndStanbyFlag(numSamples);
			}
			else Standby(true);
			return numSamples;
		}
		bool AudioRecorder::LoadSpecificChunk(RiffFile * pFile, int version)
		{
			UINT size;
			int readcaptureidx;
			pFile->Read(&size, sizeof size); // size of this part params to load
			pFile->Read(&readcaptureidx,sizeof readcaptureidx);
			pFile->Read(&_gainvol,sizeof _gainvol);
			ChangePort(readcaptureidx);
			return true;
		}
		void AudioRecorder::SaveSpecificChunk(RiffFile * pFile)
		{
			UINT size = sizeof _captureidx+ sizeof _gainvol;
			pFile->Write(&size, sizeof size); // size of this part params to save
			pFile->Write(&_captureidx,sizeof _captureidx);
			pFile->Write(&_gainvol,sizeof _gainvol);
		}


		//////////////////////////////////////////////////////////////////////////
		// Mixer
		Mixer::Mixer(int id)
		{
			_macIndex = id;
			_numPars = 0x100;
			_type = MACH_MIXER;
			_mode = MACHMODE_FX;
			strncpy(_editName, _psName, sizeof(_editName)-1);
			_editName[sizeof(_editName)-1]='\0';
			InitializeSamplesVector();
			inputs_.reserve(MAX_CONNECTIONS);
			sends_.reserve(MAX_CONNECTIONS);
			//This reserve is especially important, since it contains the
			//addresses of Wire, which are referenced by from outWires.
			returns_.reserve(MAX_CONNECTIONS);
			InitialWorkState();
		}

		Mixer::~Mixer() throw()
		{
		}

		void Mixer::Init()
		{
			Machine::Init();

			if (inputs_.size() != 0) inputs_.resize(0);
			if (returns_.size() != 0) returns_.resize(0);
			master_.Init();

			solocolumn_=-1;
		}

		void Mixer::Tick( int channel,PatternEntry* pData)
		{
			if(pData->_note == notecommands::tweak)
			{
				int nv = (pData->_cmd<<8)+pData->_parameter;
				int param = translate_param(pData->_inst);
				if(param < GetNumParams()) {
					SetParameter(param,nv);
				}
			}
			else if(pData->_note == notecommands::tweakslide)
			{
				//\todo: Tweaks and tweak slides should not be a per-machine thing, but rather be player centric.
				// doing simply "tweak" for now..
				int nv = (pData->_cmd<<8)+pData->_parameter;
				int param = translate_param(pData->_inst);
				if(param < GetNumParams()) {
					SetParameter(param,nv);
				}
			}
		}
		void Mixer::recursive_process(unsigned int frames, bool measure_cpu_usage) {
			if(_mute || Bypass()) {
				recursive_process_deps(frames, true, measure_cpu_usage);
				return;
			}

			// Step One, do the usual work, except mixing all the inputs to a single stream.
			recursive_process_deps(frames, false, measure_cpu_usage);
			// Step Two, prepare input signals for the Send Fx, and make them work
			sched_returns_processed_curr=0;
			FxSend(frames, true, measure_cpu_usage);
			{ // Step Three, Mix the returns of the Send Fx's with the leveled input signal
				cpu_time_clock::time_point t0;
				if(measure_cpu_usage) t0 = cpu_time_clock::now();
				Mix(frames);
				helpers::dsp::Undenormalize(samplesV[0], samplesV[1], frames);
				UpdateVuAndStanbyFlag(frames);
				if(measure_cpu_usage) {
					cpu_time_clock::time_point const t1(cpu_time_clock::now());
					accumulate_processing_time(t1 - t0);
				}
			}

			recursive_processed_ = true;
		}

		void Mixer::FxSend(int numSamples, bool recurse, bool measure_cpu_usage)
		{
			// Note: Since mixer allows to route returns to other sends, this needs
			// to stop in non-recurse mode when the first of such routes is found
			sched_returns_processed_prev=sched_returns_processed_curr;
			int i;
			for (i=sched_returns_processed_curr; i<numsends(); i++)
			{
				if (SendValid(i))
				{
					Machine& pSendMachine = Send(i).GetDstMachine();
					if (!pSendMachine.recursive_processed_ && !pSendMachine.recursive_is_processing_)
					{ 
						cpu_time_clock::time_point t0;
						if(measure_cpu_usage) t0 = cpu_time_clock::now();
						bool soundready=false;
						// Mix all the inputs and route them to the send fx.
						{
							if ( solocolumn_ >=0 && solocolumn_ < MAX_CONNECTIONS)
							{
								int j = solocolumn_;
								if (inWires[j].Enabled() && !Channel(j).Mute() && !Channel(j).DryOnly() && (_sendvolpl[j][i] != 0.0f || _sendvolpr[j][i] != 0.0f ))
								{
									Machine& pInMachine = inWires[j].GetSrcMachine();
									if(!pInMachine._mute && !pInMachine.Standby())
									{
										MixInToSend(j,i, numSamples);
										soundready=true;
									}
								}
							}
							else for (int j=0; j<numinputs(); j++)
							{
								if (inWires[j].Enabled() && !Channel(j).Mute() && !Channel(j).DryOnly() && (_sendvolpl[j][i] != 0.0f || _sendvolpr[j][i] != 0.0f ))
								{
									Machine& pInMachine = inWires[j].GetSrcMachine();
									if(!pInMachine._mute && !pInMachine.Standby())
									{
										MixInToSend(j,i, numSamples);
										soundready=true;
									}
								}
							}
							for (int j=0; j<i; j++)
							{
								if (Return(j).IsValid() && Return(j).SendsTo(i) && !Return(j).Mute() && (mixvolretpl[j][i] != 0.0f || mixvolretpr[j][i] != 0.0f ))
								{
									Machine& pRetMachine = Return(j).GetWire().GetSrcMachine();
									// Note: Since mixer allows to route returns to other sends, this needs
									// to stop when the first of such routes is found.
									if(!recurse) 
									{
										if(!pRetMachine.sched_processed_) {
											sched_returns_processed_curr=i;
											return;
										}
									}
									if(!pRetMachine._mute && !pRetMachine.Standby())
									{
										MixReturnToSend(j,i, numSamples, Return(j).GetWire().GetVolume());
										soundready=true;
									}
								}
							}
							if (soundready) pSendMachine.Standby(false);
						}

						// tell the FX to work, now that the input is ready.
						if(recurse){
							//Time is only accumulated in recurse mode, because in shed mode the sched_process method already does it.
							if(measure_cpu_usage) {
								cpu_time_clock::time_point const t1(cpu_time_clock::now());
								accumulate_processing_time(t1 - t0);
							}
							Machine& pRetMachine = Return(i).GetWire().GetSrcMachine();
							recursive_is_processing_=true;
							pRetMachine.recursive_process(numSamples, measure_cpu_usage);
							recursive_is_processing_=false;
							/// pInMachines are verified in Machine::WorkNoMix, so we only check the returns.
							if(!pRetMachine.Standby())Standby(false);
						}
					}
				}
			}
			sched_returns_processed_curr=i;
		}
		void Mixer::MixInToSend(int inIdx,int outIdx, int numSamples)
		{
			const Wire::Mapping & map = sendMapping[inIdx][outIdx];
			const Machine &pInMachine = inWires[inIdx].GetSrcMachine();
			const Machine &pSendMachine = sends_[outIdx]->GetDstMachine();
			for(int i(0);i<map.size();i++) {
				Wire::PinConnection pin = map[i];
				helpers::dsp::Add(pInMachine.samplesV[pin.first], pSendMachine.samplesV[pin.second], numSamples, 
					(pin.first%2)?pInMachine._rVol*_sendvolpr[inIdx][outIdx]:pInMachine._lVol*_sendvolpl[inIdx][outIdx]);
				//math::erase_all_nans_infinities_and_denormals(pSendMachine.samplesV[pin.second], numSamples);
			}
		}
		void Mixer::MixReturnToSend(int inIdx,int outIdx, int numSamples, float wirevol)
		{
			const Wire::Mapping & map = sendMapping[MAX_CONNECTIONS+inIdx][outIdx];
			Machine &pRetMachine = Return(inIdx).GetWire().GetSrcMachine();
			Machine &pSendMachine = sends_[outIdx]->GetDstMachine();
			for(int i(0);i<map.size();i++) {
				Wire::PinConnection pin = map[i];
				helpers::dsp::Add(pRetMachine.samplesV[pin.first], pSendMachine.samplesV[pin.second], numSamples,
					(pin.first%2)?pRetMachine._rVol*mixvolretpr[inIdx][outIdx]*wirevol:pRetMachine._lVol*mixvolretpl[inIdx][outIdx]*wirevol);
				//math::erase_all_nans_infinities_and_denormals(pSendMachine.samplesV[pin.second], numSamples);
			}
		}
		void Mixer::Mix(int numSamples)
		{
			if ( master_.DryWetMix() > 0.0f)
			{
				if ( solocolumn_ >= MAX_CONNECTIONS)
				{
					int i= solocolumn_-MAX_CONNECTIONS;
					if (ReturnValid(i) && !Return(i).Mute() && Return(i).MasterSend() && (mixvolretpl[i][MAX_CONNECTIONS] != 0.0f || mixvolretpr[i][MAX_CONNECTIONS] != 0.0f ))
					{
						Machine& pRetMachine = Return(i).GetWire().GetSrcMachine();
						if(!pRetMachine._mute && !pRetMachine.Standby())
						{
							Return(i).GetWire().Mix(numSamples,pRetMachine._lVol*mixvolretpl[i][MAX_CONNECTIONS],pRetMachine._rVol*mixvolretpr[i][MAX_CONNECTIONS]);
						}
					}
				}
				else for (int i=0; i<numreturns(); i++)
				{
					if (ReturnValid(i) && !Return(i).Mute() && Return(i).MasterSend() && (mixvolretpl[i][MAX_CONNECTIONS] != 0.0f || mixvolretpr[i][MAX_CONNECTIONS] != 0.0f ))
					{
						Machine& pRetMachine = Return(i).GetWire().GetSrcMachine();
						if(!pRetMachine._mute && !pRetMachine.Standby())
						{
							Return(i).GetWire().Mix(numSamples,pRetMachine._lVol*mixvolretpl[i][MAX_CONNECTIONS],pRetMachine._rVol*mixvolretpr[i][MAX_CONNECTIONS]);
						}
					}
				}
			}
			if ( master_.DryWetMix() < 1.0f && solocolumn_ < MAX_CONNECTIONS)
			{
				if ( solocolumn_ >= 0)
				{
					int i = solocolumn_;
					if (ChannelValid(i) && !Channel(i).Mute() && !Channel(i).WetOnly() && (mixvolpl[i] != 0.0f || mixvolpr[i] != 0.0f ))
					{
						Machine& pInMachine = inWires[i].GetSrcMachine();
						if(!pInMachine._mute && !pInMachine.Standby())
						{
							inWires[i].Mix(numSamples,pInMachine._lVol*mixvolpl[i],pInMachine._rVol*mixvolpr[i]);
						}
					}
				}
				else for (int i=0; i<numinputs(); i++)
				{
					if (inWires[i].Enabled() && !Channel(i).Mute() && !Channel(i).WetOnly() && (mixvolpl[i] != 0.0f || mixvolpr[i] != 0.0f ))
					{
						Machine& pInMachine = inWires[i].GetSrcMachine();
						if(!pInMachine._mute && !pInMachine.Standby())
						{
							inWires[i].Mix(numSamples,pInMachine._lVol*mixvolpl[i],pInMachine._rVol*mixvolpr[i]);
						}
					}
				}
			}
		}

		/// tells the scheduler which machines to process before this one
		void Mixer::sched_inputs(sched_deps & result) const
		{
			if(mixed) {
				// step 1: receive all standard inputs
				for(int c(0); c < MAX_CONNECTIONS; ++c) if(inWires[c].Enabled()) {
					if (inWires[c].GetSrcMachine()._isMixerSend) {
						continue;
					}
					result.push_back(&inWires[c].GetSrcMachine());
				}
			} else {
				// step 2: get the output from return fx that have been processed
				for(unsigned int i = sched_returns_processed_prev; i < sched_returns_processed_curr; ++i) {
					if(Return(i).IsValid()) {
						Machine & returned = Return(i).GetWire().GetSrcMachine();
						result.push_back(&returned);
					}
				}
			}
		}

		/// tells the scheduler which machines may be processed after this one
		void Mixer::sched_outputs(sched_deps & result) const
		{
			if(!mixed) {
				// step 1: signal sent to sends. Identify them.
				for (unsigned int i=sched_returns_processed_prev; i<sched_returns_processed_curr; i++) if (SendValid(i)) {
					Machine & input = Send(i).GetDstMachine();
					result.push_back(&input);
				}
			} else {
				// step 2: everything done, our outputs will be the next connections
				Machine::sched_outputs(result);
			}
		}

		/// called by the scheduler to ask for the actual processing of the machine
		bool Mixer::sched_process(unsigned int frames, bool measure_cpu_usage)
		{
			cpu_time_clock::time_point t0;
			if(measure_cpu_usage) t0 = cpu_time_clock::now();

			if( sched_returns_processed_curr < numreturns()) {
				// Step 1: send audio to the effects
				mixed = false;
				if(!_mute && !Bypass()) {
					FxSend(frames, false, measure_cpu_usage);
				}
				else {
					sched_returns_processed_prev = sched_returns_processed_curr;
					sched_returns_processed_curr = numreturns();
				}
			}
			else {
				// step 2: mix input with return fx to generate output
				if(Bypass()) {
					for (int i=0; i<numinputs(); i++) if (inWires[i].Enabled())	{
						Machine & input_node = inWires[i].GetSrcMachine();
						if(!input_node._mute && !input_node.Standby()) {
							inWires[i].Mix(frames,input_node._lVol,input_node._rVol);
						}
					}
				}
				else if(!_mute) {
					Mix(frames);
				}
				int pins = GetNumInputPins();
				for(int i(0);i<pins;i++) {
					math::erase_all_nans_infinities_and_denormals(samplesV[i], frames);
				}
				UpdateVuAndStanbyFlag(frames);
				InitialWorkState();
				++processing_count_;
			}
			if (measure_cpu_usage) { 
				cpu_time_clock::time_point const t1(cpu_time_clock::now());
				accumulate_processing_time(t1 - t0);
			}
			return mixed;
		}

		float Mixer::GetWireVolume(int wireIndex)
		{
			if (wireIndex< MAX_CONNECTIONS) {
				return Machine::GetWireVolume(wireIndex);
			}
			else if ( ReturnValid(wireIndex-MAX_CONNECTIONS) ) {
				return Return(wireIndex-MAX_CONNECTIONS).GetWire().GetVolume();
			}
			return 0;
		}
		void Mixer::SetWireVolume(int wireIndex,float value)
		{
			if (wireIndex < MAX_CONNECTIONS) {
				Machine::SetWireVolume(wireIndex,value);
			}
			else if (ReturnValid(wireIndex-MAX_CONNECTIONS)) {
				Return(wireIndex-MAX_CONNECTIONS).GetWire().SetVolume(value);
			}
		}
		void Mixer::OnWireVolChanged(Wire & wire) 
		{
			int wireIndex = FindInputWire(&wire);
			if (wireIndex < MAX_CONNECTIONS) {
				Machine::OnWireVolChanged(wire);
				if (ChannelValid(wireIndex)) {
					RecalcChannel(wireIndex);
				}
			}
			else if (ReturnValid(wireIndex-MAX_CONNECTIONS)) {
				RecalcReturn(wireIndex-MAX_CONNECTIONS);
			}
		}
		int Mixer::FindInputWire(const Wire* pWireToFind) const
		{
			int wire = Machine::FindInputWire(pWireToFind);
			if (wire == -1) {
				for (int c=0; c<numreturns(); c++) {
					if (&Return(c).GetWire() == pWireToFind) {
						wire = c+MAX_CONNECTIONS;
						break;
					}
				}
			}
			return wire;
		}
		int Mixer::FindOutputWire(const Wire* pWireToFind) const
		{
			int wire = Machine::FindOutputWire(pWireToFind);
			if (wire == -1) {
				for (int c=0; c<numsends(); c++) {
					if (sends_[c] == pWireToFind) {
						wire = c+MAX_CONNECTIONS;
						break;
					}
				}
			}
			return wire;
		}

		int Mixer::FindInputWire(int macIndex) const
		{
			int ret=Machine::FindInputWire(macIndex);
			if ( ret == -1) {
				for (int c=0; c<numreturns(); c++) {
					if (Return(c).GetWire().GetSrcMachine()._macIndex == macIndex) {
						ret = c+MAX_CONNECTIONS;
						break;
					}
				}
			}
			return ret;
		}
		int Mixer::FindOutputWire(int macIndex) const
		{
			int ret = Machine::FindOutputWire(macIndex);
			if (ret == -1) {
				for (int c=0; c<numsends(); c++) {
					if (SendValid(c) && Send(c).GetDstMachine()._macIndex == macIndex) {
						ret = c+MAX_CONNECTIONS;
						break;
					}
				}
			}
			return ret;
		}
		int Mixer::GetFreeInputWire(int slottype) const
		{
			if ( slottype == 0) return Machine::GetFreeInputWire(0);
			else {
				// Get a free sendfx slot
				for(int c(0) ; c < MAX_CONNECTIONS ; ++c) {
					if(!ReturnValid(c)) return c+MAX_CONNECTIONS;
				}
				return -1;
			}
		}
		void Mixer::OnPinChange(Wire & wire, Wire::Mapping const & newMapping)
		{
			if (wire.GetSrcMachine()._macIndex == _macIndex) {
				int outWire = wire.GetSrcWireIndex();
				if (outWire >= MAX_CONNECTIONS) {
					outWire-=MAX_CONNECTIONS;
					for(int c=0;c<numinputs();++c) {
						RecalcInMapping(c, outWire, inWires[c].GetMapping(), newMapping);
					}
					for(int r=0;r<numreturns();++r) {
						if (Return(r).SendsTo(outWire)) {
							RecalcRetMapping(r, outWire, Return(r).GetWire().GetMapping(), newMapping);
						}
					}
				}
			}
			else if (wire.GetSrcMachine()._isMixerSend) {
				int wireIndex = wire.GetDstWireIndex();
				if (wireIndex >= MAX_CONNECTIONS) {
					wireIndex-=MAX_CONNECTIONS;
					for (int s(0);s<numsends();++s) {
						if (Return(wireIndex).SendsTo(s)) {
							RecalcRetMapping(wireIndex, s, newMapping, Send(s).GetMapping());
						}
					}
				}
			}
			else {
				int wireIndex = wire.GetDstWireIndex();
				if (wireIndex != -1) {
					for (int s(0);s<numsends();++s) {
						RecalcInMapping(wireIndex, s, newMapping, Send(s).GetMapping());
					}
				}
			}
		}
		void Mixer::OnOutputConnected(Wire & wire, int outType, int outWire)
		{
			if (outType == 0 ) {
				Machine::OnOutputConnected(wire, outType, outWire);
			}
			else {
				InsertSend(outWire,&wire);
				for(int i=0;i<numinputs();i++) {
					if(ChannelValid(i)) {
						RecalcSend(i,outWire);
					}
				}
			}
		}
		void Mixer::OnOutputDisconnected(Wire & wire)
		{
			int wireIndex = wire.GetSrcWireIndex();
			if (wireIndex < MAX_CONNECTIONS) {
				Machine::OnOutputDisconnected(wire);
			}
			else {
				wireIndex-=MAX_CONNECTIONS;
				sends_[wireIndex] = NULL;
				DiscardSend(wireIndex, wireIndex);
			}
		}
		void Mixer::OnInputConnected(Wire& wire)
		{
			int wireIndex = wire.GetDstWireIndex();
			if (wireIndex< MAX_CONNECTIONS)
			{
				if (!ChannelValid(wireIndex))
				{
					InsertChannel(wireIndex);
				}
				Machine::OnInputConnected(wire);
				RecalcChannel(wireIndex);
			}
			else
			{
				wireIndex-=MAX_CONNECTIONS;
				if ( !ReturnValid(wireIndex)) {
					InsertReturn(wireIndex);
				}
				Machine& send = wire.GetSrcMachine().SetMixerSendFlag(true);
				if (send.FindInputWire(_macIndex) == -1) {
					int i = send.GetFreeInputWire(0);
					send.inWires[i].ConnectSource(*this,1, wireIndex);
				}
				RecalcReturn(wireIndex);
			}
		}

		void Mixer::OnInputDisconnected(Wire & wire)
		{
			int wireIndex = wire.GetDstWireIndex();
			if ( wireIndex < MAX_CONNECTIONS)
			{
				Machine::OnInputDisconnected(wire);
				DiscardChannel(wireIndex, wireIndex);
			}
			else
			{
				wireIndex-=MAX_CONNECTIONS;
				wire.GetSrcMachine().SetMixerSendFlag(false);
				DiscardReturn(wireIndex, wireIndex);
				//Send can be invalid when deleting a send machine (since it deletes its inwires first)
				if(SendValid(wireIndex)) {
					Send(wireIndex).Disconnect();
					DiscardSend(wireIndex, wireIndex);
				}
			}
		}
		void Mixer::NotifyNewSendtoMixer(Machine & callerMac, Machine & senderMac)
		{
			// Mixer reached, set connections.
			for (int i=0;i < numreturns(); i++)
			{
				if ( ReturnValid(i) && Return(i).GetWire().GetSrcMachine()._macIndex == callerMac._macIndex)
				{
					int d = senderMac.GetFreeInputWire();
					if ( SendValid(i) ) {
						Wire copy = *sends_[i];
						sends_[i]->ChangeDest(senderMac.inWires[d]);
						senderMac.inWires[d].SetVolume(copy.GetVolume());
					}
					else {
						senderMac.inWires[d].ConnectSource(*this,1,i);
					}
					for (int ch(0);ch<numinputs();ch++)
					{
						if(ChannelValid(i)) {
							RecalcSend(ch,i);
						}
					}
					break;
				}
			}
		}


		void Mixer::DeleteWires()
		{
			Machine::DeleteWires();
			for(int w=0; w<numreturns(); w++)
			{
				// Checking send/return Wires
				if(Return(w).IsValid())
				{
					Return(w).GetWire().Disconnect();
				}
			}
			for(int w=0; w<numsends(); w++)
			{
				// Checking send/return Wires
				if(SendValid(w))
				{
					Send(w).Disconnect();
				}
			}
		}
		std::string Mixer::GetAudioInputName(int port)
		{
			std::string rettxt;
			if (port < chanmax )
			{	
				int i = port-chan1;
				rettxt = "Input ";
				if ( i < 9 ) rettxt += ('1'+i);
				else { rettxt += '1'; rettxt += ('0'+i-9); }
				return rettxt;
			}
			else if ( port < returnmax)
			{
				int i = port-return1;
				rettxt = "Return ";
				if ( i < 9 ) rettxt += ('1'+i);
				else { rettxt += '1'; rettxt += ('0'+i-9); }
				return rettxt;
			}
			rettxt = "-";
			return rettxt;
		}

		int Mixer::GetNumCols()
		{
			return 2+numinputs()+numreturns();
		}
		void Mixer::GetParamRange(int numparam, int &minval, int &maxval)
		{
			int channel=numparam/16;
			int param=numparam%16;
			if ( channel == 0)
			{
				if (param == 0){ minval=0; maxval=0x1000; }
				else if (param <= 12)  {
					if (!ChannelValid(param-1)) { minval=0; maxval=0; }
					else { minval=0; maxval=0x1000; }
				}
				else if (param == 13) { minval=0; maxval=0x100; }
				else if (param == 14) { minval=0; maxval=0x400; }
				else  { minval=0; maxval=0x100; }
			}
			else if (channel <= 12 )
			{
				if (!ChannelValid(channel-1)) { minval=0; maxval=0; }
				else if (param == 0) { minval=0; maxval=0x100; }
				else if (param <= 12) {
					if(!ReturnValid(param-1)) { minval=0; maxval=0; }
					else { minval=0; maxval=0x100; }
				}
				else if (param == 13) { minval=0; maxval=3; }
				else if (param == 14) { minval=0; maxval=0x400; }
				else  { minval=0; maxval=0x100; }
			}
			else if ( channel == 13)
			{
				if ( param > 12) { minval=0; maxval=0; }
				else if ( param == 0 ) { minval=0; maxval=24; }
				else if (!ReturnValid(param-1)) { minval=0; maxval=0; }
				else { minval=0; maxval=(1<<14)-1; }
			}
			else if ( channel == 14)
			{
				if ( param == 0 || param > 12) { minval=0; maxval=0; }
				else if (!ReturnValid(param-1)) { minval=0; maxval=0; }
				else { minval=0; maxval=0x1000; }
			}
			else if ( channel == 15)
			{
				if ( param == 0 || param > 12) { minval=0; maxval=0; }
				else if (!ReturnValid(param-1)) { minval=0; maxval=0; }
				else { minval=0; maxval=0x100; }
			}
			else { minval=0; maxval=0; }
		}
		void Mixer::GetParamName(int numparam,char *name)
		{
			int channel=numparam/16;
			int param=numparam%16;
			if ( channel == 0)
			{
				if (param == 0) strcpy(name,"Master - Output");
				else if (param <= 12) sprintf(name,"Channel %d - Volume",param);
				else if (param == 13) strcpy(name,"Master - Mix");
				else if (param == 14) strcpy(name,"Master - Gain");
				else strcpy(name,"Master - Panning");
			}
			else if (channel <= 12 )
			{
				if (param == 0) sprintf(name,"Channel %d - Dry mix",channel);
				else if (param <= 12) sprintf(name,"Chan %d Send %d - Amount",channel,param);
				else if (param == 13) sprintf(name,"Channel %d  - Mix type",channel);
				else if (param == 14) sprintf(name,"Channel %d - Gain",channel);
				else sprintf(name,"Channel %d - Panning",channel);
			}
			else if ( channel == 13)
			{
				if (param > 12) strcpy(name,"");
				else if (param == 0) strcpy(name,"Set Channel Solo");
				else sprintf(name,"Return %d - Route Array",param);
			}
			else if ( channel == 14)
			{
				if ( param == 0 || param > 12) strcpy(name,"");
				else sprintf(name,"Return %d - Volume",param);
			}
			else if ( channel == 15)
			{
				if ( param == 0 || param > 12) strcpy(name,"");
				else sprintf(name,"Return %d - Panning",param);
			}
			else strcpy(name,"");
		}

		int Mixer::GetParamValue(int numparam)
		{
			int channel=numparam/16;
			int param=numparam%16;
			if ( channel == 0)
			{
				if (param == 0)
				{
					float dbs = helpers::dsp::dB(master_.Volume());
					return (dbs+96.0f)*42.67; // *(0x1000 / 96.0f)
				}
				else if (param <= 12)
				{
					if (!ChannelValid(param-1)) return 0;
					else {
						float dbs = helpers::dsp::dB(Channel(param-1).Volume());
						return (dbs+96.0f)*42.67; // *(0x1000 / 96.0f)
					}
				}
				else if (param == 13) return master_.DryWetMix()*0x100;
				else if (param == 14) return master_.Gain()*0x100;
				else return _panning*2;
			}
			else if (channel <= 12 )
			{
				if (!ChannelValid(channel-1)) return 0;
				else if (param == 0) return Channel(channel-1).DryMix()*0x100;
				else if (param <= 12)
				{
					if ( !ReturnValid(param-1)) return 0;
					else return Channel(channel-1).SendVol(param-1)*0x100;
				}
				else if (param == 13)
				{
					if (Channel(channel-1).Mute()) return 3;
					else if (Channel(channel-1).WetOnly()) return 2;
					else if (Channel(channel-1).DryOnly()) return 1;
					return 0;
				}
				else if (param == 14) { float val; GetWireVolume(channel-1,val); return val*0x100; }
				else return Channel(channel-1).Panning()*0x100;
			}
			else if ( channel == 13)
			{
				if ( param > 12) return 0;
				else if (param == 0 ) return solocolumn_+1;
				else if ( !ReturnValid(param-1)) return 0;
				else 
				{
					int val(0);
					if (Return(param-1).Mute()) val|=1;
					for (int i(0);i<numreturns();i++)
					{
						if (Return(param-1).SendsTo(i)) val|=(2<<i);
					}
					if (Return(param-1).MasterSend()) val|=(1<<13);
					return val;
				}
			}
			else if ( channel == 14)
			{
				if ( param == 0 || param > 12) return 0;
				else if ( !ReturnValid(param-1)) return 0;
				else
				{
					float dbs = helpers::dsp::dB(Return(param-1).Volume());
					return (dbs+96.0f)*42.67; // *(0x1000 / 96.0f)
				}
			}
			else if ( channel == 15)
			{
				if ( param == 0 || param > 12) return 0;
				else if ( !ReturnValid(param-1)) return 0;
				else return Return(param-1).Panning()*0x100;
			}
			else return 0;
		}

		void Mixer::GetParamValue(int numparam, char *parVal)
		{
			int channel=numparam/16;
			int param=numparam%16;
			parVal[0]='\0';
			if ( channel == 0)
			{
				if (param == 0)
				{ 
					if (master_.Volume() < 0.00002f ) strcpy(parVal,"-inf");
					else
					{
						float dbs = helpers::dsp::dB(master_.Volume());
						sprintf(parVal,"%.01fdB",dbs);
					}
				}
				else if (param <= 12)
				{ 
					if (!ChannelValid(param-1)) return;
					else if (Channel(param-1).Volume() < 0.00002f ) strcpy(parVal,"-inf");
					else
					{
						float dbs = helpers::dsp::dB(Channel(param-1).Volume());
						sprintf(parVal,"%.01fdB",dbs);
					}
				}
				else if (param == 13)
				{
					if (master_.DryWetMix() == 0.0f) strcpy(parVal,"Dry");
					else if (master_.DryWetMix() == 1.0f) strcpy(parVal,"Wet");
					else sprintf(parVal,"%.0f%%",master_.DryWetMix()*100.0f);
				}
				else if (param == 14)
				{
					float val = master_.Gain();
					float dbs = (((val>0.0f)?helpers::dsp::dB(val):-100.0f));
					sprintf(parVal,"%.01fdB",dbs);
				}
				else 
				{
					if (_panning == 0) strcpy(parVal,"left");
					else if (_panning == 128) strcpy(parVal,"right");
					else if (_panning == 64) strcpy(parVal,"center");
					else sprintf(parVal,"%.0f%%",_panning*0.78125f);
				}
			}
			else if (channel <= 12 )
			{
				if (!ChannelValid(channel-1)) return;
				else if (param == 0)
				{
					if (Channel(channel-1).DryMix() == 0.0f) strcpy(parVal,"Off");
					else sprintf(parVal,"%.0f%%",Channel(channel-1).DryMix()*100.0f);
				}
				else if (param <= 12)
				{
					if ( !ReturnValid(param-1)) return;
					else if (Channel(channel-1).SendVol(param-1) == 0.0f) strcpy(parVal,"Off");
					else sprintf(parVal,"%.0f%%",Channel(channel-1).SendVol(param-1)*100.0f);
				}
				else if (param == 13)
				{
					parVal[0]= (Channel(channel-1).DryOnly())?'D':' ';
					parVal[1]= (Channel(channel-1).WetOnly())?'W':' ';
					parVal[2]= (Channel(channel-1).Mute())?'M':' ';
					parVal[3]='\0';
				}
				else if (param == 14) 
				{
					float val;
					GetWireVolume(channel-1,val);
					float dbs = (((val>0.0f)?helpers::dsp::dB(val):-100.0f));
					sprintf(parVal,"%.01fdB",dbs);
				}
				else
				{
					if (Channel(channel-1).Panning()== 0.0f) strcpy(parVal,"left");
					else if (Channel(channel-1).Panning()== 1.0f) strcpy(parVal,"right");
					else if (Channel(channel-1).Panning()== 0.5f) strcpy(parVal,"center");
					else sprintf(parVal,"%.0f%%",Channel(channel-1).Panning()*100.0f);
				}
			}
			else if ( channel == 13)
			{
				if ( param > 12) return;
				else if (param == 0 ){ sprintf(parVal,"%d",solocolumn_+1); }
				else if ( !ReturnValid(param-1))  return;
				else 
				{
					parVal[0]= (Return(param-1).Mute())?'M':' ';
					parVal[1]= (Return(param-1).SendsTo(0))?'1':' ';
					parVal[2]= (Return(param-1).SendsTo(1))?'2':' ';
					parVal[3]= (Return(param-1).SendsTo(2))?'3':' ';
					parVal[4]= (Return(param-1).SendsTo(3))?'4':' ';
					parVal[5]= (Return(param-1).SendsTo(4))?'5':' ';
					parVal[6]= (Return(param-1).SendsTo(5))?'6':' ';
					parVal[7]= (Return(param-1).SendsTo(6))?'7':' ';
					parVal[8]= (Return(param-1).SendsTo(7))?'8':' ';
					parVal[9]= (Return(param-1).SendsTo(8))?'9':' ';
					parVal[10]= (Return(param-1).SendsTo(9))?'A':' ';
					parVal[11]= (Return(param-1).SendsTo(10))?'B':' ';
					parVal[12]= (Return(param-1).SendsTo(11))?'C':' ';
					parVal[13]= (Return(param-1).MasterSend())?'O':' ';
					parVal[14]= '\0';
				}
			}
			else if ( channel == 14)
			{
				if ( param == 0 || param > 12) return;
				else if ( !ReturnValid(param-1)) return;
				else if (Return(param-1).Volume() < 0.00002f ) strcpy(parVal,"-inf");
				else
				{ 
					float dbs = helpers::dsp::dB(Return(param-1).Volume());
					sprintf(parVal,"%.01fdB",dbs);
				}
			}
			else if ( channel == 15)
			{
				if ( param == 0 || param > 12) return;
				else if ( !ReturnValid(param-1)) return;
				else
				{
					if (Return(param-1).Panning()== 0.0f) strcpy(parVal,"left");
					else if (Return(param-1).Panning()== 1.0f) strcpy(parVal,"right");
					else if (Return(param-1).Panning()== 0.5f) strcpy(parVal,"center");
					else sprintf(parVal,"%.0f%%",Return(param-1).Panning()*100.0f);
				}
			}
		}

		bool Mixer::SetParameter(int numparam, int value)
		{
			int channel=numparam/16;
			int param=numparam%16;
			if ( channel == 0)
			{
				if (param == 0)
				{
					if ( value >= 0x1000) master_.Volume()=1.0f;
					else if ( value == 0) master_.Volume()=0.0f;
					else
					{
						float dbs = (value/42.67f)-96.0f;
						master_.Volume() = helpers::dsp::dB2Amp(dbs);
					}
					RecalcMaster();
				}
				else if (param <= 12)
				{
					if (!ChannelValid(param-1)) return false;
					else if ( value >= 0x1000) Channel(param-1).Volume()=1.0f;
					else if ( value == 0) Channel(param-1).Volume()=0.0f;
					else
					{
						float dbs = (value/42.67f)-96.0f;
						Channel(param-1).Volume() = helpers::dsp::dB2Amp(dbs);
					}
					RecalcChannel(param-1);
				}
				else if (param == 13) { master_.DryWetMix() = (value==256)?1.0f:((value&0xFF)/256.0f); RecalcMaster(); }
				else if (param == 14) { master_.Gain() = (value>=1024)?4.0f:((value&0x3FF)/256.0f); RecalcMaster(); }
				else SetPan(value>>1);
				return true;
			}
			else if (channel <= 12 )
			{
				if (!ChannelValid(channel-1)) return false;
				if (param == 0) { Channel(channel-1).DryMix() = (value==256)?1.0f:((value&0xFF)/256.0f); RecalcChannel(channel-1); }
				else if (param <= 12) {
					 if(!SendValid(param-1)) return false;
					Channel(channel-1).SendVol(param-1) = (value==256)?1.0f:((value&0xFF)/256.0f); RecalcSend(channel-1,param-1);
				} 
				else if (param == 13)
				{
					Channel(channel-1).Mute() = (value == 3)?true:false;
					Channel(channel-1).WetOnly() = (value==2)?true:false;
					Channel(channel-1).DryOnly() = (value==1)?true:false;
				}
				else if (param == 14) { float val=(value>=1024)?4.0f:((value&0x3FF)/256.0f); SetWireVolume(channel-1,val); RecalcChannel(channel-1); }
				else { Channel(channel-1).Panning() = (value==256)?1.0f:((value&0xFF)/256.0f); RecalcChannel(channel-1); }
				return true;
			}
			else if ( channel == 13)
			{
				if ( param > 12) return false;
				else if (param == 0) solocolumn_ = (value<24)?value-1:23;
				else if (!ReturnValid(param-1)) return false;
				else 
				{
					Return(param-1).Mute() = (value&1)?true:false;
					for (int i(param);i<numreturns();i++)
					{
						Return(param-1).SendsTo(i,(value&(2<<i))?true:false);
					}
					Return(param-1).MasterSend() = (value&(1<<13))?true:false;
					RecalcReturn(param-1);
				}
				return true;
			}
			else if ( channel == 14)
			{
				if ( param == 0 || param > 12) return false;
				else if (!ReturnValid(param-1)) return false;
				else
				{
					if ( value >= 0x1000) Return(param-1).Volume()=1.0f;
					else if ( value == 0) Return(param-1).Volume()=0.0f;
					else
					{
						float dbs = (value/42.67f)-96.0f;
						Return(param-1).Volume() = helpers::dsp::dB2Amp(dbs);
					}
					RecalcReturn(param-1);
				}
				return true;
			}
			else if ( channel == 15)
			{
				if ( param == 0 || param > 12) return false;
				else if (!ReturnValid(param-1)) return false;
				else { Return(param-1).Panning() = (value==256)?1.0f:((value&0xFF)/256.0f); RecalcReturn(param-1); }
				return true;
			}
			return false;
		}

		float Mixer::VuChan(int idx)
		{
			if ( inWires[idx].Enabled() ) 
			{
				//Note that since volumeDisplay is integer, when using positive gain,
				//the result can visually differ from the calculated one
				float vol;
				GetWireVolume(idx,vol);
				vol*=Channel(idx).Volume();
				int temp(round<int>(50.0f * std::log10f(vol)));
				return (inWires[idx].GetSrcMachine()._volumeDisplay+temp)/97.0f;
			}
			return 0.0f;
		}

		float Mixer::VuSend(int idx)
		{
			if ( SendValid(idx) )
			{
				//Note that since volumeDisplay is integer, when using positive gain,
				// the result can visually differ from the calculated one
				float vol;
				GetWireVolume(idx+MAX_CONNECTIONS,vol);
				vol *= Return(idx).Volume();
				int temp(round<int>(50.0f * std::log10(vol)));
				return (Send(idx).GetDstMachine()._volumeDisplay+temp)/97.0f;
			}
			return 0.0f;
		}
		void Mixer::InsertChannel(int idx,InputChannel*input)
		{
			assert(idx<MAX_CONNECTIONS);
			if ( idx >= numinputs())
			{
				for(int i=numinputs(); i<idx; ++i)
				{
					inputs_.push_back(InputChannel(numsends()));
				}
				if (input) inputs_.push_back(*input);
				else inputs_.push_back(InputChannel(numsends()));
			}
			else if (input) inputs_[idx]=*input;
			else { inputs_[idx].Init(); inputs_[idx].ResizeTo(numsends()); }
		}
		void Mixer::InsertReturn(int idx,ReturnChannel* retchan)
		{
			assert(idx<MAX_CONNECTIONS);
			if ( idx >= numreturns())
			{
				for(int i=numreturns(); i<idx; ++i)
				{
					returns_.push_back(ReturnChannel(this,numsends()));
				}
				if (retchan) returns_.push_back(*retchan);
				else returns_.push_back(ReturnChannel(this,numsends()));
			}
			else if (retchan) returns_[idx]=*retchan;
			else { returns_[idx].Init(); returns_[idx].ResizeTo(numsends());}
		}

		void Mixer::InsertSend(int idx,Wire* swire)
		{
			assert(idx<MAX_CONNECTIONS);
			if ( idx >= numsends())
			{
				for(int i=numsends(); i<idx; ++i)
				{
					sends_.push_back(NULL);
				}
				sends_.push_back(swire);
				for(int i=0; i<numinputs(); ++i)
				{
					Channel(i).ResizeTo(numsends());
				}
				for(int i=0; i<numreturns(); ++i)
				{
					Return(i).ResizeTo(numsends());
				}
			}
			else sends_[idx]=swire;
		}
		void Mixer::DiscardChannel(int idx, int idxdeleting)
		{
			assert(idx<MAX_CONNECTIONS);
			if (idx!=numinputs()-1) return;
			int i;
			for (i = idx; i >= 0; i--)
			{
				if (i != idxdeleting && inWires[i].Enabled())
					break;
			}
			inputs_.resize(i+1);
		}
		void Mixer::DiscardReturn(int idx, int idxdeleting)
		{
			assert(idx<MAX_CONNECTIONS);
			if (idx!=numreturns()-1) return;
			int i;
			for (i = idx; i >= 0; i--)
			{
				if (i != idxdeleting && Return(i).IsValid())
					break;
			}
			returns_.resize(i+1);
		}
		void Mixer::DiscardSend(int idx, int idxdeleting)
		{
			assert(idx<MAX_CONNECTIONS);
			if (idx!=numsends()-1) return;
			int i;
			for (i = idx; i >= 0; i--)
			{
				if (i != idxdeleting && SendValid(i))
					break;
			}
			sends_.resize(i+1);
		}


		void Mixer::ExchangeChans(int chann1,int chann2)
		{
			ExchangeInputWires(chann1,chann2);
			InputChannel tmp = inputs_[chann1];
			inputs_[chann1] = inputs_[chann2];
			inputs_[chann2] = tmp;
			RecalcChannel(chann1);
			RecalcChannel(chann2);
			//The following line autocleans the left-out channels at the end.
			DiscardChannel(numinputs()-1); 
		}
		void Mixer::ExchangeReturns(int chann1,int chann2)
		{
			//We have to exchange the outWire pointers of the source machine, since we will copy the wire contents from one index to the other.
			//it is done in two steps because getSrcWireIndex() navigates in order to know the index.
			Wire & wire1 = Return(chann1).GetWire();
			Wire & wire2 = Return(chann2).GetWire();
			int idx1 = (wire1.Enabled()) ? wire1.GetSrcWireIndex() : 0;
			int idx2 = (wire2.Enabled()) ? wire2.GetSrcWireIndex() : 0;
			if (wire1.Enabled()) {
				wire1.GetSrcMachine().outWires[idx1] = &wire2;
			}
			if (wire2.Enabled()) {
				wire2.GetSrcMachine().outWires[idx2] = &wire1;
			}
			//copy data from one channel to the other, including wire information
			ReturnChannel tmp(returns_[chann1]);
			returns_[chann1] = returns_[chann2];
			returns_[chann2] = tmp;
			//Exchange the sends too. There is no need to exchange the inwires of the dest machine since the outwire
			//index is not stored in the wire and the rest of the information remains equal.
			Wire* tmp2 = sends_[chann1];
			sends_[chann1] = sends_[chann2];
			sends_[chann2] = tmp2;
			for (int i(0); i < numinputs(); ++i)
			{
				//Exchange and recalculate sendVol
				Channel(i).ExchangeSends(chann1,chann2);
				if(SendValid(chann1)) RecalcSend(i,chann1);
				if(SendValid(chann2)) RecalcSend(i,chann2);
			}
			for (int i(0); i < numreturns(); ++i)
			{
				//exchange sendsTo (boolean)
				Return(i).ExchangeSends(chann1,chann2);
			}
			//Recalculating all because of the return-to-send which can apply to other returns
			//For example, exchanging returns 2 and 3 on Example - Mixerdemo.psy
			for (int i(0); i < numreturns(); ++i)
			{
				RecalcReturn(i);
			}
			//The following lines autoclean the left-out send/returns at the end.
			DiscardReturn(numreturns()-1);
			DiscardSend(numsends()-1);
		}
		void Mixer::ResizeTo(int inputs, int sends)
		{
			inputs_.resize(inputs);
			returns_.resize(sends);
			sends_.resize(sends);
			for(int i=0; i<inputs; ++i)
			{
				Channel(i).ResizeTo(sends);
			}
			for(int i=0; i<sends; ++i)
			{
				Return(i).ResizeTo(sends);
			}
		}
		void Mixer::RecalcMaster()
		{
			for (int i(0);i<numinputs();i++)
			{
				if (inWires[i].Enabled()) RecalcChannel(i);
			}
			for (int i(0);i<numreturns();i++)
			{
				if (Return(i).IsValid()) RecalcReturn(i);
			}
		}
		void Mixer::RecalcReturn(int idx)
		{
			assert(idx<MAX_CONNECTIONS);
			const ReturnChannel & ret = Return(idx);
			//Volumes/mapping for return redirected to send
			for(int send=0; send < numsends(); ++send) {
				if (ret.SendsTo(send) && SendValid(send)) {
					mixvolretpl[idx][send] = mixvolretpr[idx][send] = ret.Volume()*( Send(send).GetDstMachine().GetAudioRange() /ret.GetWire().GetSrcMachine().GetAudioRange());
					if (ret.Panning() >= 0.5f ) {
						mixvolretpl[idx][send] *= (1.0f-ret.Panning())*2.0f;
					}
					else mixvolretpr[idx][send] *= (ret.Panning())*2.0f;
					RecalcRetMapping(idx, send, ret.GetWire().GetMapping(), Send(send).GetMapping());
				}
			}
			//Volume for return to master
			float wet = master_.Volume()*master_.Gain();
			if (master_.DryWetMix() < 0.5f )
			{
				wet *= (master_.DryWetMix())*2.0f;
			}

			mixvolretpl[idx][MAX_CONNECTIONS] = mixvolretpr[idx][MAX_CONNECTIONS] = ret.Volume()*wet;
			if (ret.Panning() >= 0.5f )
			{
				mixvolretpl[idx][MAX_CONNECTIONS] *= (1.0f-ret.Panning())*2.0f;
			}
			else mixvolretpr[idx][MAX_CONNECTIONS] *= (ret.Panning())*2.0f;
		}
		void Mixer::RecalcChannel(int idx)
		{
			assert(idx<MAX_CONNECTIONS);
			const InputChannel & chan = Channel(idx);

			float dry = master_.Volume()*master_.Gain();
			if (master_.DryWetMix() > 0.5f )
			{
				dry *= (1.0f-master_.DryWetMix())*2.0f;
			}
			//Volumes from in to master.
			mixvolpl[idx] = mixvolpr[idx] = chan.Volume()*chan.DryMix()*dry;
			if (chan.Panning() >= 0.5f )
			{
				mixvolpl[idx] *= (1.0f-chan.Panning())*2.0f;
			}
			else mixvolpr[idx] *= (chan.Panning())*2.0f;
			//Volumes and mapping from in to sends.
			for (int i(0);i<numsends();i++) {
				if (SendValid(i)) {
					RecalcSend(idx,i);
				}
			}
		}
		void Mixer::RecalcSend(int chan,int send)
		{
			assert(chan<MAX_CONNECTIONS);
			assert(send<MAX_CONNECTIONS);
			float val;
			GetWireVolume(chan,val);
			const InputChannel &chann = Channel(chan);

			_sendvolpl[chan][send] =  _sendvolpr[chan][send] = chann.Volume()*val*chann.SendVol(send)/(Send(send).GetVolMultiplier()*inWires[chan].GetVolMultiplier());
			if (chann.Panning() >= 0.5f )
			{
				_sendvolpl[chan][send] *= (1.0f-chann.Panning())*2.0f;
			}
			else _sendvolpr[chan][send] *= (chann.Panning())*2.0f;
			RecalcInMapping(chan, send, inWires[chan].GetMapping(), Send(send).GetMapping());
		}

		void Mixer::RecalcInMapping(int inIdx, int sendIdx, const Wire::Mapping& inMap, const Wire::Mapping& sendMap)
		{
			Wire::Mapping& newMap = sendMapping[inIdx][sendIdx];
			newMap.clear();
			for(int i(0);i<inMap.size();i++) {
				for (int k(0);k<sendMap.size();k++)
				{
					if(inMap[i].second == sendMap[k].first) {
						newMap.push_back(Wire::PinConnection(inMap[i].first,sendMap[k].second));
					}
				}
			}
		}
		void Mixer::RecalcRetMapping(int retIdx, int sendIdx, const Wire::Mapping& retMap, const Wire::Mapping& sendMap)
		{
			Wire::Mapping& newMap = sendMapping[MAX_CONNECTIONS+retIdx][sendIdx];
			newMap.clear();
			for(int i(0);i<retMap.size();i++) {
				for (int k(0);k<sendMap.size();k++)
				{
					if(retMap[i].second == sendMap[k].first) {
						newMap.push_back(Wire::PinConnection(retMap[i].first,sendMap[k].second));
					}
				}
			}
		}

		bool Mixer::LoadSpecificChunk(RiffFile* pFile, int version)
		{
			uint32_t filesize;
			pFile->Read(&filesize,sizeof(filesize));

			pFile->Read(&solocolumn_,sizeof(solocolumn_));
			pFile->Read(&master_.Volume(),sizeof(float));
			pFile->Read(&master_.Gain(),sizeof(float));
			pFile->Read(&master_.DryWetMix(),sizeof(float));

			int numins(0),numrets(0);
			pFile->Read(&numins,sizeof(int));
			pFile->Read(&numrets,sizeof(int));
			if ( numins >0 ) InsertChannel(numins-1);
			if ( numrets >0 ) InsertReturn(numrets-1);
			if ( numrets >0 ) InsertSend(numrets-1, NULL);
			for (int i(0);i<numinputs();i++)
			{
				for (int j(0);j<numrets;j++)
				{
					float send(0.0f);
					pFile->Read(&send,sizeof(float));
					Channel(i).SendVol(j)=send;
				}
				pFile->Read(&Channel(i).Volume(),sizeof(float));
				pFile->Read(&Channel(i).Panning(),sizeof(float));
				pFile->Read(&Channel(i).DryMix(),sizeof(float));
				pFile->Read(&Channel(i).Mute(),sizeof(bool));
				pFile->Read(&Channel(i).DryOnly(),sizeof(bool));
				pFile->Read(&Channel(i).WetOnly(),sizeof(bool));
			}
			legacyReturn_.resize(numrets);
			legacySend_.resize(numrets);
			for (int i(0);i<numrets;i++)
			{
				{
					LegacyWire& leg = legacyReturn_[i];
					pFile->Read(&leg._inputMachine,sizeof(leg._inputMachine));	// Incoming (Return) connections Machine number
					pFile->Read(&leg._inputConVol,sizeof(leg._inputConVol));	// /volume value for the current return wire. Range 0.0..1.0. (As opposed to the standard wires)
					pFile->Read(&leg._wireMultiplier,sizeof(leg._wireMultiplier));	// Ignore. (value to divide returnVolume for work. The reason is because natives output at -32768.0f..32768.0f range )
					if (leg._inputConVol > 8.0f) { //bugfix on 1.10.1 alpha
						leg._inputConVol /= 32768.f;
					}
				}
				{
					LegacyWire& leg2 = legacySend_[i];
					pFile->Read(&leg2._inputMachine,sizeof(leg2._inputMachine));	// Outgoing (Send) connections Machine number
					pFile->Read(&leg2._inputConVol,sizeof(leg2._inputConVol));	//volume value for the current send wire. Range 0.0..1.0. (As opposed to the standard wires)
					pFile->Read(&leg2._wireMultiplier,sizeof(leg2._wireMultiplier));	// Ignore. (value to divide returnVolume for work. The reason is because natives output at -32768.0f..32768.0f range )
					if (leg2._inputConVol > 0.f && leg2._inputConVol < 0.0002f) { //bugfix on 1.10.1 alpha
						leg2._inputConVol *= 32768.f;
					}
				}				
				for (int j(0);j<numrets;j++)
				{
					bool send(false);
					pFile->Read(&send,sizeof(bool));
					Return(i).SendsTo(j,send);
				}
				pFile->Read(&Return(i).MasterSend(),sizeof(bool));
				pFile->Read(&Return(i).Volume(),sizeof(float));
				pFile->Read(&Return(i).Panning(),sizeof(float));
				pFile->Read(&Return(i).Mute(),sizeof(bool));
			}
			return true;
		}

		void Mixer::SaveSpecificChunk(RiffFile* pFile)
		{
			uint32_t size(sizeof(solocolumn_)+sizeof(master_)+2*sizeof(int));
			size+=(3*sizeof(float)+3*sizeof(bool)+numsends()*sizeof(float))*numinputs();
			size+=(2*sizeof(float)+2*sizeof(bool)+numsends()*sizeof(bool)+2*sizeof(float)+sizeof(int))*numreturns();
			size+=(2*sizeof(float)+sizeof(int))*numsends();
			pFile->Write(&size,sizeof(size));

			pFile->Write(&solocolumn_,sizeof(solocolumn_));
			pFile->Write(&master_.Volume(),sizeof(float));
			pFile->Write(&master_.Gain(),sizeof(float));
			pFile->Write(&master_.DryWetMix(),sizeof(float));

			const int numins = numinputs();
			const int numrets = numreturns();
			pFile->Write(&numins,sizeof(int));
			pFile->Write(&numrets,sizeof(int));
			for (int i(0);i<numinputs();i++)
			{
				for (int j(0);j<numsends();j++)
				{
					pFile->Write(&Channel(i).SendVol(j),sizeof(float));
				}
				pFile->Write(&Channel(i).Volume(),sizeof(float));
				pFile->Write(&Channel(i).Panning(),sizeof(float));
				pFile->Write(&Channel(i).DryMix(),sizeof(float));
				pFile->Write(&Channel(i).Mute(),sizeof(bool));
				pFile->Write(&Channel(i).DryOnly(),sizeof(bool));
				pFile->Write(&Channel(i).WetOnly(),sizeof(bool));
			}
			for (int i(0);i<numreturns();i++)
			{
				float volume, volMultiplier; 
				int wMacIdx;
				
				//Returning machines and values
				const Wire & wireRet = Return(i).GetWire();
				wMacIdx = (wireRet.Enabled()) ? wireRet.GetSrcMachine()._macIndex : -1;
				volume = wireRet.GetVolume();
				volMultiplier = wireRet.GetVolMultiplier();
				pFile->Write(&wMacIdx,sizeof(int));	// Incoming connections Machine number
				pFile->Write(&volume,sizeof(float));	// Incoming connections Machine vol
				pFile->Write(&volMultiplier,sizeof(float));	// Value to multiply _inputConVol[] to have a 0.0...1.0 range

				//Sending machines and values
				if (SendValid(i)) {
					wMacIdx = Send(i).GetDstMachine()._macIndex;
					volume = Send(i).GetVolume();
					volMultiplier = Send(i).GetVolMultiplier();
				}
				else {
					wMacIdx = -1;
					volume = 1.0f;
					volMultiplier = 1.0f;
				}
				pFile->Write(&wMacIdx,sizeof(int));	// send connections Machine number
				pFile->Write(&volume,sizeof(float));	// send connections Machine vol
				pFile->Write(&volMultiplier,sizeof(float));	// Value to multiply _inputConVol[] to have a 0.0...1.0 range

				//Rewiring of returns to sends and mix values
				for (int j(0);j<numsends();j++)
				{
					bool send(Return(i).SendsTo(j));
					pFile->Write(&send,sizeof(bool));
				}
				pFile->Write(&Return(i).MasterSend(),sizeof(bool));
				pFile->Write(&Return(i).Volume(),sizeof(float));
				pFile->Write(&Return(i).Panning(),sizeof(float));
				pFile->Write(&Return(i).Mute(),sizeof(bool));
			}
		}
		bool Mixer::LoadWireMapping(RiffFile* pFile, int version)
		{
			Machine::LoadWireMapping(pFile, version);
			int numWires = 0;
			for (int i(0);i<legacyReturn_.size();i++) {
				if (legacyReturn_[i]._inputCon) numWires++;
			}

			for(int countWires=0; countWires < numWires; countWires++)
			{
				int wireIdx, numPairs;
				Wire::PinConnection::first_type src;
				Wire::PinConnection::second_type dst;
				pFile->Read(wireIdx);
				if(wireIdx >= MAX_CONNECTIONS) {
					//We cannot ensure correctness from now onwards.
					return false;
				}
				pFile->Read(numPairs);
				Wire::Mapping& pinMapping = legacyReturn_[wireIdx].pinMapping;
				pinMapping.reserve(numPairs);
				for(int j=0; j < numPairs; j++){
					pFile->Read(src);
					pFile->Read(dst);
					pinMapping.push_back(Wire::PinConnection(src,dst));
				}
			}
			numWires=0;
			for (int i(0);i<legacySend_.size();i++) {
				if (legacySend_[i]._inputCon) numWires++;
			}

			for(int countWires=0; countWires < numWires; countWires++)
			{
				int wireIdx, numPairs;
				Wire::PinConnection::first_type src;
				Wire::PinConnection::second_type dst;
				pFile->Read(wireIdx);
				if(wireIdx >= MAX_CONNECTIONS) {
					//We cannot ensure correctness from now onwards.
					return false;
				}
				pFile->Read(numPairs);
				Wire::Mapping& pinMapping = legacySend_[wireIdx].pinMapping;
				pinMapping.reserve(numPairs);
				for(int j=0; j < numPairs; j++){
					pFile->Read(src);
					pFile->Read(dst);
					pinMapping.push_back(Wire::PinConnection(src,dst));
				}
			}
			return true;
		}
		bool Mixer::SaveWireMapping(RiffFile* pFile)
		{
			Machine::SaveWireMapping(pFile);
			for(int i = 0; i < MAX_CONNECTIONS; i++)
			{
				if (!ReturnValid(i)) {
					continue;
				}
				const Wire::Mapping& pinMapping = Return(i).GetWire().GetMapping();
				int numPairs=pinMapping.size();
				pFile->Write(i);
				pFile->Write(numPairs);
				for(int j=0; j <pinMapping.size(); j++) {
					const Wire::PinConnection &pin = pinMapping[j];
					pFile->Write(pin.first);
					pFile->Write(pin.second);
				}
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++)
			{
				if (!SendValid(i)) {
					continue;
				}
				const Wire::Mapping& pinMapping = Send(i).GetMapping();
				int numPairs=pinMapping.size();
				pFile->Write(i);
				pFile->Write(numPairs);
				for(int j=0; j <pinMapping.size(); j++) {
					const Wire::PinConnection &pin = pinMapping[j];
					pFile->Write(pin.first);
					pFile->Write(pin.second);
				}
			}
			return true;
		}

		void Mixer::PostLoad(Machine** _pMachine)
		{
			Machine::PostLoad(_pMachine);
			for (int j(0); j<numreturns(); ++j)
			{
				LegacyWire& wire = legacyReturn_[j];
				//inputCon is not used in legacyReturn.
				if ( wire._inputMachine >= 0 && wire._inputMachine < MAX_MACHINES
					&& _macIndex != wire._inputMachine && _pMachine[wire._inputMachine])
				{
					if (wire.pinMapping.size() > 0) {
						Return(j).GetWire().ConnectSource(*_pMachine[wire._inputMachine],1
							, FindLegacyOutput(_pMachine[wire._inputMachine], _macIndex)
							, &wire.pinMapping);
					}
					else {
						Return(j).GetWire().ConnectSource(*_pMachine[wire._inputMachine],1
							, FindLegacyOutput(_pMachine[wire._inputMachine], _macIndex));
					}
					Return(j).GetWire().SetVolume(wire._inputConVol);
					LegacyWire& wire2 = legacySend_[j];
					Send(j).SetVolume(wire2._inputConVol);
					if (wire2.pinMapping.size() > 0) {
						Send(j).ChangeMapping(wire2.pinMapping);
					}
				}
			}
			
			RecalcMaster();

			legacyReturn_.clear();
			legacySend_.clear();

		}
	}
}
