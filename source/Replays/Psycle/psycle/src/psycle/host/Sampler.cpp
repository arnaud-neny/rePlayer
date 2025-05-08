///\file
///\brief implementation file for psycle::host::Sampler.

#include <psycle/host/detail/project.private.hpp>
#include "Sampler.hpp"
#include "Song.hpp"
#include "XMInstrument.hpp"
#include "Player.hpp"
#include "FileIO.hpp"
#include "Configuration.hpp"
#include <psycle/helpers/value_mapper.hpp>
#include <psycle/helpers/math.hpp>
namespace psycle
{
	namespace host
	{
		char* Sampler::_psName = "Sampler";

		WaveDataController::WaveDataController()
			:wave(NULL)
			,_speed(0)
			,_vol(0.f)
			,_lVolDest(0)
			,_rVolDest(0)
			,_lVolCurr(0)
			,_rVolCurr(0)
			,resampler_data(NULL)
		{
			_pos.QuadPart = 0;
		}
		Voice::Voice()
			:inst(NULL)
			,_instrument(0xFF)
			,_channel(-1)
			,_sampleCounter(0)
			,_cutoff(0)
			,effCmd(SAMPLER_CMD_NONE)
		{
			_filter.Init(44100);
			Init();
		}
		Voice::~Voice()
		{
		}

		void Voice::Init()
		{
			_envelope._stage = ENV_OFF;
			_envelope._sustain = 0;
			_filterEnv._stage = ENV_OFF;
			_filterEnv._sustain = 0;
			_channel = -1;
			_triggerNoteOff = 0;
			_triggerNoteDelay = 0;
			effretTicks = 0;
			_effPortaSpeed =4294967296.0f;
		}
		void Voice::NewLine()
		{
			effretTicks = 0;
			effCmd=SAMPLER_CMD_NONE;
			if ( _triggerNoteOff > _sampleCounter) _triggerNoteOff -= _sampleCounter;
			else _triggerNoteOff = 0;
			if ( _triggerNoteDelay > _sampleCounter) _triggerNoteDelay -= _sampleCounter;
			else _triggerNoteDelay = 0;
			_sampleCounter=0;
		}

		Sampler::Sampler(int index)
		{
			_macIndex = index;
			_numPars=0;
			_type = MACH_SAMPLER;
			_mode = MACHMODE_GENERATOR;
			strncpy(_editName, _psName, sizeof(_editName)-1);
			_editName[sizeof(_editName)-1]='\0';
			InitializeSamplesVector();
			
			baseC = notecommands::middleC;
			linearslide=true;
			_resampler.quality(helpers::dsp::resampler::quality::spline);
			for (int i = 0; i < MAX_TRACKS; i++)
			{
				lastInstrument[i]=255;
			}
		}

		Sampler::~Sampler()
		{
			for (int i=0; i<SAMPLER_MAX_POLYPHONY; i++)
			{
				if (_voices[i].controller.resampler_data != NULL) _resampler.DisposeResamplerData(_voices[i].controller.resampler_data);
			}
		}
		void Sampler::Init(void)
		{
			Machine::Init();
			multicmdMem.resize(0);
			_numVoices = SAMPLER_DEFAULT_POLYPHONY;
			for (int i=0; i<_numVoices; i++) { _voices[i].Init(); }
			SetSampleRate(Global::player().SampleRate());
		}

		void Sampler::Stop(void)
		{
			for (int i=0; i<_numVoices; i++) { 
				_voices[i].NoteOffFast(); 
				_voices[i]._effPortaSpeed =4294967296.0f;
			}
		}

		void Sampler::StopInstrument(int insIdx)
		{
			for (int i=0; i<_numVoices; i++)
			{
				Voice* pVoice = &_voices[i];
				if ( pVoice->_instrument == insIdx &&
					 (pVoice->_envelope._stage != ENV_OFF ||
					  pVoice->_triggerNoteDelay > 0))
				{
					pVoice->Init();
				}
			}
		}
		int Sampler::NumAuxColumnIndexes()
		{
			return Global::song().samples.size();
		}
		const char* Sampler::AuxColumnName(int idx) const
		{
			SampleList &m_Samples = Global::song().samples;
			return m_Samples.Exists(idx)?m_Samples[idx].WaveName().c_str():"";
		}

