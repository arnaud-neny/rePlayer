// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "psy3filter.h"
#include "fileio.h"
#include "song.h"
#include "machinefactory.h"
#include "internalkeys.hpp"
#include "mixer.h"
#include "vstplugin.h"
#include <psycle/helpers/datacompression.hpp>
#include <universalis/os/loggers.hpp>
#include <diversalis/os.hpp>
#include <sstream>

namespace psycle { namespace core {

namespace loggers = universalis::os::loggers;
using namespace helpers;

std::string const Psy3Filter::FILE_FOURCC = "PSY3";
/// Current version of the Song file and its chunks.
/// format: 0x0000AABB
/// A = Major version. It can't be loaded, skip the whole chunk. (Right now the loader does it, so simply do nothing)
/// B = minor version. It can be loaded with the existing loader, but not all information will be available.
uint32_t const Psy3Filter::VERSION_INFO = 0x0000;
uint32_t const Psy3Filter::VERSION_SNGI = 0x0000;
uint32_t const Psy3Filter::VERSION_SEQD = 0x0000;
uint32_t const Psy3Filter::VERSION_PATD = 0x0000;
uint32_t const Psy3Filter::VERSION_MACD = 0x0000;
uint32_t const Psy3Filter::VERSION_INSD = 0x0000;
uint32_t const Psy3Filter::VERSION_WAVE = 0x0000;

uint32_t const Psy3Filter::FILE_VERSION =
	Psy3Filter::VERSION_INFO +
	Psy3Filter::VERSION_SNGI +
	Psy3Filter::VERSION_SEQD +
	Psy3Filter::VERSION_PATD +
	Psy3Filter::VERSION_PATD +
	Psy3Filter::VERSION_MACD +
	Psy3Filter::VERSION_INSD +
	Psy3Filter::VERSION_WAVE;

Psy3Filter * Psy3Filter::getInstance() {
	///\todo
	static Psy3Filter s;
	return &s;
}

Psy3Filter::Psy3Filter()
:
	singleCat(),
	singleLine()
{}

bool Psy3Filter::testFormat(const std::string & fileName) {
	RiffFile file;
	file.Open(fileName);
	char header[9];
	file.ReadArray(header, 8);
	header[8] = 0;
	file.Close(); ///\todo RiffFile's dtor doesn't close!!!
	return !strcmp(header, "PSY3SONG");
}

bool Psy3Filter::load(const std::string & fileName, CoreSong & song) {
	song.progress(1,0,"");
	song.progress(2,0,"Loading... psycle song fileformat version 3...");
	if(loggers::trace()) {
		std::ostringstream s;
		s << "psycle: core: psy3 loader: loading psycle song fileformat version 3: " << fileName;
		loggers::trace()(s.str());
	}

	RiffFile file;
	file.Open(fileName);

	// skip header
	file.Skip(8);
	//char Header[9];
	//file.ReadChunk(&Header, 8);
	//Header[8]=0;

	//Disabled since now we load a song in a new Song object *and* song.clear() sets setReady to true.
	//song.clear();
	seqList.clear();
	song.sequence().removeAll();
	// here we add in one single Line the patterns
	singleLine = &song.sequence().createNewLine();
	
	//size_t filesize = file.FileSize();
	uint32_t version = 0;
	uint32_t size = 0;
	bool problemfound = false;
	size_t fileposition = 0;
	char header[5];
	header[4]=0;
	uint32_t chunkcount = LoadSONGv0(&file,song);
	/* chunk_loop: */
	size_t filesize = file.FileSize();

	while(file.ReadArray(header, 4) && chunkcount) {
		int progress_number =  static_cast<int>(file.GetPos() * 16384.0f / filesize);
		song.progress(4, progress_number, "");

		if(!std::strcmp(header,"INFO")) {
			song.progress(2, progress_number, "Loading... Song authorship information...");
			--chunkcount;
			problemfound=false;
			file.Read(version);
			file.Read(size);
			fileposition = file.GetPos();
			if((version & 0xff00) == 0) { // chunkformat v0
				LoadINFOv0(&file, song, version & 0xff);
				//bug in psycle 1.8.5, writes size as 12bytes more!
				if(version == 0) size = song.name().length()+song.author().length()+song.comment().length() + 3;
			}
			//else if((version & 0xff00) == 0x0100) // and so on
		} else if(!std::strcmp(header,"SNGI")) {
			song.progress(2, progress_number, "Loading... Song properties information...");
			--chunkcount;
			problemfound=false;
			file.Read(version);
			file.Read(size);
			fileposition = file.GetPos();
			if((version & 0xff00) == 0) { // chunkformat v0
				LoadSNGIv0(&file, song, version & 0xff);
				// fix for a bug existing in the song saver in the 1.7.x series
				if(version == 0) size = 11 * sizeof(uint32_t) + song.tracks() * 2 * sizeof(bool);
			}
			//else if((version & 0xff00) == 0x0100) // and so on
		} else if(!std::strcmp(header,"SEQD")) {
			song.progress(2, progress_number, "Loading... Song sequence...");
			--chunkcount;
			problemfound=false;
			file.Read(version);
			file.Read(size);
			fileposition = file.GetPos();
			if ((version & 0xff00) == 0) { // chunkformat v0
				LoadSEQDv0(&file, song, version & 0xff);
			}
			//else if((version & 0xff00) == 0x0100) // and so on
		} else if(!std::strcmp(header,"PATD")) {
			song.progress(2, progress_number, "Loading... Song patterns...");
			--chunkcount;
			problemfound=false;
			file.Read(version);
			file.Read(size);
			fileposition = file.GetPos();
			if((version  & 0xff00) == 0) { // chunkformat v0
				LoadPATDv0(&file, song, version & 0xff);
				//\ Fix for a bug existing in the Song Saver in the 1.7.x series
				if((version == 0x0000) &&( file.GetPos() == fileposition+size+4)) size += 4;
			}
			//else if((version & 0xff00) == 0x0100) // and so on
		} else if(!std::strcmp(header,"MACD")) {
			song.progress(2, progress_number, "Loading... Song machines...");
			--chunkcount;
			problemfound=false;
			file.Read(version);
			file.Read(size);
			fileposition = file.GetPos();
			if((version & 0xff00) == 0) {// chunkformat v0
				Machine* mac = LoadMACDv0(&file, song, version & 0xff);
				//Bugfix.
				#if defined DIVERSALIS__OS__MICROSOFT
					if (mac->getMachineKey().host() == Hosts::VST
						&& ((vst::plugin*)mac)->ProgramIsChunk() == false) {
							size = static_cast<uint32_t>(file.GetPos() - fileposition);
					}
				#else
                if(mac!=nullptr) {
                    //unused warning.
					///\todo
                }
				#endif
			}
			//else if((version & 0xff00) == 0x0100 ) //and so on
		} else if(!std::strcmp(header,"INSD")) {
			song.progress(2, progress_number, "Loading... Song instruments...");
			--chunkcount;
			problemfound=false;
			file.Read(version);
			file.Read(size);
			fileposition = file.GetPos();
			if((version & 0xff00) == 0) {// chunkformat v0
				LoadINSDv0(&file,song,version&0x00FF);
			}
			//else if((version & 0xff00) == 0x0100) // and so on
		} else if(!std::strcmp(header,"EINS")) {
			song.progress(2, progress_number, "Loading... Extended Song instruments...");
			--chunkcount;
			problemfound=false;
			file.Read(version);
			file.Read(size);
			fileposition = file.GetPos();
			if((version & 0xffff0000) == EINS_VERSION_ONE) {// chunkformat v1
				LoadEINSv1(&file,song,version&0xFFFF, size);
			}
			//else if((version & 0xff00) == 0x0100) // and so on
		} else {
			if (!problemfound) {
				//song.progress.emit(2, 0, "Loading... invalid position or unknown chunk found. skipping it...");
				if(loggers::warning()) {
					std::ostringstream s;
					s << "psycle: core: psy3 loader: skipping invalid position or unknown chunk found:" << header << ", position:" << file.GetPos();
					loggers::warning()(s.str());
				}
			}
			problemfound = true;
			// shift back 3 bytes and try again
			fileposition = file.GetPos()-3;
			size = 0;
		}
		// For invalid version chunks, or chunks that haven't been read correctly/completely.
		if(file.GetPos() != fileposition+size) {
			///\todo: verify how it works with invalid data.
			//if (file.GetPos() > fileposition+size) loggers::trace()("Cursor ahead of size! resyncing with chunk size.");
			//else loggers::trace()("Cursor still inside chunk, resyncing with chunk size.");
			file.Seek(fileposition+size);
		}
	}

	
	song.progress(4,16384,"");

	///\todo: Move this to something like "song.validate()"

	// now that we have loaded all the patterns, time to prepare them.
	double pos = 0;
	std::vector<int>::iterator it = seqList.begin();
	for(; it < seqList.end(); ++it) {
		#if 0
			Pattern* pat = song.sequence().PatternPool()->findById(*it);
		#else
			Pattern* pat = song.sequence().FindPattern(*it);
		#endif
		assert(pat);
		singleLine->createEntry(*pat, pos);
		pos += pat->beats();
	}

	// test all connections for invalid machines. disconnect invalid machines.
	
	for(int i(0) ; i < MAX_MACHINES ; ++i) if(song.machine(i)) {
		Machine* mac = song.machine(i);
		mac->_connectedInputs = 0;
		mac->_connectedOutputs = 0;
		for(int c(0) ; c < MAX_CONNECTIONS ; ++c) {
			if(mac->_connection[c]) {
				if(mac->_outputMachines[c] < 0 || mac->_outputMachines[c] >= MAX_MACHINES) {
					mac->_connection[c] = false;
					mac->_outputMachines[c] = -1;
				} else if(!song.machine(mac->_outputMachines[c])) {
					mac->_connection[c] = false;
					mac->_outputMachines[c] = -1;
				} else ++mac->_connectedOutputs;
			} else mac->_outputMachines[c] = -1;

			if(mac->_inputCon[c]) {
				if(mac->_inputMachines[c] < 0 || mac->_inputMachines[c] >= MAX_MACHINES) {
					mac->_inputCon[c] = false;
					mac->_inputMachines[c] = -1;
				} else if (!song.machine(mac->_inputMachines[c])) {
					mac->_inputCon[c] = false;
					mac->_inputMachines[c] = -1;
				} else {
					++mac->_connectedInputs;
					mac->_wireMultiplier[c]=song.machine(mac->_inputMachines[c])->GetAudioRange() / mac->GetAudioRange();
				}
			} else song.machine(i)->_inputMachines[c] = -1;
		}
	}
	RestoreMixerSendFlags(song);
	//song.progress.emit(5,0,"");
	if(chunkcount) {
		if(!song.machine(MASTER_INDEX)) {
			Machine* mac = MachineFactory::getInstance().CreateMachine(InternalKeys::master, MASTER_INDEX);
			song.AddMachine(mac);
		}
		std::ostringstream s;
		s << "psycle: core: psy3 loader: error reading from file '" << file.file_name()
		<< "'. Some chunks were missing in the file.";
		loggers::warning()(s.str());
		song.report(s.str(), "Song Load Error.");
	}
	///\todo:
	return true;
}

int Psy3Filter::LoadSONGv0(RiffFile* file,CoreSong& song) {
	int32_t fileversion = 0;
	uint32_t size = 0;
	uint32_t chunkcount = 0;

	file->Read(fileversion);
	file->Read(size);
	if(fileversion > CURRENT_FILE_VERSION) {
		std::ostringstream s;
		s << "This file is from a newer version of Psycle! This process will try to load it anyway.";
		loggers::warning()(s.str());
		song.report(s.str(), "Load Warning");
	}

	file->Read(chunkcount);
	int bytesread(4);
	if(size > 4) {
		// This is left here if someday, extra data is added to the file version chunk.
		// update "bytesread" accordingly.

		//file->Read(chunkversion);
		//if((chunkversion&0xFF00) ) == x) {} else if(...) {}

		file->Skip(size - bytesread);// Size of the current header DATA // This ensures that any extra data is skipped.
	}
	return chunkcount;
}

bool Psy3Filter::LoadINFOv0(RiffFile* file,CoreSong& song,int /*minorversion*/) {
	char Name[129]; char Author[65]; char Comment[65536];

	file->ReadString(Name, 128);
	song.name(Name);
	file->ReadString(Author, 64);
	song.author(Author);
	bool result = file->ReadString(Comment, 65535);
	song.comment(Comment);
	return result;
}

bool Psy3Filter::LoadSNGIv0(RiffFile* file,CoreSong& song,int /*minorversion*/) {
	MachineCallbacks* callbacks = MachineFactory::getInstance().getCallbacks();
	int32_t temp(0);
	int16_t temp16(0);
	bool fileread = false;

	// why all these temps?  to make sure if someone changes the defs of
	// any of these members, the rest of the file reads ok.  assume
	// everything is 32-bit, when we write we do the same thing.

	// # of tracks for whole song
	file->Read(temp);
	song.tracks(temp);
	// bpm
	{ ///\todo: This was a hack added in 1.9alpha to allow decimal BPM values
		file->Read(temp16);
		int BPMCoarse = temp16;
		file->Read(temp16);
		song.bpm(BPMCoarse + temp16 / 100.0f );
	}
	// linesperbeat
	file->Read(temp);
	song.tick_speed(temp);
	linesPerBeat = song.tick_speed();

	// current octave
	file->Read(temp);
	octave = static_cast<unsigned char>(temp);
	// machineSoloed
	file->Read(machineSoloed);
	// trackSoloed
	file->Read(trackSoloed);
	file->Read(seqBus);
	file->Read(midiSelected);
	file->Read(auxcolSelected);
	file->Read(instSelected);

	// sequence width, for multipattern
	file->Read(temp);
	for(unsigned int i(0) ; i < song.tracks(); ++i) {
		bool tmp=0;
		file->Read(tmp);
		song.sequence().setMutedTrack(i,tmp);
		fileread = file->Read(tmp);
		song.sequence().setArmedTrack(i,tmp);
	}
	callbacks->timeInfo().setBpm(song.bpm());
	callbacks->timeInfo().setTicksSpeed(song.tick_speed(),song.is_ticks());
	return fileread;
}

bool Psy3Filter::LoadSEQDv0(RiffFile* file,CoreSong& /*song*/,int /*minorversion*/) {
	int32_t index(0);
	uint32_t temp(0);
	bool fileread = false;
	// index, for multipattern - for now always 0
	file->Read(index);
	if(index < MAX_SEQUENCES) {
		char pTemp[256];
		// play length for this sequence
		file->Read(temp);
		int playLength = temp;
		// name, for multipattern, for now unused
		file->ReadString(pTemp, sizeof pTemp);
		for(int i(0) ; i < playLength; ++i) {
			fileread = file->Read(temp);
			seqList.push_back(temp);
		}
	}
	return fileread;
}

bool Psy3Filter::LoadPATDv0(RiffFile* file,CoreSong& song,int /*minorversion*/) {
	bool fileread = false;

	// index
	int32_t index = 0;
	file->Read(index);
	//todo: This loading is quite slow now. Needs an improvement
	if(index < MAX_PATTERNS) {
		// num lines
		uint32_t temp = 0;
		file->Read(temp);
		// clear it out if it already exists
		unsigned int numLines = temp;
		// num tracks per pattern // eventually this may be variable per pattern, like when we get multipattern
		file->Read(temp);
		char patternName[32];
		file->ReadString(patternName, sizeof patternName);

		// create a Pattern
		Pattern & pat = *new Pattern();
		pat.setID(index);
		{
			std::ostringstream s;
			s << index;
			pat.setName(patternName + s.str());
		}
		song.sequence().Add(pat);

		float beatpos = 0;

		{ // read pattern data
			unsigned char * data_start;
			{
				uint32_t size = 0;
				file->Read(size);
				unsigned char * compressed = new unsigned char[size];
				fileread = file->ReadArray(compressed, size);
				DataCompression::BEERZ77Decomp2(compressed, &data_start);
				delete[] compressed;
			}
			unsigned char * data = data_start;
			for(unsigned int y = 0; y < numLines ; ++y) { // lines
				for (unsigned int x = 0; x < song.tracks(); ++x) { // tracks
					PatternEvent event;
					event.setNote(*data); ++data;
					event.setInstrument(*data); ++data;
					event.setMachine(*data); ++data;
					event.setCommand(*data); ++data;
					event.setParameter(*data); ++data;
					if(!event.empty()) {
						if(event.note() == notetypes::tweak) {
                            pat.insert( beatpos, x, event);
						} else if(event.note() == notetypes::tweak_slide) {
                            pat.insert( beatpos, x, event);
						} else if(event.note() == notetypes::midi_cc) {
                            pat.insert( beatpos, x, event);
						///\todo: Also, move the Global commands (tempo, mute..) out of the pattern.
						} else {
							if(event.command() == commandtypes::NOTE_DELAY)
								/// Convert old value (part of line) to new value (part of beat)
								event.setParameter(event.parameter() / linesPerBeat);
                            pat.insert( beatpos, x, event);
						}
						if(
							(event.note() <= notetypes::release || event.note() == notetypes::empty) &&
							event.command() == commandtypes::EXTENDED &&
							event.parameter() < commandtypes::extended::SET_BYPASS
						) linesPerBeat = event.parameter();
					}
				}
				beatpos += 1.0f / linesPerBeat;
			}
			delete[] data_start;
		}
		pat.timeSignatures().clear();
		pat.timeSignatures().push_back(TimeSignature(beatpos));
	}
	return fileread;
}

Machine* Psy3Filter::LoadMACDv0(RiffFile * file, CoreSong & song, int minorversion) {
	MachineFactory& factory = MachineFactory::getInstance();
	int32_t index = 0;

	file->Read(index);
	if(index < MAX_MACHINES) {
		Machine::id_type const id(index);
		// assume version 0 for now
		int32_t type = -1;
		Machine* mac=0;
		char sDllName[256];
		file->Read(type);
		file->ReadString(sDllName, 256);
		bool failedLoad = false;
		switch(type) {
			case MACH_MASTER:
				mac = factory.CreateMachine(InternalKeys::master, MASTER_INDEX);
				break;
			case MACH_SAMPLER:
				mac = factory.CreateMachine(InternalKeys::sampler, id);
				break;
			case MACH_MIXER:
				mac = factory.CreateMachine(InternalKeys::mixer, id);
				break;
			case MACH_XMSAMPLER:
				mac = factory.CreateMachine(InternalKeys::sampulse, id);
				break;
			case MACH_DUPLICATOR:
				mac = factory.CreateMachine(InternalKeys::duplicator, id);
				break;
			case MACH_AUDIOINPUT:
				mac = factory.CreateMachine(InternalKeys::audioinput, id);
				break;
			//case MACH_LFO:
				//mac = factory.CreateMachine(InternalKeys::lfo, id);
				//break;
			//case MACH_SCOPE:
			case MACH_DUMMY:
				mac = factory.CreateMachine(InternalKeys::dummy, id);
				break;
			case MACH_PLUGIN:
			{
				mac = factory.CreateMachine(MachineKey(Hosts::NATIVE, sDllName, 0, true), id);
				break;
			}
			case MACH_VST:
			case MACH_VSTFX:
			{
				mac = factory.CreateMachine(MachineKey(Hosts::VST, sDllName, 0), id);
				break;
			}
			default: ;
		}
		if(!mac) {
			if(loggers::warning()) {
				std::ostringstream s;
				s << "Problem loading machine! type: " << type << ", dllName: " << sDllName;
				loggers::warning()(s.str());
			}
			mac = factory.CreateMachine(InternalKeys::dummy, id);
			failedLoad = true;
		}
		song.AddMachine(mac);
		mac->LoadFileChunk(file,minorversion);
		if(failedLoad) mac->SetEditName(mac->GetEditName() + " (replaced)");
	}
	return song.machine(index);
}

bool Psy3Filter::LoadINSDv0(RiffFile* file,CoreSong& song,int minorversion) {
	int32_t index(0);
	file->Read(index);
	if(index < MAX_INSTRUMENTS) song._pInstrument[index]->LoadFileChunk(file, minorversion, true);
	///\todo:
	return true;
}

bool Psy3Filter::LoadEINSv1(RiffFile* file, CoreSong& song, int minorversion, uint32_t size) {

	// Instrument Data Load
	int numInstruments;
	size_t begins=file->GetPos();
	size_t filepos=file->GetPos();
	file->Read(numInstruments);
	int idx = -1;
	for(int i = 0;i < numInstruments && filepos < begins+size;i++)
	{
		file->Read(idx);
		filepos = file->GetPos();
		size_t sizeIns = song.rInstrument(idx).Load(*file);
		if ((minorversion&0xFFFF) > 0) {
			//Version 0 doesn't write the chunk size correctly
			//so we cannot correct it in case of error
			file->Seek(filepos+sizeIns);
			filepos=file->GetPos();
		}
	}
	int numSamples;
	file->Read(numSamples);
	for(int i = 0;i < numSamples && filepos < begins+size;i++)
	{
		file->Read(idx);
		filepos = file->GetPos();
		unsigned int sizeSamp = song.SampleData(idx).Load(*file);
		if ((minorversion&0xFFFF) > 0) {
			//Version 0 doesn't write the chunk size correctly
			//so we cannot correct it in case of error
			file->Seek(filepos+sizeSamp);
			filepos=file->GetPos();
		}
	}
	filepos=file->GetPos();
	return begins+size == filepos;

}

void Psy3Filter::RestoreMixerSendFlags(CoreSong& song) {
	for(int i(0);i < MAX_MACHINES; ++i) if(song.machine(i) && song.machine(i)->getMachineKey() == InternalKeys::mixer) {
		Mixer & mac = static_cast<Mixer&>(*song.machine(i));
        for(unsigned int j(0); j < mac.numreturns(); ++j) if(mac.Return(j).IsValid())
			song.machine(mac.Return(j).Wire().machine_)->SetMixerSendFlag(&song);
	}
}

}}
