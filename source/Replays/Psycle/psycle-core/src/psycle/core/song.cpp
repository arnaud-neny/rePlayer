// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "song.h"

#include "fileio.h"
#include "machine.h"
#include "machinefactory.h"
#include "internalkeys.hpp"

#include <psycle/helpers/datacompression.hpp>
#include <psycle/helpers/riff.hpp>

#include <sstream>
#include <iostream> // only for debug output

namespace psycle { namespace core {

using namespace helpers;

SongSerializer CoreSong::serializer_;

CoreSong::CoreSong()
:
	is_ready_()
{
	for(unsigned int i(0); i < MAX_MACHINES; ++i) machines_[i] = 0;
	for(unsigned int i(0); i < MAX_INSTRUMENTS; ++i) _pInstrument[i] = new Instrument;
	clear(); UNIVERSALIS__COMPILER__WARNING("Due to C++ semantics, CoreSong::clear() will be called, even in a derived class that implements clear().")
}

CoreSong::~CoreSong() {
	DeleteAllMachines();
	FreeInstrumentsMemory();
}

void CoreSong::clear() {
	is_ready(false);
	tracks(MAX_TRACKS);
	name("Untitled");
	author("Unnamed");
	comment("No Comments");
	{ // General properties
		bpm(125.0f);
		tick_speed(4);
	}
	// Clean up allocated machines.
	DeleteAllMachines();
	// Cleaning instruments
	DeleteInstruments();
	// Clear patterns and creates a new master_pattern
	sequence().removeAll();
}

bool CoreSong::load(std::string const & filename) {
	is_ready(false);
	bool const result(serializer_.loadSong(filename, *this));
	///\todo: set is_ready to true or to result? what would be better?
	is_ready(true);
	return result;
}

bool CoreSong::save(std::string const & filename, int version) {
	return serializer_.saveSong(filename, *this, version);
}

void CoreSong::AddMachine(Machine * pmac, Machine::id_type newIdx) {
	assert("not null" and pmac); ///\todo if it can't be null, then pass a reference, not a pointer!
	if ( newIdx != -1) {
		if (!machine(newIdx) ){
			pmac->id(newIdx);
		}
	}
	else if ( pmac->id() == -1) {
		if(pmac->IsGenerator()) pmac->id(GetFreeBus());
		else pmac->id(GetFreeFxBus());
		if(pmac->id() == -1) {
			return;
		}
	}
	machine(pmac->id(), pmac);
}

void CoreSong::ReplaceMachine(Machine * pmac, Machine::id_type idx) {
	if(machine(idx)) {
		///\todo:
		//Machine* pOrig = machine(idx);
		//for (int i=0; i < pOrig->GetAudioInputs() && i < pmac->GetAudioInputs(); i++) {
			//if (pOrig->copOrig->getdest
		//}

		DeleteMachine(machine(idx));
	}
	machine(idx, pmac);
}
void CoreSong::ExchangeMachines(Machine::id_type macIdx1, Machine::id_type macIdx2) {
	assert(macIdx1 >= 0 && macIdx2 >= 0 && macIdx1 < MAX_MACHINES && macIdx2 < MAX_MACHINES);
	Machine * mac1 = machines_[macIdx1];
	machines_[macIdx1] = machines_[macIdx2];
	machines_[macIdx2] = mac1;
}
void CoreSong::DeleteAllMachines() {
	for(Machine::id_type c(0); c < MAX_MACHINES; ++c) if(machines_[c]) {
		for(Machine::id_type j(c + 1); j < MAX_MACHINES; ++j) if(machines_[c] == machines_[j]) {
			{ ///\todo wtf? duplicate machine? could happen if loader messes up?
				std::ostringstream s;
				s << c << " and " << j << " have duplicate pointers";
				#if defined PSYCLE__CORE__SIGNALS
					report.emit(s.str(), "duplicate machine found");
				#endif
			}
			machines_[j] = 0;
		}
		DeleteMachine(machines_[c]);
		machines_[c] = 0;
	}
}
void CoreSong::DeleteMachine(Machine * mac) {
	assert("not null" and mac); ///\todo if it can't be null, then pass a reference, not a pointer!
	scoped_lock lock(mutex_);
	mac->DeleteWires(*this);
	// If it's a (Vst)Plugin, the destructor calls to release the underlying library
	try {
		Machine::id_type id = mac->id();
		delete mac;
		machines_[id] = 0;
	} catch(...) {
		///\todo
	}
}

Machine::id_type CoreSong::GetFreeBus() {
	for(unsigned int c(0) ; c < MAX_BUSES ; ++c) if(!machines_[c]) return c;
	return -1; 
}

Machine::id_type CoreSong::GetFreeFxBus() {
	for(unsigned int c(MAX_BUSES) ; c < MAX_BUSES * 2 ; ++c) if(!machines_[c]) return c;
	return -1; 
}

Machine::id_type CoreSong::FindBusFromIndex(Machine::id_type smac) {
	if(!machines_[smac]) return Machine::id_type(255);
	return smac;
}

bool CoreSong::ValidateMixerSendCandidate(Machine & mac, bool rewiring) {
	// Basically, we dissallow a send comming from a generator as well as multiple-outs for sends.
	if(!mac.acceptsConnections()) return false;
    if( ( mac._connectedOutputs > 1 || mac._connectedOutputs > 0) && !rewiring) return false;
	for(int i(0); i < MAX_CONNECTIONS; ++i)
		if(mac._inputCon[i] && !ValidateMixerSendCandidate(*machine(mac._inputMachines[i]), false))
			return false;
	return true;
}

Wire::id_type CoreSong::InsertConnection(Machine & srcMac, Machine & dstMac, InPort::id_type srctype, OutPort::id_type dsttype, float volume) {
	scoped_lock lock(mutex_);
	// Verify that the destination is not a generator
	if(!dstMac.acceptsConnections()) return -1;
	// Verify that src is not connected to dst already, and that destination is not connected to source.
	if(srcMac.FindOutputWire(dstMac.id()) > -1 || dstMac.FindOutputWire(srcMac.id()) > -1) return -1;
	// disallow mixer as a sender of another mixer
	if(srcMac.getMachineKey() == InternalKeys::mixer && dstMac.getMachineKey() == InternalKeys::mixer && dsttype != 0) return -1;
	// If source is in a mixer chain, dissallow the new connection.
	if(srcMac._isMixerSend) return -1;
	// If destination is in a mixer chain (or the mixer itself), validate the sender first
	if(dstMac._isMixerSend || (dstMac.getMachineKey() == InternalKeys::mixer && dsttype == 1))
		if(!ValidateMixerSendCandidate(srcMac)) return -1;
	///\todo: srctype not being used right now.
	return srcMac.ConnectTo(dstMac,dsttype,srctype,volume);
}

bool CoreSong::ChangeWireDestMac(Machine & srcMac, Machine & newDstMac, OutPort::id_type srctype, Wire::id_type wiretochange, InPort::id_type dsttype) {
	scoped_lock lock(mutex_);
	// Verify that the destination is not a generator
	if(!newDstMac.acceptsConnections()) return false;
	// Verify that src is not connected to dst already, and that destination is not connected to source.
	if(srcMac.FindOutputWire(newDstMac.id()) > -1 || newDstMac.FindOutputWire(srcMac.id()) > -1) return false;
	if(srcMac.getMachineKey() == InternalKeys::mixer && newDstMac.getMachineKey() == InternalKeys::mixer && wiretochange >=MAX_CONNECTIONS) return false;
	// If source is in a mixer chain, dissallow the new connection.
	// If destination is in a mixer chain (or the mixer itself), validate the sender first
	if(newDstMac._isMixerSend || (newDstMac.getMachineKey() == InternalKeys::mixer && wiretochange >= MAX_CONNECTIONS))
		///\todo: validate for the case whre srcMac->_isMixerSend
		if(!ValidateMixerSendCandidate(srcMac,true)) return false;
	return srcMac.MoveWireDestTo(newDstMac,srctype,wiretochange,dsttype);
}
bool CoreSong::ChangeWireSourceMac(Machine & newSrcMac, Machine & dstMac, InPort::id_type dsttype, Wire::id_type wiretochange, OutPort::id_type srctype) {
	scoped_lock lock(mutex_);
	// Verify that the destination is not a generator
	if(!dstMac.acceptsConnections()) return false;
	// Verify that src is not connected to dst already, and that destination is not connected to source.
	if(newSrcMac.FindOutputWire(dstMac.id()) > -1 || dstMac.FindOutputWire(newSrcMac.id()) > -1) return false;
	// disallow mixer as a sender of another mixer
	if(newSrcMac.getMachineKey() == InternalKeys::mixer && dstMac.getMachineKey() == InternalKeys::mixer && wiretochange >= MAX_CONNECTIONS) return false;
	// If source is in a mixer chain, dissallow the new connection.
	if(newSrcMac._isMixerSend ) return false;
	// If destination is in a mixer chain (or the mixer itself), validate the sender first
	if(dstMac._isMixerSend || (dstMac.getMachineKey() == InternalKeys::mixer && wiretochange >= MAX_CONNECTIONS))
		if(!ValidateMixerSendCandidate(newSrcMac,false)) return false;
	return dstMac.MoveWireSourceTo(newSrcMac,dsttype,wiretochange,srctype);
}

// IFF structure ripped by krokpitr
// Current Code Extremely modified by [JAZ] ( RIFF based )
// Advise: IFF files use Big Endian byte ordering, so use
// ReadBE/WriteBE instead of Read/Write.

/*
** IFF Riff Header
** ----------------

/// "FORM"
char Id[4]
/// of the data contained in the file (except Id and length)
ULONGINV hlength
/// "16SV" == 16bit . 8SVX == 8bit
char type[4]

/// "NAME"
char name_Id[4]
/// of the data contained in the header "NAME". It is 22 bytes
ULONGINV hlength
/// name of the sample.
char name[22]

/// "VHDR"
char vhdr_Id[4]
/// of the data contained in the header "VHDR". it is 20 bytes
ULONGINV hlength
/// Lenght of the sample. It is in bytes, not in Samples.
ULONGINV Samplength
/// Start point for the loop. It is in bytes, not in Samples.
ULONGINV loopstart
/// Length of the loop (so loopEnd = loopstart+looplenght) In bytes.
ULONGINV loopLength
/// Always $20 $AB $01 $00 //
unsigned char unknown2[5];
unsigned char volumeHiByte;
unsigned char volumeLoByte;
unsigned char unknown3;

/// "BODY"
char body_Id[4]
/// of the data contained in the file. It is the sample length as well (in bytes)
ULONGINV hlength
/// the sample.
char *data

*/

bool CoreSong::IffAlloc(Instrument::id_type instrument,const char * str) {
	scoped_lock lock(mutex_);
	RiffFile file;
	RiffChunkHeader hd;
	char fourCC[4];
	int bits = 0;
	// opens the file and reads the "FORM" header.
	if(!file.Open(str)) return false;
	DeleteLayer(instrument);
	file.ReadArray(fourCC,4);
	if(file.matchFourCC(fourCC,"16SV")) bits = 16;
	else if(file.matchFourCC(fourCC,"8SVX")) bits = 8;
	file.Read(hd);
	if(file.matchFourCC(hd.id, "NAME")) {
		///\todo should be hd.size instead of "22", but it is incorrectly read.
		file.ReadArray(_pInstrument[instrument]->waveName, 22); _pInstrument[instrument]->waveName[21] = 0;
		std::strncpy(_pInstrument[instrument]->_sName,str, 31);
		_pInstrument[instrument]->_sName[31]='\0';
		file.Read(hd);
	}
	if(file.matchFourCC(hd.id, "VHDR")) {
		uint32_t Datalen, ls, le;
		Datalen = ls = le = -1;
		file.ReadBE(Datalen);
		file.ReadBE(ls);
		file.ReadBE(le);
		if(bits == 16) {
			Datalen >>= 1;
			ls >>= 1;
			le >>= 1;
		}
		_pInstrument[instrument]->waveLength = Datalen;
		if(ls != le) {
			_pInstrument[instrument]->waveLoopStart = ls;
			_pInstrument[instrument]->waveLoopEnd = ls + le;
			_pInstrument[instrument]->waveLoopType = true;
		}
		file.Skip(8); // Skipping unknown bytes (and volume on bytes 6&7)
		file.Read(hd);
	}
	if(file.matchFourCC(hd.id, "BODY")) {
		int16_t * csamples;
		uint32_t const Datalen(_pInstrument[instrument]->waveLength);
		_pInstrument[instrument]->waveStereo = false;
		_pInstrument[instrument]->waveDataL = new int16_t[Datalen];
		csamples = _pInstrument[instrument]->waveDataL;
		if(bits == 16) for(unsigned int smp(0) ; smp < Datalen; ++smp) {
			file.ReadBE(*csamples);
			++csamples;
		} else for(unsigned int smp(0) ; smp < Datalen; ++smp) {
			int8_t tmp;
			file.Read(tmp);
			*csamples = tmp * 0x101;
			++csamples;
		}
	}
	file.Close();
	return true;
}

bool CoreSong::WavAlloc(Instrument::id_type iInstr, bool bStereo, int32_t iSamplesPerChan, const char * pathToWav) {
	assert(iSamplesPerChan < (1 << 30)); // Since in some places, signed values are used, we cannot use the whole range.
	DeleteLayer(iInstr);
	_pInstrument[iInstr]->waveDataL = new int16_t[iSamplesPerChan];
	if(bStereo) {
		_pInstrument[iInstr]->waveDataR = new int16_t[iSamplesPerChan];
		_pInstrument[iInstr]->waveStereo = true;
	} else _pInstrument[iInstr]->waveStereo = false;
	_pInstrument[iInstr]->waveLength = iSamplesPerChan;

	// Get the filename -- code adapted from: 
	// http://www.programmersheaven.com/mb/CandCPP/318649/318649/readmessage.aspx
	///\todo the code below is not nice
	char fileName[255]; 
	//char slash = File::slash().c_str()[0]; // note: a single forward slash seems to work on both windows and linux.
	char const *ptr = std::strrchr(pathToWav, '/'); // locate filename part of path.
	if (ptr) {
		std::strcpy(fileName, ptr + 1); // copy remainder of string
	}
	else {
		std::strcpy(fileName, pathToWav); // copy remainder of string
	}
	ptr = std::strchr(fileName, '.'); // strip file extension
	if(ptr) *const_cast<char*>(ptr) = 0; // if the extension exists, truncate it

	std::strncpy(_pInstrument[iInstr]->waveName, fileName, 31);
	_pInstrument[iInstr]->waveName[31] = '\0';
	std::strncpy(_pInstrument[iInstr]->_sName, fileName, 31);
	_pInstrument[iInstr]->_sName[31]='\0';
	return true;
}

bool CoreSong::WavAlloc(Instrument::id_type instrument,const char * pathToWav) { 
	assert(pathToWav != 0);
	WaveFile file;
	ExtRiffChunkHeader hd;
	// opens the file and read the format Header.
	DDCRET retcode(file.OpenForRead(pathToWav));
	if(retcode != DDC_SUCCESS) {
		return false; 
	}
	scoped_lock lock(mutex_);
	// sample type
	int st_type(file.NumChannels());
	int bits(file.BitsPerSample());
	uint32_t Datalen(file.NumSamples());

	// Initializes the layer.
	WavAlloc(instrument, st_type == 2, Datalen, pathToWav);
	// Reading of Wave data.
	// We don't use the WaveFile "ReadSamples" functions, because there are two main differences:
	// We need to convert 8bits to 16bits, and stereo channels are in different arrays.
	int16_t * sampL(_pInstrument[instrument]->waveDataL);

	///\todo use template code for all this semi-repetitive code.

	uint32_t io; /// \todo why is this declared here?
	// mono
	if(st_type == 1) {
		uint8_t smp8;
		switch(bits) {
			case 8:
				for(io = 0 ; io < Datalen ; ++io) {
					file.Read(smp8);
					*sampL = (smp8 << 8) - 32768;
					++sampL;
				}
				break;
			case 16:
				file.ReadData(sampL, Datalen);
				break;
			case 24:
				for(io = 0 ; io < Datalen ; ++io) {
					file.Read(smp8); ///\todo [bohan] is the lsb just discarded? [JosepMa]: yes. sampler only knows about 16bit samples
					file.ReadData(sampL, 1);
					++sampL;
				}
				break;
			default:
				break; ///\todo should throw an exception
		}
	}
	// stereo
	else {
		int16_t *sampR(_pInstrument[instrument]->waveDataR);
		uint8_t smp8;
		switch(bits) {
			case 8:
				for(io = 0 ; io < Datalen ; ++io) {
					file.Read(smp8);
					*sampL = (smp8 << 8) - 32768;
					++sampL;
					file.Read(smp8);
					*sampR = (smp8 << 8) - 32768;
					++sampR;
				}
				break;
			case 16:
				for(io = 0 ; io < Datalen ; ++io) {
					file.ReadData(sampL, 1);
					file.ReadData(sampR, 1);
					++sampL;
					++sampR;
				}
				break;
			case 24:
				for(io = 0 ; io < Datalen ; ++io) {
					file.Read(smp8); ///\todo [bohan] is the lsb just discarded? [JosepMa]: yes. sampler only knows about 16bit samples
					file.ReadData(sampL, 1);
					++sampL;
					file.Read(smp8); ///\todo [bohan] is the lsb just discarded? [JosepMa]: yes. sampler only knows about 16bit samples
					file.ReadData(sampR, 1);
					++sampR;
				}
				break;
			default:
				break; ///\todo should throw an exception
		}
	}
	retcode = file.Read(hd);
	while(retcode == DDC_SUCCESS) {
		if(hd.ckID == FourCC("smpl")) {
			file.Skip(28);
			char pl;
			file.Read(pl);
			if(pl == 1) {
				file.Skip(15);
				uint32_t ls; file.Read(ls);
				uint32_t le; file.Read(le);
				_pInstrument[instrument]->waveLoopStart = ls;
				_pInstrument[instrument]->waveLoopEnd = le;
				// only for my bad sample collection
				if(!((ls <= 0) && (le >= Datalen - 1))) _pInstrument[instrument]->waveLoopType = true;
				else { ls = 0; le = 0; }
			}
			file.Skip(9);
		}
		else if(hd.ckSize > 0) file.Skip(hd.ckSize);
		else file.Skip(1);
		retcode = file.Read(hd); ///\todo bloergh!
	}
	file.Close();
	return true;
}

///\todo mfc+winapi->std
bool CoreSong::CloneIns(Instrument::id_type /*src*/, Instrument::id_type /*dst*/) {
	UNIVERSALIS__COMPILER__WARNING("Instrument cloning is not implemented.")
	// src has to be occupied and dst must be empty
	#if 0
		if(!Gloxxxxxxxxxxxxxxbal::song()._pInstrument[src]->Empty() && !Gloxxxxxxxxxxxxxxxbal::song()._pInstrument[dst]->Empty())
			return false;
		if(!Gloxxxxxxxxxxxxxxxxbal::song()._pInstrument[dst]->Empty()) {
			int temp = src;
			src = dst;
			dst = temp;
		}
		if(Gloxxxxxxxxxxxxxxxxxxbal::song()._pInstrument[src]->Empty())
			return false;

		// ok now we get down to business
		// save our file
		CString filepath = Gloxxxxxxxxxxxxxxxxxxxbal::configuration().GetSongDir().c_str();
		filepath += "\\psycle.tmp";
		::DeleteFile(filepath);
		RiffFile file;
		if (!file.Create(filepath.GetBuffer(1), true))
			return false;

		file.WriteChunk("INSD",4);
		uint32_t version = CURRENT_FILE_VERSION_INSD;
		file.Write(version);
		std::fpos_t pos = file.GetPos();
		uint32_t size = 0;
		file.Write(size);

		uint32_t index = dst; // index
		file.Write(index);

		_pInstrument[src]->SaveFileChunk(&file);

		std::fpos_t pos2 = file.GetPos(); 
		size = pos2 - pos - sizeof size;
		file.Seek(pos);
		file.Write(size);

		file.Close();

		// now load it

		if (!file.Open(filepath.GetBuffer(1))) {
			DeleteFile(filepath);
			return false;
		}
		char Header[5];
		file.ReadChunk(&Header, 4);
		Header[4] = 0;

		if(std::strcmp(Header,"INSD")==0) {
			file.Read(version);
			file.Read(size);
			if(version > CURRENT_FILE_VERSION_INSD) {
				// there is an error, this file is newer than this build of psycle
				file.Close();
				DeleteFile(filepath);
				return false;
			} else {
				file.Read(index);
				index = dst;
				if(index < MAX_INSTRUMENTS) {
					// we had better load it
					_pInstrument[index]->LoadFileChunk(&file,version);
				} else {
					file.Close();
					DeleteFile(filepath);
					return false;
				}
			}
		} else {
			file.Close();
			DeleteFile(filepath);
			return false;
		}
		file.Close();
		DeleteFile(filepath);
	#endif
	return true;
}

void CoreSong::ExchangeInstruments(Instrument::id_type src, Instrument::id_type dst) {
	scoped_lock lock(mutex_);
	assert(src >= 0 && dst >= 0 && src < MAX_INSTRUMENTS && dst < MAX_INSTRUMENTS);
	Instrument* inst1 = _pInstrument[src];
	_pInstrument[src] = _pInstrument[dst];
	_pInstrument[dst] = inst1;
}

void CoreSong::/*Reset*/DeleteInstrument(Instrument::id_type id) {
	scoped_lock lock(mutex_);
	_pInstrument[id]->Reset();
}

void CoreSong::/*Reset*/DeleteInstruments() {
	scoped_lock lock(mutex_);
	for(Instrument::id_type id(0) ; id < MAX_INSTRUMENTS ; ++id) _pInstrument[id]->Reset();
}

void CoreSong::/*Delete*/FreeInstrumentsMemory() {
	for(Instrument::id_type id(0) ; id < MAX_INSTRUMENTS ; ++id) {
		delete _pInstrument[id];
		_pInstrument[id] = 0;
	}
}

void CoreSong::DeleteLayer(Instrument::id_type id) {
	_pInstrument[id]->DeleteLayer();
}

void CoreSong::patternTweakSlide(int /*machine*/, int /*command*/, int /*value*/, int /*patternPosition*/, int /*track*/, int /*line*/) {
	///\todo rework for multitracking
	#if 0
		bool bEditMode = true;

		// UNDO CODE MIDI PATTERN TWEAK
		if (value < 0) value = 0x8000-value;// according to doc psycle uses this weird negative format, but in reality there are no negatives for tweaks..
		if (value > 0xffff) value = 0xffff;// no else incase of neg overflow
		//if(viewMode == VMPattern && bEditMode)
		{
			// write effect
			const int ps = playOrder[patternPosition];
			int line = Gloxxxxxxxxxxxxxxxxxbal::pPlayer()->_lineCounter;
			unsigned char * toffset;
			if(Gloxxxxxxxxxxxxxxxxxxxxbal::pPlayer()->_playing&&Gloxxxxxxxxxxxxxxxxxxbal::pConfig()->_followSong) {
				if(_trackArmedCount) {
					//SelectNextTrack();
				} else if(!Gloxxxxxxxxxxxxxxxxxxxxbal::pConfig()->_RecordUnarmed) return;
				toffset = _ptrack(ps,track)+(line*MULTIPLY);
			} else {
				toffset = _ptrackline(ps, track, line);
			}

			// build entry
			PatternEntry *entry = (PatternEntry*) toffset;
			if(entry->_note >= 120) {
				if((entry->_mach != machine) || (entry->_cmd != ((value>>8)&255)) || (entry->_parameter != (value&255)) || (entry->_inst != command) || ((entry->_note != cdefTweakM) && (entry->_note != cdefTweakE) && (entry->_note != cdefTweakS))) {
					//AddUndo(ps,editcur.track,line,1,1,editcur.track,editcur.line,editcur.col,editPosition);
					entry->_mach = machine;
					entry->_cmd = (value>>8)&255;
					entry->_parameter = value&255;
					entry->_inst = command;
					entry->_note = cdefTweakS;

					//NewPatternDraw(editcur.track,editcur.track,editcur.line,editcur.line);
					//Repaint(DMData);
				}
			}
		}
	#endif
}

/****************************************************************************************************/
// Song

Song::Song() {
	clearMyData();
}

void Song::clear() {
	CoreSong::clear();
	clearMyData();
}

/*void Song::New() {
	SetReady(false);
	clear();
	
}*/

void SetDefaultPatternLines(int /*defaultPatLines*/)
{
	//todo
}
void Song::clearMyData() {
	seqBus = 0;
	machineSoloed = -1;
	_trackSoloed = -1;
	_instSelected = 0;
	midiSelected = 0;
	auxcolSelected = 0;
	_trackArmedCount=0;
	currentOctave=4;
	_saved=false;
	filename("Untitled.psy");
	AddMachine(MachineFactory::getInstance().CreateMachine(InternalKeys::master,MASTER_INDEX));
	Pattern & pattern = *new Pattern();
	pattern.timeSignatures().clear();
	pattern.timeSignatures().push_back(psycle::core::TimeSignature(16.0));
	pattern.setID(0);
	pattern.setName("Untitled");
	sequence().Add(pattern);
	SequenceLine & line = sequence().createNewLine();
	line.createEntry(pattern, 0);
	is_ready(true);
}

void Song::DeleteMachine(Machine * mac)  {
	if(mac->id() == machineSoloed) machineSoloed = -1;
	CoreSong::DeleteMachine(mac);
}

}}