		void Sampler::SetSampleRate(int sr)
		{
			Machine::SetSampleRate(sr);
			for (int i=0; i<_numVoices; i++) { 
				_voices[i]._filter.SampleRate(sr);
				_voices[i]._envelope.UpdateSRate(sr);
				_voices[i]._filterEnv.UpdateSRate(sr);
			}
		}

		bool Sampler::playsTrack(const int track) const
		{ 
			return (TriggerDelayCounter[track] > 0 || GetCurrentVoice(track) != -1);
		}

		int Sampler::GetCurrentVoice(int track) const
		{
			for ( int voice=0; voice<_numVoices; voice++)
			{
				// ENV_OFF is not checked, because channel will be -1
				if ( _voices[voice]._channel == track && (_voices[voice]._triggerNoteDelay > 0 || _voices[voice]._envelope._stage != ENV_FASTRELEASE))
				{
					return voice;
				}
			}
			return -1;
		}

		void Sampler::NewLine()
		{
			for (int voice=0;voice<_numVoices;voice++)
			{
				_voices[voice].NewLine();
			}
		}
		void Sampler::PostNewLine()
		{
			multicmdMem.clear();
		}

		int Sampler::GetFreeVoice() const
		{
			int useVoice=-1;
			for (int idx=0; idx<_numVoices; idx++)	// Find a voice to apply the new note
			{
				switch(_voices[idx]._envelope._stage)
				{
					case ENV_OFF: 
						if ( _voices[idx]._triggerNoteDelay == 0 ) 
						{ 
							useVoice=idx;
							idx=_numVoices; // Ok, we can go out from the loop already.
						}
						break;
					case ENV_FASTRELEASE: 
						useVoice = idx;
						break;
					case ENV_RELEASE:
						if ( useVoice == -1 ) useVoice= idx;
						break;
					default:break;
				}
			}
			return useVoice;
		}
		void Sampler::Tick(int channel, PatternEntry* pData)
		{
			if ( _mute ) return; // Avoid new note entering when muted.

			PatternEntry data = *pData;
			if(data._note == notecommands::midicc) {
				//TODO: This has one problem, it requires a non-mcm command to trigger the memory.
				data._inst = channel;
				multicmdMem.push_back(data);
				return;
			}
			else if ( data._note > notecommands::release && data._note != notecommands::empty) {
				 // don't process twk , twf of Mcm Commands
				return;
			}

			if (data._inst == 255)
			{
				data._inst = lastInstrument[channel];
				if (data._inst == 255) {
					return;  // no previous sample. Skip
				}
			}
			else { lastInstrument[channel] = data._inst; }

			if ( !Global::song().samples.IsEnabled(data._inst) ) return; // if no wave, return.

			int idx = GetCurrentVoice(channel);
			if (data._cmd != SAMPLER_CMD_NONE) {
				// Adding also the current command, to make the loops easier.
				PatternEntry data2 = data;
				data2._inst = channel;
				multicmdMem.push_back(data2);
			}
			bool doporta=false;
			for (std::vector<PatternEntry>::const_iterator ite = multicmdMem.begin(); ite != multicmdMem.end(); ++ite) {
				if(ite->_inst == channel) {
					if (ite->_cmd == SAMPLER_CMD_PORTA2NOTE && data._note < notecommands::release && idx != -1) {
						if (isLinearSlide()) {
							EnablePerformFx();
						}
						doporta=true;
					}
					else if (ite->_cmd == SAMPLER_CMD_PORTADOWN || ite->_cmd == SAMPLER_CMD_PORTAUP){
						if (isLinearSlide()) {
							EnablePerformFx();
						}
					}
				}
			}

			int useVoice = -1;
			if ( data._note < notecommands::release && !doporta)	// Handle Note On.
			{
				useVoice = GetFreeVoice(); // Find a voice to apply the new note
				if (idx != -1) // NoteOff previous Notes in this channel.
				{
					switch (_voices[idx].inst->_NNA)
					{
					case 0:
						_voices[idx].NoteOffFast();
						break;
					case 1:
						_voices[idx].NoteOff();
						break;
					}
					if ( useVoice == -1 ) { useVoice = idx; }
				}
				if ( useVoice == -1 )	// No free voices. Assign first one.
				{						// This algorithm should be replace by a LRU lookup
					useVoice=0;
				}
				_voices[useVoice]._channel=channel;
			}
			else {
				if (idx != -1)
				{
					if ( data._note == notecommands::release ) _voices[idx].NoteOff();  //  Handle Note Off
					useVoice=idx;
				}
				if ( useVoice == -1 ) return;   // No playing note on this channel. Just go out.
												// Change it if you have channel commands.
			}
			// If you want to make a command that controls more than one voice (the entire channel, for
			// example) you'll need to change this. Otherwise, add it to Voice.Tick().
			_voices[useVoice].Tick(&data, channel, _resampler, baseC, multicmdMem); 
		}

