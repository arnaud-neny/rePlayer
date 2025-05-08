// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

///\implementation psycle::core::Sampler

#include <psycle/core/detail/project.private.hpp>
#include "sampler.h"

#include "fileio.h"
#include "song.h"
#include "cpu_time_clock.hpp"
#include <psycle/helpers/math.hpp>
#include <psycle/helpers/value_mapper.hpp>

namespace psycle { namespace core {

using namespace helpers;
using namespace helpers::math;

std::string Sampler::_psName = "Sampler";
InstPreview Sampler::wavprev;
InstPreview Sampler::waved;

static inline int alteRand(int x) {
	return (x*rand())/RAND_MAX;
}

Voice::Voice():resampler_data(NULL)
{
}
Voice::~Voice()
{
}


Sampler::Sampler(MachineCallbacks* callbacks, Machine::id_type id)
:
	Machine(callbacks, id)
{
	SetEditName(_psName);
	defineOutputAsStereo();
	SetAudioRange(32768.0f);
	//DefineStereoOutput(1);

	baseC = 60;
	resampler_.quality(dsp::resampler::quality::linear);
	for (int i=0; i<SAMPLER_MAX_POLYPHONY; i++)
	{
		_voices[i]._envelope._stage = ENV_OFF;
		_voices[i]._envelope._sustain = 0;
		_voices[i]._filterEnv._stage = ENV_OFF;
		_voices[i]._filterEnv._sustain = 0;
		_voices[i]._cutoff = 0;
		_voices[i]._sampleCounter = 0;
		_voices[i]._triggerNoteOff = 0;
		_voices[i]._triggerNoteDelay = 0;
		_voices[i]._channel = -1;
		_voices[i]._wave._lVolCurr = 0;
		_voices[i]._wave._rVolCurr = 0;

		_voices[i].effCmd = SAMPLER_CMD_NONE;
	}
	for (Instrument::id_type i(0); i < MAX_TRACKS; i++) lastInstrument[i]=255;
}

Sampler::~Sampler() {
	for (int i=0; i<SAMPLER_MAX_POLYPHONY; i++)
	{
		if (_voices[i].resampler_data != NULL) resampler_.DisposeResamplerData(_voices[i].resampler_data);
	}
}
void Sampler::Init()
{
	Machine::Init();

	_numVoices = SAMPLER_DEFAULT_POLYPHONY;

	for (int i=0; i<_numVoices; i++)
	{
		_voices[i]._envelope._stage = ENV_OFF;
		_voices[i]._envelope._sustain = 0;
		_voices[i]._filterEnv._stage = ENV_OFF;
		_voices[i]._filterEnv._sustain = 0;
		_voices[i]._filter.Init(44100);
		_voices[i]._triggerNoteOff = 0;
		_voices[i]._triggerNoteDelay = 0;
	}
}

int Sampler::GenerateAudioInTicks( int startSample, int numSamples )
{
	assert(numSamples >= 0);
	//nanoseconds const t0(cpu_time_clock());
	const PlayerTimeInfo & timeInfo = callbacks->timeInfo();
	if (!_mute)
	{
		Standby(false);
		for (int voice=0; voice<_numVoices; voice++)
		{
			// A correct implementation needs to take numsamples into account.
			// This will not be fixed to leave sampler compatible with old songs.
			PerformFx(voice);
		}
		int ns = numSamples;
		while (ns)
		{
			int nextevent = ns+1;
			for (unsigned int i=0; i < callbacks->song().tracks(); i++)
			{
				if (TriggerDelay[i].command() )
				{
					if (TriggerDelayCounter[i] < nextevent)
					{
						nextevent = TriggerDelayCounter[i];
					}
				}
			}
			if (nextevent > ns)
			{
				for (unsigned int i=0; i < callbacks->song().tracks(); i++)
				{
					// come back to this
					if (TriggerDelay[i].command() )
					{
						TriggerDelayCounter[i] -= ns;
					}
				}
				for (int voice=0; voice<_numVoices; voice++)
				{
					VoiceWork(startSample, ns, voice );
				}
				ns = 0;
			}
			else
			{
				if (nextevent)
				{
					ns -= nextevent;
					for (int voice=0; voice<_numVoices; voice++)
					{
						VoiceWork(startSample, nextevent, voice );
					}
				}
				for (unsigned int i=0; i < callbacks->song().tracks(); i++)
				{
					// come back to this
					if (TriggerDelay[i].command() == commandtypes::NOTE_DELAY)
					{
						if (TriggerDelayCounter[i] == nextevent)
						{
							// do event
							Tick(i, TriggerDelay[i] );
							TriggerDelay[i].setCommand( 0 );
						}
						else
						{
							TriggerDelayCounter[i] -= nextevent;
						}
					}
					else if (TriggerDelay[i].command() == commandtypes::RETRIGGER)
					{
						if (TriggerDelayCounter[i] == nextevent)
						{
							// do event
							Tick(i, TriggerDelay[i] );
							TriggerDelayCounter[i] = static_cast<int>( (RetriggerRate[i]*timeInfo.samplesPerTick())/256 );
						}
						else
						{
							TriggerDelayCounter[i] -= nextevent;
						}
					}
					else if (TriggerDelay[i].command() == commandtypes::RETR_CONT)
					{
						if (TriggerDelayCounter[i] == nextevent)
						{
							// do event
							Tick(i, TriggerDelay[i] );
							TriggerDelayCounter[i] = static_cast<int>( (RetriggerRate[i]*timeInfo.samplesPerTick())/256 );
							int parameter = TriggerDelay[i].parameter() & 0x0f;
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
						else
						{
							TriggerDelayCounter[i] -= nextevent;
						}
					}
				}
			}
		}
		UpdateVuAndStanbyFlag(numSamples);
	}
	else Standby(true);

	//nanoseconds const t1(cpu_time_clock());
	//accumulate_processing_time(t1 - t0);

	recursive_processed_ = true;
	return numSamples;
}

void Sampler::SetSampleRate(int sr)
{
	Machine::SetSampleRate(sr);
	for (int i=0; i<_numVoices; i++) {
		_voices[i]._envelope._stage = ENV_OFF;
		_voices[i]._envelope._sustain = 0;
		_voices[i]._filterEnv._stage = ENV_OFF;
		_voices[i]._filterEnv._sustain = 0;
		_voices[i]._filter.SampleRate(sr);
		_voices[i]._triggerNoteOff = 0;
		_voices[i]._triggerNoteDelay = 0;
	}
}

void Sampler::Stop(void)
{
	for (int i=0; i<_numVoices; i++)
	{
		NoteOffFast(i);
	}
	Machine::Stop();
}

void Sampler::VoiceWork(int startSample, int numsamples, int voice )
{
	assert(numsamples >= 0);
	const PlayerTimeInfo & timeInfo = callbacks->timeInfo();
	Voice* pVoice = &_voices[voice];
	float* pSamplesL = _pSamplesL+startSample;
	float* pSamplesR = _pSamplesR+startSample;
	float left_output;
	float right_output;

	pVoice->_sampleCounter += numsamples;

	if ((pVoice->_triggerNoteDelay) && (pVoice->_sampleCounter >= pVoice->_triggerNoteDelay))
	{
		if ( pVoice->effCmd == SAMPLER_CMD_RETRIG && pVoice->effretTicks)
		{
			pVoice->_triggerNoteDelay = pVoice->_sampleCounter+ pVoice->effVal;
			pVoice->_envelope._step = (1.0f/callbacks->song()._pInstrument[pVoice->_instrument]->ENV_AT)*(44100.0f/timeInfo.sampleRate());
			pVoice->_filterEnv._step = (1.0f/callbacks->song()._pInstrument[pVoice->_instrument]->ENV_F_AT)*(44100.0f/timeInfo.sampleRate());
			pVoice->effretTicks--;
			pVoice->_wave._pos = 0;
			if ( pVoice->effretMode == 1 )
			{
				pVoice->_wave._lVolDest += pVoice->effretVol;
				pVoice->_wave._rVolDest += pVoice->effretVol;
			}
			else if (pVoice->effretMode == 2 )
			{
				pVoice->_wave._lVolDest *= pVoice->effretVol;
				pVoice->_wave._rVolDest *= pVoice->effretVol;
			}
		}
		else 
		{
			pVoice->_triggerNoteDelay=0;
		}
		pVoice->_envelope._stage = ENV_ATTACK;
	}
	else if (pVoice->_envelope._stage == ENV_OFF)
	{
		pVoice->_wave._lVolCurr = 0;
		pVoice->_wave._rVolCurr = 0;
		return;
	}
	else if ((pVoice->_triggerNoteOff) && (pVoice->_sampleCounter >= pVoice->_triggerNoteOff))
	{
		pVoice->_triggerNoteOff = 0;
		NoteOff( voice );
	}

	dsp::resampler::work_func_type resampler_work = resampler_.work;
	while (numsamples)
	{
		left_output=0;
		right_output=0;

		if (pVoice->_envelope._stage != ENV_OFF)
		{
			left_output = resampler_work(pVoice->_wave._pL + (pVoice->_wave._pos >> 32),
								pVoice->_wave._pos>>32,
								pVoice->_wave._pos & 0xFFFFFFFF,
								pVoice->_wave._length, pVoice->resampler_data);
			if (pVoice->_wave._stereo)
			{
				right_output = resampler_work(pVoice->_wave._pR + (pVoice->_wave._pos >> 32),
									pVoice->_wave._pos >> 32,
									pVoice->_wave._pos & 0xFFFFFFFF,
									pVoice->_wave._length, pVoice->resampler_data);
			}

			// Filter section
			//
			if (pVoice->_filter.Type() < dsp::F_NONE)
			{
				TickFilterEnvelope( voice );
				int newcutoff = pVoice->_cutoff + round<int>(pVoice->_filterEnv._value*pVoice->_coModify);
				if (newcutoff < 0) { newcutoff = 0; }
				else if (newcutoff > 127) { newcutoff = 127; }
				pVoice->_filter.Cutoff(newcutoff);

				if (pVoice->_wave._stereo)
				{
					pVoice->_filter.WorkStereo(left_output, right_output);
				}
				else
				{
					left_output = pVoice->_filter.Work(left_output);
				}
			}

			TickEnvelope( voice );

			// calculate volume
			
			if(pVoice->_wave._lVolCurr<0)
				pVoice->_wave._lVolCurr=pVoice->_wave._lVolDest;
			if(pVoice->_wave._rVolCurr<0)
				pVoice->_wave._rVolCurr=pVoice->_wave._rVolDest;

			if(pVoice->_wave._lVolCurr>pVoice->_wave._lVolDest)
				pVoice->_wave._lVolCurr-=0.005f;
			if(pVoice->_wave._lVolCurr<pVoice->_wave._lVolDest)
				pVoice->_wave._lVolCurr+=0.005f;
			if(pVoice->_wave._rVolCurr>pVoice->_wave._rVolDest)
				pVoice->_wave._rVolCurr-=0.005f;
			if(pVoice->_wave._rVolCurr<pVoice->_wave._rVolDest)
				pVoice->_wave._rVolCurr+=0.005f;

			if(!pVoice->_wave._stereo)
				right_output=left_output;
			right_output *= pVoice->_wave._rVolCurr*pVoice->_envelope._value;
			left_output *= pVoice->_wave._lVolCurr*pVoice->_envelope._value;



			pVoice->_wave._pos += pVoice->_wave._speed;

			// Loop handler
			//
			if ((pVoice->_wave._loop) && ((pVoice->_wave._pos>>32) >= pVoice->_wave._loopEnd))
			{
				pVoice->_wave._pos -= (int64_t)(pVoice->_wave._loopEnd - pVoice->_wave._loopStart) << 32;
			}
			if ((pVoice->_wave._pos>>32) >= pVoice->_wave._length)
			{
				pVoice->_envelope._stage = ENV_OFF;
			}
		}
			
		*pSamplesL = (*pSamplesL)+left_output;
		*pSamplesR = (*pSamplesR)+right_output;
		pSamplesL++;
		pSamplesR++;
		numsamples--;
	}
}

void Sampler::Tick( )
{
	for (int voice=0;voice<_numVoices;voice++)
	{
		if ( _voices[voice].effCmd != SAMPLER_CMD_EXTENDED )
		{
			_voices[voice].effOld=_voices[voice].effCmd;
			_voices[voice].effCmd=SAMPLER_CMD_NONE;
		}
	}
}

void Sampler::Tick( int channel, const PatternEvent & event )
{
	if ( event.note() > notetypes::release ) // don't process twk , twf of Mcm Commands
	{
		if ( event.command() == 0 || event.note() != 255) return; // Return in everything but commands!
	}
	if ( _mute ) return; // Avoid new note entering when muted.

	int voice;
	int useVoice = -1;


	PatternEvent data = event;

	if (data.instrument() >= 255)
	{
		data.setInstrument( lastInstrument[channel] );
		if (data.instrument() >= 255)
		{
			return;  // no previous sample
		}
	}
	else
	{
		data.setInstrument( lastInstrument[channel] = event.instrument() );
	}


	if ( data.note() < notetypes::release ) // Handle Note On.
	{
		if ( callbacks->song()._pInstrument[data.instrument()]->waveLength == 0 ) return; // if no wave, return.

		for (voice=0; voice<_numVoices; voice++) // Find a voice to apply the new note
		{
			switch(_voices[voice]._envelope._stage)
			{
				case ENV_OFF: 
					if ( _voices[voice]._triggerNoteDelay == 0 ) 
					{ 
						useVoice=voice; 
						voice=_numVoices; // Ok, we can go out from the loop already.
					}
					break;
				case ENV_FASTRELEASE: 
					useVoice = voice;
					break;
				case ENV_RELEASE:
					if ( useVoice == -1 ) 
						useVoice= voice;
					break;
				default:break;
			}
		}
		for ( voice=0; voice<_numVoices; voice++)
		{
			if ( _voices[voice]._channel == channel ) // NoteOff previous Notes in this channel.
			{
				switch (callbacks->song()._pInstrument[_voices[voice]._instrument]->_NNA)
				{
				case 0:
					NoteOffFast( voice );
					break;
				case 1:
					NoteOff( voice );
					break;
				}
				if ( useVoice == -1 ) { useVoice = voice; }
			}
		}
		if ( useVoice == -1 ) // No free voices. Assign first one.
		{ ///\todo This algorithm should be replace by a LRU lookup
			useVoice=0;
		}
		_voices[useVoice]._channel=channel;
	}
	else {
		for (voice=0;voice<_numVoices; voice++)  // Find the...
		{
			if (( _voices[voice]._channel == channel ) && // ...playing voice on current channel.
				(_voices[voice]._envelope._stage != ENV_OFF ) &&
				//(_voices[voice]._envelope._stage != ENV_RELEASE ) &&
				// Effects can STILL apply in this case.
				// Think on a slow fadeout and changing panning
				(_voices[voice]._envelope._stage != ENV_FASTRELEASE )) 
			{
				if ( data.note() == notetypes::release ) NoteOff( voice );//  Handle Note Off
				useVoice=voice;
			}
		}
		if ( useVoice == -1 )
			// No playing note on this channel. Just go out.
			// Change it if you have channel commands.
			return;
	}
	// If you want to make a command that controls more than one voice (the entire channel, for
	// example) you'll need to change this. Otherwise, add it to VoiceTick().
	VoiceTick( useVoice, data ); 
}


bool Sampler::LoadSpecificChunk(RiffFile* pFile, int version)
{
	//Old version had default C4 as false
	DefaultC4(false);
	uint32_t size = 0;
	pFile->Read(size);
	if (size) {
		if (version > CURRENT_FILE_VERSION_MACD) {
			// data is from a newer format of psycle, it might be unsafe to load.
			pFile->Skip(size);
			return false;
		}
		else {
			/// Version 0
			int32_t temp = 0;
			pFile->Read(temp); // numSubtracks
			_numVoices=temp;
			pFile->Read(temp); // quality
			
			switch (temp) {
			case 2:	resampler_.quality(helpers::dsp::resampler::quality::spline); break;
			case 3:	resampler_.quality(helpers::dsp::resampler::quality::sinc); break;
			case 0:	resampler_.quality(helpers::dsp::resampler::quality::zero_order); break;
			case 1:
			default: resampler_.quality(helpers::dsp::resampler::quality::linear);
			}
			if(size > 3*sizeof(uint32_t ))
			{
                uint32_t internalversion = 0;
				pFile->Read(internalversion);
				if (internalversion >= SAMPLERVERSION) {
                    bool defaultC4 = false;
					pFile->Read(defaultC4); // correct A4 frequency.
					DefaultC4(defaultC4);
				}
			}
		}
	}
	return true;
}

void Sampler::SaveSpecificChunk(RiffFile* pFile) const
{
	int32_t temp;
	uint32_t size = 3*sizeof(temp) + 1*sizeof(bool);
	pFile->Write(size);
	temp = _numVoices;
	pFile->Write(temp); // numSubtracks
	switch (resampler_.quality()) {
		case helpers::dsp::resampler::quality::zero_order: temp = 0; break;
		case helpers::dsp::resampler::quality::spline: temp = 2; break;
		case helpers::dsp::resampler::quality::sinc: temp = 3; break;
		case helpers::dsp::resampler::quality::linear: //fallthrough
		default: temp = 1;
	}
	pFile->Write(temp); // quality

	pFile->Write(SAMPLERVERSION);
	bool defaultC4 = isDefaultC4();
	pFile->Write(defaultC4); // correct A4
}

int Sampler::VoiceTick( int voice, const PatternEvent & entry )
{
	const PlayerTimeInfo & timeInfo = callbacks->timeInfo();
	PatternEvent pEntry = entry;

	Voice* pVoice = &_voices[voice];
	int triggered = 0;
	uint64_t w_offset = 0;

	pVoice->_sampleCounter=0;
	pVoice->effCmd= pEntry.command();

	switch( pEntry.command() ) // DO NOT ADD here those commands that REQUIRE a note.
	{
		case SAMPLER_CMD_EXTENDED:
			if ((pEntry.parameter() & 0xf0) == SAMPLER_CMD_EXT_NOTEOFF)
			{
				pVoice->_triggerNoteOff = static_cast<int>( (timeInfo.samplesPerTick()/6)*(pEntry.parameter() & 0x0f) );
			}
			else if (((pEntry.parameter() & 0xf0) == SAMPLER_CMD_EXT_NOTEDELAY) && ((pEntry.parameter() & 0x0f) == 0 ))
			{
				pEntry.setCommand(0); 
				pEntry.setParameter(0);
			}
			break;
		case SAMPLER_CMD_PORTAUP:
			pVoice->effVal=pEntry.parameter();
			break;
		case SAMPLER_CMD_PORTADOWN:
			pVoice->effVal=pEntry.parameter();
			break;
	}
	

//  All this mess should be really changed with classes using the "operator=" to "copy" values.
	Instrument* pIns = callbacks->song()._pInstrument[pEntry.instrument()];
	int twlength = pIns->waveLength;
	
	if (pEntry.note() < notetypes::release && twlength > 0)
	{
		pVoice->_triggerNoteOff=0;
		pVoice->_instrument = pEntry.instrument();
		
		// Init filter synthesizer
		//
		pVoice->_filter.Init(timeInfo.sampleRate());

		pVoice->_cutoff = (pIns->_RCUT) ? alteRand(pIns->ENV_F_CO) : pIns->ENV_F_CO;
		pVoice->_filter.Ressonance((pIns->_RRES) ? alteRand(pIns->ENV_F_RQ) : pIns->ENV_F_RQ);
		pVoice->_filter.Type(pIns->ENV_F_TP);
		pVoice->_coModify = (float) pIns->ENV_F_EA;
		pVoice->_filterEnv._sustain = (float)pIns->ENV_F_SL*0.0078125f;

		if (( pEntry.command() != SAMPLER_CMD_EXTENDED) || ((pEntry.parameter() & 0xf0) != SAMPLER_CMD_EXT_NOTEDELAY))
		{
			pVoice->_filterEnv._stage = ENV_ATTACK;
		}
		pVoice->_filterEnv._step = (1.0f/pIns->ENV_F_AT)*(44100.0f/timeInfo.sampleRate());
		pVoice->_filterEnv._value = 0;
		
		// Init Wave
		//
		pVoice->_wave._pL = pIns->waveDataL;
		pVoice->_wave._pR = pIns->waveDataR;
		pVoice->_wave._stereo = pIns->waveStereo;
		pVoice->_wave._length = twlength;
		
		// Init loop
		if (pIns->waveLoopType)
		{
			pVoice->_wave._loop = true;
			pVoice->_wave._loopStart = pIns->waveLoopStart;
			pVoice->_wave._loopEnd = pIns->waveLoopEnd;
		}
		else
		{
			pVoice->_wave._loop = false;
		}
		

		// Init Resampler
		//
		double speeddouble;
		if ( pIns->_loop)
		{
			double const totalsamples = double(timeInfo.samplesPerTick()*pIns->_lines);
			speeddouble = (pVoice->_wave._length/totalsamples)*4294967296.0f;
		}
		else
		{
			float const finetune = (float)pIns->waveFinetune/256.f;
			speeddouble = pow(2.0f, (pEntry.note()+pIns->waveTune-baseC +finetune)/12.0f)*4294967296.0f*(44100.0f/timeInfo.sampleRate());
		}
        pVoice->_wave._speed = speeddouble;

		if (pVoice->resampler_data != NULL) resampler_.DisposeResamplerData(pVoice->resampler_data);
        pVoice->resampler_data = resampler_.GetResamplerData();
        resampler_.UpdateSpeed(pVoice->resampler_data, speeddouble);


		// Handle wave_start_offset cmd
		//
		if (pEntry.command() == SAMPLER_CMD_OFFSET)
		{
			w_offset = pEntry.parameter()*pVoice->_wave._length;
			pVoice->_wave._pos = w_offset << 24;
		}
		else
		{
			pVoice->_wave._pos = 0;
		}

		// Calculating volume coef ---------------------------------------
		//
		pVoice->_wave._vol = (float)pIns->waveVolume*0.01f;

		if (pEntry.command() == SAMPLER_CMD_VOLUME)
		{
			pVoice->_wave._vol *= value_mapper::map_256_1( pEntry.parameter() );
		}
		
		// Panning calculation -------------------------------------------
		//
		float panFactor;
		
		if (pIns->_RPAN)
		{
			panFactor = (float)rand()/RAND_MAX;
		}
		else if ( pEntry.command() == SAMPLER_CMD_PANNING )
		{
			panFactor = value_mapper::map_256_1( pEntry.parameter() );
		}
		else {
			panFactor = value_mapper::map_256_1(pIns->_pan);
		}

		pVoice->_wave._rVolDest = panFactor;
		pVoice->_wave._lVolDest = 1-panFactor;

		if (pVoice->_wave._rVolDest > 0.5f)
		{
			pVoice->_wave._rVolDest = 0.5f;
		}
		if (pVoice->_wave._lVolDest > 0.5f)
		{
			pVoice->_wave._lVolDest = 0.5f;
		}

		pVoice->_wave._lVolCurr = (pVoice->_wave._lVolDest *= pVoice->_wave._vol);
		pVoice->_wave._rVolCurr = (pVoice->_wave._rVolDest *= pVoice->_wave._vol);

		// Init Amplitude Envelope
		//
		pVoice->_envelope._step = (1.0f/pIns->ENV_AT)*(44100.0f/timeInfo.sampleRate());
		pVoice->_envelope._value = 0.0f;
		pVoice->_envelope._sustain = (float)pIns->ENV_SL*0.01f;
		if (( pEntry.command() == SAMPLER_CMD_EXTENDED) && ((pEntry.parameter() & 0xf0) == SAMPLER_CMD_EXT_NOTEDELAY))
		{
			pVoice->_triggerNoteDelay = static_cast<int>( (timeInfo.samplesPerTick()/6)*(pEntry.parameter() & 0x0f) );
			pVoice->_envelope._stage = ENV_OFF;
		}
		else
		{
			if (pEntry.command() == SAMPLER_CMD_RETRIG && (pEntry.parameter() & 0x0f) > 0)
			{
				pVoice->effretTicks=(pEntry.parameter() & 0x0f); // number of Ticks.
				pVoice->effVal= static_cast<int>( (timeInfo.samplesPerTick()/(pVoice->effretTicks+1)) );
				
				int volmod = (pEntry.parameter() & 0xf0)>>4; // Volume modifier.
				switch (volmod) 
				{
					case 0:
					case 8: pVoice->effretVol = 0; pVoice->effretMode=0; break;
					case 1:
					case 2:
					case 3:
					case 4:
					case 5: pVoice->effretVol = (float)(std::pow(2.,volmod-1)/64); pVoice->effretMode=1; break;
					case 6: pVoice->effretVol = 0.66666666f; pVoice->effretMode=2; break;
					case 7: pVoice->effretVol = 0.5f; pVoice->effretMode=2; break;
					case 9:
					case 10:
					case 11:
					case 12:
					case 13: pVoice->effretVol = (float)(std::pow(2.,volmod-9)*(-1))/64; pVoice->effretMode=1; break;
					case 14: pVoice->effretVol = 1.5f; pVoice->effretMode=2; break;
					case 15: pVoice->effretVol = 2.0f; pVoice->effretMode=2; break;
				}
				pVoice->_triggerNoteDelay = pVoice->effVal;
			}
			else
			{
				pVoice->_triggerNoteDelay=0;
			}
			pVoice->_envelope._stage = ENV_ATTACK;
		}
		
		triggered = 1;
	}

	else if ((pEntry.command() == SAMPLER_CMD_VOLUME) || ( pEntry.command() == SAMPLER_CMD_PANNING ) )
	{
		// Calculating volume coef ---------------------------------------
		//
		pVoice->_wave._vol = (float)pIns->waveVolume*0.01f;

		if ( pEntry.command() == SAMPLER_CMD_VOLUME ) pVoice->_wave._vol *= value_mapper::map_256_1(pEntry.parameter() );
		
		// Panning calculation -------------------------------------------
		//
		float panFactor;
		
		if (pIns->_RPAN)
		{
			panFactor = (float)rand()/RAND_MAX;
		}
		else if ( pEntry.command() == SAMPLER_CMD_PANNING )
		{
			panFactor = value_mapper::map_256_1( pEntry.parameter() );
		}
		else
		{
			panFactor = value_mapper::map_256_1(pIns->_pan);
		}

		pVoice->_wave._rVolDest = panFactor;
		pVoice->_wave._lVolDest = 1-panFactor;

		if (pVoice->_wave._rVolDest > 0.5f)
		{
			pVoice->_wave._rVolDest = 0.5f;
		}
		if (pVoice->_wave._lVolDest > 0.5f)
		{
			pVoice->_wave._lVolDest = 0.5f;
		}

		pVoice->_wave._lVolDest *= pVoice->_wave._vol;
		pVoice->_wave._rVolDest *= pVoice->_wave._vol;
	}

	return triggered;

}

void Sampler::TickFilterEnvelope( int voice )
{
	const PlayerTimeInfo & timeInfo = callbacks->timeInfo();

	Voice* pVoice = &_voices[voice];
	switch (pVoice->_filterEnv._stage)
	{
	case ENV_ATTACK:
		pVoice->_filterEnv._value += pVoice->_filterEnv._step;

		if (pVoice->_filterEnv._value > 1.0f)
		{
			pVoice->_filterEnv._stage = ENV_DECAY;
			pVoice->_filterEnv._value = 1.0f;
			pVoice->_filterEnv._step = ((1.0f - pVoice->_filterEnv._sustain) / callbacks->song()._pInstrument[pVoice->_instrument]->ENV_F_DT) * (44100.0f/timeInfo.sampleRate());
		}
		break;
	case ENV_DECAY:
		pVoice->_filterEnv._value -= pVoice->_filterEnv._step;

		if (pVoice->_filterEnv._value < pVoice->_filterEnv._sustain)
		{
			pVoice->_filterEnv._value = pVoice->_filterEnv._sustain;
			pVoice->_filterEnv._stage = ENV_SUSTAIN;
		}
		break;
	case ENV_RELEASE:
		pVoice->_filterEnv._value -= pVoice->_filterEnv._step;

		if (pVoice->_filterEnv._value < 0)
		{
			pVoice->_filterEnv._value = 0;
			pVoice->_filterEnv._stage = ENV_OFF;
		}
		break;
	default:
		break;
	}
}

void Sampler::TickEnvelope( int voice )
{
	const PlayerTimeInfo & timeInfo = callbacks->timeInfo();

	Voice* pVoice = &_voices[voice];
	switch (pVoice->_envelope._stage)
	{
	case ENV_ATTACK:
		pVoice->_envelope._value += pVoice->_envelope._step;
		if (pVoice->_envelope._value > 1.0f)
		{
			pVoice->_envelope._value = 1.0f;
			pVoice->_envelope._stage = ENV_DECAY;
			pVoice->_envelope._step = ((1.0f - pVoice->_envelope._sustain)/callbacks->song()._pInstrument[pVoice->_instrument]->ENV_DT)*(44100.0f/timeInfo.sampleRate());
		}
		break;
	case ENV_DECAY:
		pVoice->_envelope._value -= pVoice->_envelope._step;
		if (pVoice->_envelope._value < pVoice->_envelope._sustain)
		{
			pVoice->_envelope._value = pVoice->_envelope._sustain;
			pVoice->_envelope._stage = ENV_SUSTAIN;
		}
		break;
	case ENV_RELEASE:
	case ENV_FASTRELEASE:
		pVoice->_envelope._value -= pVoice->_envelope._step;
		if (pVoice->_envelope._value <= 0)
		{
			pVoice->_envelope._value = 0;
			pVoice->_envelope._stage = ENV_OFF;
		}
		break;
	default:break;
	}
}

void Sampler::NoteOff( int voice )
{
	Voice* pVoice = &_voices[voice];
	if (pVoice->_envelope._stage != ENV_OFF)
	{
		const PlayerTimeInfo & timeInfo = callbacks->timeInfo();

		pVoice->_envelope._stage = ENV_RELEASE;
		pVoice->_filterEnv._stage = ENV_RELEASE;
		pVoice->_envelope._step = (pVoice->_envelope._value/callbacks->song()._pInstrument[pVoice->_instrument]->ENV_RT)*(44100.0f/timeInfo.sampleRate());
		pVoice->_filterEnv._step = (pVoice->_filterEnv._value/callbacks->song()._pInstrument[pVoice->_instrument]->ENV_F_RT)*(44100.0f/timeInfo.sampleRate());
	}
}

void Sampler::NoteOffFast( int voice )
{
	Voice* pVoice = &_voices[voice];
	if (pVoice->_envelope._stage != ENV_OFF)
	{
		pVoice->_envelope._stage = ENV_FASTRELEASE;
		pVoice->_envelope._step = pVoice->_envelope._value/OVERLAPTIME;

		pVoice->_filterEnv._stage = ENV_RELEASE;
		pVoice->_filterEnv._step = pVoice->_filterEnv._value/OVERLAPTIME;
	}
}

void Sampler::PerformFx( int voice )
{
	int64_t shift;
	switch(_voices[voice].effCmd)
	{
		// 0x01 : Pitch Up
		case 0x01:
			shift=_voices[voice].effVal*4294967;
			_voices[voice]._wave._speed+=shift;
			resampler_.UpdateSpeed(_voices[voice].resampler_data,_voices[voice]._wave._speed);
		break;

		// 0x02 : Pitch Down
		case 0x02:
			shift=_voices[voice].effVal*4294967;
			_voices[voice]._wave._speed-=shift;
			if ( _voices[voice]._wave._speed < 0 ) _voices[voice]._wave._speed=0;
			resampler_.UpdateSpeed(_voices[voice].resampler_data,_voices[voice]._wave._speed);
		break;
		
		default:
		break;
	}
}


void Sampler::DoPreviews(int amount, float* pLeft, float* pRight)
{
	//todo do better.. use a vector<InstPreview*> or something instead
	if(wavprev.IsEnabled())
	{
		wavprev.Work(pLeft, pRight, amount);
	}
	if(waved.IsEnabled())
	{
		waved.Work(pLeft, pRight, amount);
	}
}

}}