		int Voice::Tick(PatternEntry* pEntry,int channelNum, dsp::resampler& resampler, int baseC, std::vector<PatternEntry>&multicmdMem)
		{		
			int triggered = 0;
			uint64_t w_offset = 0;
			bool dooffset = false;
			bool dovol = false;
			bool dopan = false;
			bool doporta = false;

			//If this sample is not enabled, Voice::Tick is not called. Also, Sampler::Tick takes care of previus instrument used.
			_instrument = pEntry->_inst;
			inst = Global::song()._pInstrument[_instrument];

			// Setup commands that affect the new or already playing voice.
			for (std::vector<PatternEntry>::const_iterator ite = multicmdMem.begin(); ite != multicmdMem.end(); ++ite) {
				if(ite->_inst == channelNum) {
					// one shot {
					switch(ite->_cmd) {
						case SAMPLER_CMD_PANNING: {
							controller._pan = helpers::value_mapper::map_256_1(ite->_parameter);
							dopan=true;
						} break;
						case SAMPLER_CMD_OFFSET : {
							const XMInstrument::WaveData<>& wave = Global::song().samples[_instrument];
							w_offset = static_cast<uint64_t>(ite->_parameter*wave.WaveLength()) << 24;
							dooffset=true;
						} break;
						case SAMPLER_CMD_VOLUME : {
							controller._vol = helpers::value_mapper::map_256_1(ite->_parameter);
							dovol=true;
						} break;
						// }
						// Running {
						case SAMPLER_CMD_PORTAUP: {
							effVal=ite->_parameter;
							effCmd = ite->_cmd;
						} break;
						case SAMPLER_CMD_PORTADOWN: {
							effVal=ite->_parameter;
							effCmd = ite->_cmd;
						} break;
						case SAMPLER_CMD_PORTA2NOTE: {
							if (_envelope._stage != ENV_OFF) {
								effCmd = ite->_cmd;
								effVal=ite->_parameter;
								if (pEntry->_note < notecommands::release) {
									const XMInstrument::WaveData<>& wave = Global::song().samples[_instrument];
									float const finetune = (float)wave.WaveFineTune()*0.01f;
									double speeddouble = pow(2.0f, (pEntry->_note+wave.WaveTune()-baseC +finetune)/12.0f)*((float)wave.WaveSampleRate()/Global::player().SampleRate());
									_effPortaSpeed = static_cast<int64_t>(speeddouble*4294967296.0f);
								}
								if (_effPortaSpeed < controller._speed) {
									effVal *=-1;
								}
								doporta=true;
							}
						} break;
						case SAMPLER_CMD_RETRIG: {
							if((ite->_parameter&0x0f) > 0)
							{
								effretTicks=(ite->_parameter&0x0f); // number of Ticks.
								effVal= (Global::player().SamplesPerRow()/(effretTicks+1));

								int volmod = (ite->_parameter&0xf0)>>4; // Volume modifier.
								switch (volmod) 
								{
									case 0:  //fallthrough
									case 8:	effretVol = 0; effretMode=0; break;

									case 1:  //fallthrough
									case 2:  //fallthrough
									case 3:  //fallthrough
									case 4:  //fallthrough
									case 5: effretVol = (float)(std::pow(2.,volmod-1)/64); effretMode=1; break;

									case 6: effretVol = 0.66666666f;	 effretMode=2; break;
									case 7: effretVol = 0.5f;			 effretMode=2; break;

									case 9:  //fallthrough
									case 10:  //fallthrough
									case 11:  //fallthrough
									case 12:  //fallthrough
									case 13: effretVol = (float)(std::pow(2.,volmod-9)*(-1))/64; effretMode=1; break;

									case 14: effretVol = 1.5f;					effretMode=2; break;
									case 15: effretVol = 2.0f;					effretMode=2; break;
								}
								_triggerNoteDelay = effVal;
							}
						} break;
						// }
						case SAMPLER_CMD_EXTENDED: {
						// delayed {
							if ((ite->_parameter & 0xf0) == SAMPLER_CMD_EXT_NOTEOFF) {
								//This means there is always 6 ticks per row whatever number of rows.
								_triggerNoteOff = (Global::player().SamplesPerRow()/6.f)*(ite->_parameter & 0x0f);
							}
							else if ((ite->_parameter & 0xf0) == SAMPLER_CMD_EXT_NOTEDELAY && (ite->_parameter & 0x0f) != 0) {
								//This means there is always 6 ticks per row whatever number of rows.
								_triggerNoteDelay = (Global::player().SamplesPerRow()/6.f)*(ite->_parameter & 0x0f);
							}
						// }
						} break;
					}
				}
			}

			if (pEntry->_note < notecommands::release && !doporta)
			{
				const XMInstrument::WaveData<>& wave = Global::song().samples[_instrument];
				controller.wave = &wave;

				// Init Resampler
				//
				double speeddouble;
				if (inst->_loop)
				{
					double const totalsamples = double(Global::player().SamplesPerRow()*inst->_lines);
					speeddouble = (wave.WaveLength()/totalsamples);
				}	
				else
				{
					float const finetune = (float)wave.WaveFineTune()*0.01f;
					speeddouble = pow(2.0f, (pEntry->_note+wave.WaveTune()-baseC +finetune)/12.0f)*((float)wave.WaveSampleRate()/Global::player().SampleRate());
				}
				controller._speed = static_cast<int64_t>(speeddouble*4294967296.0f);

				if (controller.resampler_data != NULL) resampler.DisposeResamplerData(controller.resampler_data);
				controller.resampler_data = resampler.GetResamplerData();
				resampler.UpdateSpeed(controller.resampler_data,speeddouble);

				if (!dooffset) { dooffset = true; w_offset = 0; }

				// Init Amplitude Envelope
				//
				_envelope._sustain = static_cast<float>(inst->ENV_SL)*0.01f;
				_envelope._step = (1.0f/inst->ENV_AT)*_envelope.sratefactor;
				_envelope._value = 0.0f;
				controller._lVolCurr = controller._lVolDest;
				controller._rVolCurr = controller._rVolDest;
				if (!dovol) { dovol=true; controller._vol = 1.f; }
				if (!dopan) {
					if (inst->_RPAN) {
						dopan = true; 
						controller._pan = value_mapper::map_32768_1(rand());
					}
					else {
						dopan = true; 
						controller._pan = Global::song().samples[_instrument].PanFactor();
					}
				}

				//Init filter
				_cutoff = (inst->_RCUT) ? alteRand(inst->ENV_F_CO) : inst->ENV_F_CO;
				_filter.Ressonance((inst->_RRES) ? alteRand(inst->ENV_F_RQ) : inst->ENV_F_RQ);
				_filter.Type(inst->ENV_F_TP);
				_coModify = static_cast<float>(inst->ENV_F_EA);
				_filterEnv._sustain = value_mapper::map_128_1(inst->ENV_F_SL);
				_filterEnv._step = (1.0f/inst->ENV_F_AT)*_envelope.sratefactor;
				_filterEnv._value = 0;

				if (_triggerNoteDelay == 0) {
					_envelope._stage = ENV_ATTACK;
					_filterEnv._stage = ENV_ATTACK;
				}
				else {
					_envelope._stage = ENV_OFF;
					_filterEnv._stage = ENV_OFF;
				}

				triggered = 1;
			}

			if (dovol || dopan) {
				// Panning calculation -------------------------------------------
				controller._rVolDest = controller._pan;
				controller._lVolDest = 1.f-controller._pan;
				//FT2 Style (Two slides) mode, but with max amp = 0.5.
				if (controller._rVolDest > 0.5f) { controller._rVolDest = 0.5f; }
				if (controller._lVolDest > 0.5f) { controller._lVolDest = 0.5f; }

				controller._lVolDest *= Global::song().samples[_instrument].WaveGlobVolume() * controller._vol;
				controller._rVolDest *= Global::song().samples[_instrument].WaveGlobVolume() * controller._vol;
			}
			if (dooffset) {
				controller._pos.QuadPart = w_offset;
			}

			if (triggered)
			{
				controller._lVolCurr = controller._lVolDest;
				controller._rVolCurr = controller._rVolDest;
			}

			return triggered;

		}

		void Sampler::EnablePerformFx()
		{
			for (int i=0; i < Global::song().SONGTRACKS; i++)
			{
				if (TriggerDelay[i]._cmd == 0)
				{
					TriggerDelayCounter[i] = 0;
					TriggerDelay[i]._cmd = 0xEF;
					TriggerDelay[i]._parameter = Global::song().TicksPerBeat()/Global::song().LinesPerBeat();
					break;
				}
			}
		}

		int Sampler::GenerateAudioInTicks(int /*startSample*/,  int numSamples)
		{
			if (!_mute)
			{
				Standby(false);
				if (!linearslide) {
					for (int voice=0; voice<_numVoices; voice++)
					{
						// A correct implementation needs to take numsamples and player samplerate into account.
						// This will not be fixed to keep sampler compatible with old songs.
						_voices[voice].PerformFxOld(_resampler);
					}
				}
				//remaining samples to work
				int ns = numSamples;
				//sample pointer offset.
				int us = 0;
				while (ns)
				{
					int nextevent = ns+1;
					//get the nearest upcoming event.
					for (int i=0; i < Global::song().SONGTRACKS; i++)
					{
						if (TriggerDelay[i]._cmd)
						{
							if (TriggerDelayCounter[i] < nextevent)
							{
								nextevent = TriggerDelayCounter[i];
							}
						}
					}
					// if nextevent doesn't happen on this work call
					if (nextevent > ns)
					{
						//update nextevent times.
						for (int i=0; i < Global::song().SONGTRACKS; i++)
						{
							if (TriggerDelay[i]._cmd)
							{
								TriggerDelayCounter[i] -= ns;
							}
						}
						//and do work.
						for (int voice=0; voice<_numVoices; voice++)
						{
							_voices[voice].Work(ns, _resampler.work, samplesV[0]+us,samplesV[1]+us);
						}
						ns = 0;
					}
					// if nextevent happens on this work call
					else
					{
						//if it hasn't come yet
						if (nextevent)
						{
							ns -= nextevent;
							//do work.
							for (int voice=0; voice<_numVoices; voice++)
							{
								_voices[voice].Work(nextevent, _resampler.work, samplesV[0]+us,samplesV[1]+us);
							}
							us += nextevent;
						}
						//and get nextevent
						for (int i=0; i < Global::song().SONGTRACKS; i++)
						{
							// if it is this event
							if (TriggerDelayCounter[i] == nextevent)
							{
								if (TriggerDelay[i]._cmd == PatternCmd::NOTE_DELAY)
								{
									// do event
									Tick(i,&TriggerDelay[i]);
									TriggerDelay[i]._cmd = 0;
								}
								else if (TriggerDelay[i]._cmd == PatternCmd::RETRIGGER)
								{
									// do event
									Tick(i,&TriggerDelay[i]);
									TriggerDelayCounter[i] = (RetriggerRate[i]*Global::player().SamplesPerRow())/256;
								}
								else if (TriggerDelay[i]._cmd == PatternCmd::RETR_CONT)
								{
									// do event
									Tick(i,&TriggerDelay[i]);
									TriggerDelayCounter[i] = (RetriggerRate[i]*Global::player().SamplesPerRow())/256;
									int parameter = TriggerDelay[i]._parameter&0x0f;
									if (parameter < 9)
									{
										RetriggerRate[i]+= 4*parameter;
									}
									else
									{
										RetriggerRate[i]-= 2*(16-parameter);
										if (RetriggerRate[i] < 16)
										{
											RetriggerRate[i] = 16;
										}
									}
								}
								//Small hack to have a tracker tick.
								else if (TriggerDelay[i]._cmd == 0xEF) {
									TriggerDelay[i]._parameter--;
									if (TriggerDelay[i]._parameter > 0) {
										TriggerDelayCounter[i] = Global::player().SamplesPerTick();
									}
									else {
										TriggerDelay[i]._cmd = 0;
									}
									for (int voice=0; voice<_numVoices; voice++)
									{
										_voices[voice].PerformFxNew(_resampler);
									}
								}
							}
							//update nextevent times.
							else
							{
								TriggerDelayCounter[i] -= nextevent;
							}
						}
					}
				}
				UpdateVuAndStanbyFlag(numSamples);
			}

			else Standby(true);
			return numSamples;
		}

		void Voice::Work(int numsamples, helpers::dsp::resampler::work_func_type pResamplerWork, float* pSamplesL, float* pSamplesR)
		{
			float left_output;
			float right_output;

			// If the sample has been deleted while playing...
			if (!Global::song().samples.IsEnabled(_instrument))
			{
				Init();
				return;
			}


			_sampleCounter += numsamples;

			if (_triggerNoteDelay>0 )
			{
				if (_sampleCounter >= _triggerNoteDelay)
				{
					if (effretTicks > 0)
					{
						effretTicks--;
						_triggerNoteDelay = _sampleCounter+ effVal;

						_envelope._step = (1.0f/inst->ENV_AT)*_envelope.sratefactor;
						_filterEnv._step = (1.0f/inst->ENV_F_AT)*_envelope.sratefactor;
						controller._pos.QuadPart = 0;
						if ( effretMode == 1 )
						{
							controller._lVolDest += effretVol;
							controller._rVolDest += effretVol;
						}
						else if (effretMode == 2 )
						{
							controller._lVolDest *= effretVol;
							controller._rVolDest *= effretVol;
						}
					}
					else 
					{
						_triggerNoteDelay=0;
					}
					_envelope._stage = ENV_ATTACK;
				}
				else if (_envelope._stage == ENV_OFF)
				{
					return;
				}
			}
			else if (_envelope._stage == ENV_OFF)
			{
				Init();
				return;
			}
			else if ((_triggerNoteOff) && (_sampleCounter >= _triggerNoteOff))
			{
				NoteOff();
			}

			while (numsamples)
			{
				left_output=0;
				right_output=0;

				controller.Work(pResamplerWork, left_output, right_output);

				// Amplitude section
				{
					_envelope.TickEnvelope(inst->ENV_DT);
					controller.RampVolume();
				}
				// Filter section
				//
				if (_filter.Type() != dsp::F_NONE)
				{
					_filterEnv.TickEnvelope(inst->ENV_F_DT);
					int newcutoff = _cutoff + helpers::math::round<int, float>(_filterEnv._value*_coModify);
					if (newcutoff < 0) {
						newcutoff = 0;
					}
					else if (newcutoff > 127) {
						newcutoff = 127;
					}
					_filter.Cutoff(newcutoff);
					if (controller.wave->IsWaveStereo())
					{
						_filter.WorkStereo(left_output, right_output);
					}
					else
					{
						left_output = _filter.Work(left_output);
					}
				}

				if (!controller.wave->IsWaveStereo())
					right_output=left_output;
				right_output *= controller._rVolCurr*_envelope._value;
				left_output *= controller._lVolCurr*_envelope._value;

				// Move sample position
				if (!controller.PostWork()) {
					Init();
					break;
				}
					
				*pSamplesL++ = *pSamplesL+left_output;
				*pSamplesR++ = *pSamplesR+right_output;
				numsamples--;
			}
		}

		void Voice::NoteOff()
		{
			if (_envelope._stage != ENV_OFF)
			{
				_envelope._stage = ENV_RELEASE;
				_filterEnv._stage = ENV_RELEASE;
				_envelope._step = (_envelope._value/inst->ENV_RT)*_envelope.sratefactor;
				_filterEnv._step = (_filterEnv._value/inst->ENV_F_RT)*_envelope.sratefactor;
			}
			_triggerNoteDelay = 0;
			_triggerNoteOff = 0;
		}

		void Voice::NoteOffFast()
		{
			if (_envelope._stage != ENV_OFF)
			{
				_envelope._stage = ENV_FASTRELEASE;
				_envelope._step = _envelope._value/OVERLAPTIME;

				_filterEnv._stage = ENV_RELEASE;
				_filterEnv._step = _filterEnv._value/OVERLAPTIME;
			}
			_triggerNoteDelay = 0;
			_triggerNoteOff = 0;
		}

		void Voice::PerformFxOld(dsp::resampler& resampler)
		{
			// 4294967 stands for (2^30/250), meaning that
			//value 250 = (inc)decreases the speed in 1/4th of the original (wave) speed each PerformFx call.
			int64_t shift;
			switch(effCmd)
			{
				// 0x01 : Pitch Up
				case SAMPLER_CMD_PORTAUP:
					shift=static_cast<int64_t>(effVal)*4294967ll * static_cast<float>(controller.wave->WaveSampleRate())/Global::player().SampleRate();
					controller._speed+=shift;
					resampler.UpdateSpeed(controller.resampler_data,controller._speed);
				break;
				// 0x02 : Pitch Down
				case SAMPLER_CMD_PORTADOWN:
					shift=static_cast<int64_t>(effVal)*4294967ll * static_cast<float>(controller.wave->WaveSampleRate())/Global::player().SampleRate();
					controller._speed-=shift;
					if ( controller._speed < 0 ) controller._speed=0;
					resampler.UpdateSpeed(controller.resampler_data,controller._speed);
				break;
				// 0x03 : Porta to note
				case SAMPLER_CMD_PORTA2NOTE:
					//effVal is multiplied by -1 in Tick if it needs to slide down.
					shift=static_cast<int64_t>(effVal)*4294967ll * static_cast<float>(controller.wave->WaveSampleRate())/Global::player().SampleRate();
					controller._speed+=shift;
					if (( effVal < 0 && controller._speed < _effPortaSpeed ) 
						|| ( effVal > 0 && controller._speed > _effPortaSpeed )) {
						controller._speed = _effPortaSpeed;
						effCmd = SAMPLER_CMD_NONE;
					}
					resampler.UpdateSpeed(controller.resampler_data,controller._speed);
				break;

				default:
				break;
			}
		}
		void Voice::PerformFxNew(dsp::resampler& resampler)
		{
			//value 1 = (inc)decreases the speed in one seminote each beat.
			double factor = 1.0/(12.0*Global::song().TicksPerBeat());
			switch(effCmd)
			{
				// 0x01 : Pitch Up
				case SAMPLER_CMD_PORTAUP:
					controller._speed*=pow(2.0,effVal*factor);
					resampler.UpdateSpeed(controller.resampler_data,controller._speed);
				break;
				// 0x02 : Pitch Down
				case SAMPLER_CMD_PORTADOWN:
					controller._speed*=pow(2.0,-effVal*factor);
					if ( controller._speed < 0 ) controller._speed=0;
					resampler.UpdateSpeed(controller.resampler_data,controller._speed);
				break;
				// 0x03 : Porta to note
				case SAMPLER_CMD_PORTA2NOTE:
					//effVal is multiplied by -1 in Tick() if it needs to slide down.
					controller._speed*=pow(2.0,effVal*factor);
					if (( effVal < 0 && controller._speed < _effPortaSpeed ) 
						|| ( effVal > 0 && controller._speed > _effPortaSpeed )) {
						controller._speed = _effPortaSpeed;
						effCmd = SAMPLER_CMD_NONE;
					}
					resampler.UpdateSpeed(controller.resampler_data,controller._speed);
				break;

				default:
				break;
			}
		}

		void Sampler::ChangeResamplerQuality(helpers::dsp::resampler::quality::type quality)
		{
			for (int i=0; i<SAMPLER_MAX_POLYPHONY; i++)
			{
				if (_voices[i].controller.resampler_data != NULL) {
					_resampler.DisposeResamplerData(_voices[i].controller.resampler_data);
					_voices[i].controller.resampler_data = NULL;
				}
			}
			_resampler.quality(quality);
			for (int i=0; i<SAMPLER_MAX_POLYPHONY; i++)
			{
				if (_voices[i]._envelope._stage != ENV_OFF) {
					double speeddouble = static_cast<double>(_voices[i].controller._speed)/4294967296.0f;
					_voices[i].controller.resampler_data = _resampler.GetResamplerData();
					_resampler.UpdateSpeed(_voices[i].controller.resampler_data, speeddouble);
				}
			}
		}

		bool Sampler::LoadSpecificChunk(RiffFile* pFile, int version)
		{
			//Old version had default C4 as false
			DefaultC4(false);
			LinearSlide(false);
			uint32_t size=0;
			pFile->Read(&size,sizeof(size));
			if (size)
			{
				/// Version 0
				int temp;
				pFile->Read(&temp, sizeof(temp)); // numSubtracks
				_numVoices=temp;
				pFile->Read(&temp, sizeof(temp)); // quality
				switch (temp)
				{
					case 2:	_resampler.quality(helpers::dsp::resampler::quality::spline); break;
					case 3:	_resampler.quality(helpers::dsp::resampler::quality::sinc); break;
					case 0:	_resampler.quality(helpers::dsp::resampler::quality::zero_order); break;
					case 1:
					default: _resampler.quality(helpers::dsp::resampler::quality::linear);
				}

				if(size > 3*sizeof(uint32_t))
				{
					uint32_t internalversion;
					pFile->Read(internalversion);
					if (internalversion >= 1) {
						bool defaultC4;
						pFile->Read(&defaultC4, sizeof(bool)); // correct A4 frequency.
						DefaultC4(defaultC4);
					}
					if (internalversion >= 2) {
						bool slidemode;
						pFile->Read(&slidemode, sizeof(bool)); // correct slide.
						LinearSlide(slidemode);
					}
				}
			}
			return TRUE;
		}

		void Sampler::SaveSpecificChunk(RiffFile* pFile) 
		{
			uint32_t temp;
			uint32_t size = 3*sizeof(temp) + 2*sizeof(bool);
			pFile->Write(&size,sizeof(size));
			temp = _numVoices;
			pFile->Write(&temp, sizeof(temp)); // numSubtracks
			switch (_resampler.quality())
			{
				case helpers::dsp::resampler::quality::zero_order: temp = 0; break;
				case helpers::dsp::resampler::quality::spline: temp = 2; break;
				case helpers::dsp::resampler::quality::sinc: temp = 3; break;
				case helpers::dsp::resampler::quality::linear: //fallthrough
				default: temp = 1;
			}
			pFile->Write(&temp, sizeof(temp)); // quality

			pFile->Write(SAMPLERVERSION);
			bool defaultC4 = isDefaultC4();
			pFile->Write(&defaultC4, sizeof(bool)); // correct A4
			pFile->Write(&linearslide, sizeof(bool)); // correct slide
		}
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		/// old file format vomit. don't look at it.

		bool Sampler::Load(RiffFile* pFile)
		{
			int i;

			pFile->Read(&_editName,16);
			_editName[15] = 0;
			linearslide=false;

			legacyWires.resize(MAX_CONNECTIONS);
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputMachine,sizeof(legacyWires[i]._inputMachine));	// Incoming connections Machine number
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._outputMachine,sizeof(legacyWires[i]._outputMachine));	// Outgoing connections Machine number
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputConVol,sizeof(legacyWires[i]._inputConVol));	// Incoming connections Machine vol
				legacyWires[i]._wireMultiplier = 1.0f;
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._connection,sizeof(legacyWires[i]._connection));      // Outgoing connections activated
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputCon,sizeof(legacyWires[i]._inputCon));		// Incoming connections activated
			}
			pFile->Read(&_connectionPoint[0], sizeof(_connectionPoint));
			pFile->Skip(2*sizeof(int)); // numInputs and numOutputs

			pFile->Read(&_panning, sizeof(_panning));
			Machine::SetPan(_panning);
			pFile->Skip(8*sizeof(int)); // SubTrack[]
			pFile->Read(&_numVoices, sizeof(_numVoices)); // numSubtracks

			if (_numVoices < 4)
			{
				// Psycle versions < 1.1b2 had polyphony per channel,not per machine.
				_numVoices = 8;
			}

			pFile->Read(&i, sizeof(int)); // interpol
			switch (i)
			{
				case 2:	_resampler.quality(helpers::dsp::resampler::quality::spline); break;
				case 3:	_resampler.quality(helpers::dsp::resampler::quality::sinc); break;
				case 0:	_resampler.quality(helpers::dsp::resampler::quality::zero_order); break;
				case 1:
				default: _resampler.quality(helpers::dsp::resampler::quality::linear);
			}
			pFile->Skip(69);

			return true;
		}
	}
}
