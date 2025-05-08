#if 0
// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "psy4filter.h"

#include "fileio.h"
#include "song.h"
#include "machinefactory.h"
#include "internalkeys.hpp"
#include "pattern.h"
#include "zipwriter.h"
#include "zipwriterstream.h"
#include "zipreader.h"
#include "xml.h"

#include "signalslib.h"

#ifdef DIVERSALIS__OS__MICROSOFT
	#include <io.h>
#else
	#include <unistd.h>
	#include <sys/types.h>
#endif
#if defined PSYCLE_CORE__CONFIG__LIBXMLPP_AVAILABLE && defined DIVERSALIS__COMPILER__FEATURE__AUTO_LINK
	#pragma comment(lib, "xml++-2.6.lib")
	#pragma comment(lib, "xml2")
	#pragma comment(lib, "glibmm-2.4.lib")
	#pragma comment(lib, "gobject-2.0.lib")
	#pragma comment(lib, "sigc-2.0.lib")
	#pragma comment(lib, "glib-2.0.lib")
	#pragma comment(lib, "intl")
	#pragma comment(lib, "iconv")
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cerrno>
#include <sstream>
#include <iostream> // only for debug output

namespace psycle { namespace core {

Psy4Filter* Psy4Filter::getInstance() {
	// don`t use multithreaded
	static Psy4Filter s;
	return &s; 
}

Psy4Filter::Psy4Filter()
{}

bool Psy4Filter::testFormat( const std::string & fileName )
{
	///\todo this creates a temporary file. need to find a way for all operations to be performed in ram

	zipreader *z;
	zipreader_file *f;
	//int fd = open( fileName.c_str(), O_RDONLY );
	FILE* file1 = fopen(fileName.c_str(), "rb");
	int fd = fileno(file1);
	z = zipreader_open(fd);
	//int outFd = open(std::string("psytemp.xml").c_str(), O_RDWR|O_CREAT|O_TRUNC, 0644);
	FILE* file2 = fopen(std::string("psytemp.xml").c_str(), "wb+");
	int outFd = fileno(file2);
	f = zipreader_seek(z, "xml/song.xml");

	if (!zipreader_extract(f, outFd)) {
		zipreader_close( z );
		//close( outFd );
		//close( fd );
		fclose(file2);
		fclose(file1);
		return false;
	}
	//close( outFd );
	fclose(file2);

	f = zipreader_seek(z, "bin/song.bin");
	//outFd = open(std::string("psytemp.bin").c_str(), O_RDWR|O_CREAT|O_TRUNC, 0644);
	file2 = fopen(std::string("psytemp.bin").c_str(), "wb+");
	outFd = fileno(file2);
	if (!zipreader_extract(f, outFd )) {
		zipreader_close( z );
		//close( outFd );
		//close( fd );
		fclose(file2);
		fclose(file1);
		return false;
	}
	//close( outFd );
	fclose(file2);
	
	zipreader_close( z );
	//close( fd );
	fclose(file1);

#ifndef PSYCLE__CORE__CONFIG__LIBXMLPP_AVAILABLE
	return false;
#else
	xmlpp::DomParser parser;
	parser.parse_file("psytemp.xml");
	if(!parser) return false;
	xmlpp::Element const & root_element(*parser.get_document()->get_root_node());
	return root_element.get_name() == "psy4";
#endif

}

bool Psy4Filter::load(const std::string & /*fileName*/, CoreSong& song)
{
	///\todo this creates a temporary file. need to find a way for all operations to be performed in ram

	std::map<int, Pattern*> patMap;
	patMap.clear();
	song.sequence().removeAll();
	//Disabled since now we load a song in a new Song object *and* song.clear() sets setReady to true.
	//song.clear();
	
	float lastPatternPos = 0;
	PatternCategory* lastCategory = 0;
	Pattern* lastPattern  = 0;
	SequenceLine* lastSeqLine  = 0;
	std::clog << "psy4filter detected for load\n";

#ifdef PSYCLE__CORE__CONFIG__LIBXMLPP_AVAILABLE
	xmlpp::DomParser parser;
	parser.parse_file("psytemp.xml");
	if(!parser) return false;
	xmlpp::Document const & document(*parser.get_document());
	xmlpp::Element const & root_element(*document.get_root_node());

	std::string attrib;

	// Song info.
	{
		xmlpp::Element const& info
			(get_first_element(root_element,"info"));

		try {
			song.setName(
				get_attribute(get_first_element(info,"name"),
					"text"
				).get_value());
		}
		catch(...) {
			std::cerr << "couldn't read name of song" << std::endl;
		}

		try {
			song.setAuthor(
				get_attribute(get_first_element(info,"author"),
					"text"
				).get_value());
		}
		catch(...) {
			std::cerr << "couldn't read author of song" << std::endl;
		}

		try {
			song.setComment(
				get_attribute(get_first_element(info,"comment"),
					"text"
				).get_value());
		}
		catch(...) {
			std::cerr << "couldn't read song comment" << std::endl;
		}
	}

	// Pattern data.
	{
		xmlpp::Element const& pattern_data
			(get_first_element(root_element,"patterndata"));

		xmlpp::Node::NodeList const & categories
			(pattern_data.get_children("category"));

		for(xmlpp::Node::NodeList::const_iterator i = categories.begin(); i != categories.end(); ++i) {

			xmlpp::Element const & category
				(dynamic_cast<xmlpp::Element const &>(**i));
			
			try {
				lastCategory = song.sequence().patternPool()->createNewCategory(get_attribute(category,"name").get_value());
			}
			catch(...) {
				std::cerr << "expected name attribute in category element\n";
			}

			try {
				lastCategory->setColor
					(get_attr<long int>(category,"color"));
			}
			catch(...) {
				// ignore, color is not important
			}

			xmlpp::Node::NodeList const & patterns(category.get_children("pattern"));
			for(xmlpp::Node::NodeList::const_iterator i = patterns.begin(); i != patterns.end(); ++i) {

				try { // each pattern
					xmlpp::Element const & pattern
						(dynamic_cast<xmlpp::Element const &>(**i));

					lastPattern = lastCategory->createNewPattern
						(get_attribute(pattern,"name").get_value());

					lastPattern->clearBars(); ///\todo we just created it.. why does it need cleaning?

					try {
						lastPattern->setBeatZoom
							(str_hex<int>(get_attribute(pattern,"zoom").get_value()));
					}
					catch(...) {
					}
					
					try {
						patMap[str_hex<int>(
							get_attribute(pattern,"id").get_value())
						] = lastPattern;
					}
					catch(...) {
						std::cerr << "expected id attribute in pattern element\n";
					}
					
					try { // pattern signature
						xmlpp::Element const & signature
							(get_first_element(pattern,"sign"));
						
						try {
							lastPattern->addBar(
								TimeSignature(
									get_attr<float>(signature,"free")));
						}
						catch(...) {
							// no 'free' attribute found, so
							// try reading a 'num'/'denom' style time sig
							try {
								TimeSignature sig(
									get_attr<int>(signature,"num"),
									get_attr<int>(signature,"denom"));
								
								sig.setCount(
									get_attr<int>(signature,"count"));
								
								lastPattern->addBar(sig);
							}
							catch(xml_helper_attribute_not_found e) {
								std::cerr << "Missing attribute " << e.attr << " in <sign> element" << std::endl;
							}
						}
					}
					catch(...) {
						std::cerr << "there was a problem reading signature in one of the patterns" << std::endl;
					}
					

					xmlpp::Node::NodeList const & pattern_lines
						(pattern.get_children("patline"));

					for(xmlpp::Node::NodeList::const_iterator i = pattern_lines.begin(); i != pattern_lines.end(); ++i) {
						
						try { // each pattern line
							xmlpp::Element const & pattern_line
								(dynamic_cast<xmlpp::Element const &>(**i));

							try {
								lastPatternPos =
									get_attr<float>(pattern_line,"pos");
							}
							catch(...) {
								std::cerr << "expected pos attribute in patline element\n";
							}
							
							xmlpp::Node::NodeList const & pattern_events
								(pattern_line.get_children("patevent"));
							
							for(xmlpp::Node::NodeList::const_iterator i = pattern_events.begin(); i != pattern_events.end(); ++i) {
								
								try { // each pattern event
									xmlpp::Element const & pattern_event
										(dynamic_cast<xmlpp::Element const &>(**i));
									
									PatternEvent data;
									try {
										data.setMachine
											(get_attr_hex<int>(pattern_event,"mac"));
									}
									catch(...) {
										std::cerr << "expected mac attribute in patevent element\n";
									}
									
									try {
										data.setInstrument
											(get_attr_hex<int>(pattern_event,"inst"));
									}
									catch(...) {
										std::cerr << "expected inst attribute in patevent element\n";
									}
									try {
										data.setNote
											(get_attr_hex<int>(pattern_event,"note"));
									}
									catch(...) {
										std::cerr << "expected note attribute in patevent element\n";
									}
									try {
										data.setParameter
											(get_attr_hex<int>(pattern_event,"param"));
									}
									catch(...) {
										std::cerr << "expected param attribute in patevent element\n";
									}
									
									try {
										data.setCommand
											(get_attr_hex<int>(pattern_event,"cmd"));
									}
									catch(...) {
										std::cerr << "expected cmd attribute in patevent element\n";
									}
									try {
										(*lastPattern)
											[lastPatternPos]
											.notes()
											[get_attr_hex<int>(pattern_event,"track")]
											= data;
									}
									catch(...) {
										std::cerr << "expected track attribute in patevent element\n";
									}
								}
								catch(...) {
									std::cerr << "there was a problem reading a track event" << std::endl;
								}
							}
						}
						catch(...) {
							std::cerr << "there was a problem reading a pattern line" << std::endl;
						}
					}
				}
				catch(...) {
					std::cerr << "There was a problem reading a pattern" << std::endl;
				}
			}
		}
	}


	// Sequence data.
	{
		xmlpp::Element const & sequence
			(get_first_element(root_element,"sequence"));

		xmlpp::Node::NodeList const & sequencer_lines(sequence.get_children("seqline"));
		for(xmlpp::Node::NodeList::const_iterator i = sequencer_lines.begin(); i != sequencer_lines.end(); ++i) {
			xmlpp::Element const & sequencer_line(dynamic_cast<xmlpp::Element const &>(**i));
			lastSeqLine = song.sequence().createNewLine();
			xmlpp::Node::NodeList const & sequencer_entries(sequencer_line.get_children("seqentry"));
			for(xmlpp::Node::NodeList::const_iterator i = sequencer_entries.begin(); i != sequencer_entries.end(); ++i) {
				xmlpp::Element const & sequencer_entry(dynamic_cast<xmlpp::Element const &>(**i));
				std::string id;
				try {
					id = get_attribute(sequencer_entry,"patid").get_value();
				}
				catch(...) {
					std::cerr << "expected patid attribute in seqentry element\n";
					continue;
				}
				std::map<int, Pattern*>::iterator it =
					patMap.find(str<int>(id));
				if(it == patMap.end())
					continue;
				
				Pattern * pattern(it->second);
				if (!pattern)
					continue;

				double pos(0);
				try {
					pos = get_attr<double>(sequencer_entry,"pos");
				}
				catch(...) {
					std::cerr << "expected pos attribute in seqentry element\n";
					continue;
				}

				SequenceEntry * entry =
					lastSeqLine->createEntry(pattern, pos);

				try {
					entry->setStartPos
						(get_attr<float>(sequencer_entry,"start"));
				}
				catch(...) {}

				try {
					entry->setEndPos
						(get_attr<float>(sequencer_entry,"end"));
				}
				catch(...) {}

				try {
					entry->setTranspose
						(get_attr<int>(sequencer_entry,"transpose"));
				}
				catch(...) {}
			}
		}
	}

	if (true) {
		RiffFile file;
		file.Open("psytemp.bin");
		//progress.emit(1,0,"");
		//progress.emit(2,0,"Loading... psycle bin data ...");

		// skip header
		file.Skip(8);
		///\todo:
		size_t filesize = file.FileSize();
		uint32_t version = 0;
		uint32_t size = 0;
		char header[5];
		header[4]=0;
		uint32_t chunkcount = LoadSONGv0(&file,song);
		std::clog << chunkcount << std::endl;

		/* chunk_loop: */
		while(file.ReadArray(header, 4) && chunkcount)
		{
			file.Read(version);
			file.Read(size);

			int fileposition = file.GetPos();
			//progress.emit(4,static_cast<int>(fileposition*16384.0f)/filesize),"");

			if(std::strcmp(header,"MACD") == 0)
			{
				//progress.emit(2,0,"Loading... Song machines...");
				if ((version&0xFF00) == 0x0000) // chunkformat v0
				{
					LoadMACDv0(&file,song,version&0x00FF);
				}
				else if ( (version&0xFF00) == 0x0100 ) //and so on
				{
					loadMACDv1(&file,song,version&0x00FF);
				}
			}
			else if(std::strcmp(header,"INSD") == 0)
			{
				//progress.emit(2,0,"Loading... Song instruments...");
				if ((version&0xFF00) == 0x0000) // chunkformat v0
				{
					LoadINSDv0(&file,song,version&0x00FF);
				}
				//else if ( (version&0xFF00) == 0x0100 ) //and so on
			}
			else
			{
				//loggers::warning("foreign chunk found. skipping it.");
				//progress.emit(2,0,"Loading... foreign chunk found. skipping it...");
				std::ostringstream s;
					s << "foreign chunk: version: " << version << ", size: " << size;
				//loggers::trace(s.str());
				if(size && size < filesize-fileposition)
				{
					file.Skip(size);
				}
			}
			// For invalid version chunks, or chunks that haven't been read correctly/completely.
			if(file.GetPos() != fileposition+size)
			{
			///\todo: verify how it works with invalid data.
			//if (file.GetPos() > fileposition+size) loggers::trace("Cursor ahead of size! resyncing with chunk size.");
			//else loggers::trace("Cursor still inside chunk, resyncing with chunk size.");
				file.Seek(fileposition+size);
			}
			--chunkcount;
		}
		//progress.emit(4,16384,"");

		///\todo: Move this to something like "song.validate()" 

		// test all connections for invalid machines. disconnect invalid machines.
		for(int i(0) ; i < MAX_MACHINES ; ++i)
		{
			if(song.machine(i))
			{
				song.machine(i)->_connectedInputs = 0;
				song.machine(i)->_connectedOutputs = 0;
				for (int c(0) ; c < MAX_CONNECTIONS ; ++c)
				{
					if(song.machine(i)->_connection[c])
					{
						if(song.machine(i)->_outputMachines[c] < 0 || song.machine(i)->_outputMachines[c] >= MAX_MACHINES)
						{
							song.machine(i)->_connection[c] = false;
							song.machine(i)->_outputMachines[c] = -1;
						}
						else if(!song.machine(song.machine(i)->_outputMachines[c]))
						{
							song.machine(i)->_connection[c] = false;
							song.machine(i)->_outputMachines[c] = -1;
						}
						else 
						{
							song.machine(i)->_connectedOutputs++;
						}
					}
					else
					{
						song.machine(i)->_outputMachines[c] = -1;
					}

					if (song.machine(i)->_inputCon[c])
					{
						if (song.machine(i)->_inputMachines[c] < 0 || song.machine(i)->_inputMachines[c] >= MAX_MACHINES)
						{
							song.machine(i)->_inputCon[c] = false;
							song.machine(i)->_inputMachines[c] = -1;
						}
						else if (!song.machine(song.machine(i)->_inputMachines[c]))
						{
							song.machine(i)->_inputCon[c] = false;
							song.machine(i)->_inputMachines[c] = -1;
						}
						else
						{
							song.machine(i)->_connectedInputs++;
						}
					}
					else
					{
						song.machine(i)->_inputMachines[c] = -1;
					}
				}
			}
		}

		//progress.emit(5,0,"");
		if(chunkcount)
		{
			if (!song.machine(MASTER_INDEX) )
			{
				song.addMachine(MachineFactory::getInstance().CreateMachine(InternalKeys::master(),MASTER_INDEX));
			}
			std::ostringstream s;
			s << "Error reading from file '" << file.file_name() << "'" << std::endl;
			s << "some chunks were missing in the file";
			//report.emit(s.str(), "Song Load Error.");
		}
		///\todo:
	}

	#endif
	return true;
} // load

bool Psy4Filter::save( const std::string & file_Name, const CoreSong& song )
{
	//FIXME: Verify if this was needed for something. It was preventing to save to the correct directory.
	// std::string fileName = File::extractFileNameFromPath(file_Name);
	const std::string &fileName = file_Name;

	bool autosave = false;
	///\todo:
	if ( !autosave )
	{
		//progress.emit(1,0,"");
		//progress.emit(2,0,"Saving...");
	}

	// ideally, you should create a temporary file on the same physical
	// disk as the target zipfile... 

	//zipwriter *z = zipwriter_start(open(fileName.c_str(), O_RDWR|O_CREAT, 0644));
	FILE* file1 = fopen(fileName.c_str(), "wb+");
	zipwriter *z = zipwriter_start(fileno(file1));
	zipwriterfilestream xmlFile(z, "xml/song.xml" );

	std::ostringstream xml;
	xml << "<psy4>" << std::endl;
	xml << "<info>" << std::endl;
	xml << "<name text='" << replaceIllegalXmlChr( song.name() ) << "' />" << std::endl;
	xml << "<author text='" << replaceIllegalXmlChr( song.author() ) << "' />" << std::endl;;
	xml << "<comment text='" << replaceIllegalXmlChr( song.comment() ) << "' />" << std::endl;;
	xml << "</info>" << std::endl;
	// xml << song.sequence().patternPool().toXml(); todo
	xml << song.sequence().toXml();
	xml << "</psy4>" << std::endl;

	xmlFile << xml.str();
	xmlFile.close();


	zipwriter_file *f;


	///\todo:
	if ( !autosave )
	{
		//progress.emit(1,0,"");
		//progress.emit(2,0,"Saving binary data...");
	}

	// we create here a temp file, cause our RiffFile is a fstream
	// and the zipwriter is only a ostream
	// modifiying the Rifffile makes it possible
	// to write direct into the zip without using a temp here

	RiffFile file;
	file.Create(std::string("psycle_tmp.bin").c_str(), true);

	file.WriteArray("PSY4",4);
	saveSONGv0(&file,song);

	for(int32_t index(0) ; index < MAX_MACHINES; ++index)
	{
		if (song.machine(index))
		{
			saveMACDv1(&file,song,index);
			if ( !autosave )
			{
				//progress.emit(4,-1,"");
			}
		}
	}
	for(int index(0) ; index < MAX_INSTRUMENTS; ++index)
	{
		if (!song._pInstrument[index]->Empty())
		{
			saveINSDv0(&file,song,index);
			if ( !autosave )
			{
				//progress.emit(4,-1,"");
			}
		}
	}
	///\todo:
	file.Close();

	// copy the bin data to the zip
	f = zipwriter_addfile(z, std::string("bin/song.bin").c_str(), 9);
	//zipwriter_copy(open("psycle_tmp.bin", O_RDONLY), f);
	FILE* file2 = fopen("psycle_tmp.bin", "rb");
	zipwriter_copy(fileno(file2), f);
	fclose(file2);
	if (!zipwriter_finish(z)) {
		std::cerr << "Zipwriter failed." << std::endl;
		return false;
	}

	// remove temp file
	if( std::remove("psycle_tmp.bin") == -1 ) std::cerr << "Error deleting temp file" << std::endl;
	return true;
}

int Psy4Filter::LoadSONGv0(RiffFile* file,CoreSong& /*song*/)
{
	int32_t fileversion = 0;
	uint32_t size = 0;
	uint32_t chunkcount = 0;
	file->Read(fileversion);
	file->Read(size);
	if(fileversion > CURRENT_FILE_VERSION)
	{
		//report.emit("This file is from a newer version of Psycle! This process will try to load it anyway.", "Load Warning");
	}
	file->Read(chunkcount);
	const int bytesread=4;
	if ( size > 4)
	{
		file->Skip(size - bytesread); // Size of the current header DATA // This ensures that any extra data is skipped.
	}
	return chunkcount;
}

bool Psy4Filter::saveSONGv0( RiffFile * file, const CoreSong& song )
{
	uint32_t chunkcount;
	uint32_t version, size;
	// chunk header;

	file->WriteArray("SONG",4);
	version = CURRENT_FILE_VERSION;
	file->Write(version);
	size = sizeof chunkcount;
	file->Write(size);

	chunkcount = 3; // 3 chunks plus:
	for(int i(0) ; i < MAX_MACHINES; ++i)
			if(song.machine(i)) ++chunkcount;
	for(int i(0) ; i < MAX_INSTRUMENTS ; ++i)
			if(!song._pInstrument[i]->Empty()) ++chunkcount;

	file->Write(chunkcount);

	return true;
}

bool Psy4Filter::loadMACDv1( RiffFile * file, CoreSong& song, int minorversion )
{
	MachineFactory& factory = MachineFactory::getInstance();
	uint32_t index=0, host=0, keyindex=0;
	char sDllName[256];

	// chunk data
	
	file->Read(index);
	file->Read(host);
	file->ReadString(sDllName,256);
	file->Read(keyindex);
	MachineKey key(Hosts::type(host), sDllName, keyindex, true);
	Machine* mac = factory.CreateMachine(key,index);
	if (mac) {
		song.AddMachine(mac);
		return song.machine(index)->LoadFileChunk(file,minorversion);
	} else {
		mac = factory.CreateMachine(InternalKeys::dummy,index);
		mac->SetEditName(mac->GetEditName() + " (replaced)");
		song.AddMachine(mac);
		return false;
	}
}

bool Psy4Filter::saveMACDv1( RiffFile * file, const CoreSong& song, int index )
{
	uint32_t version, size;
	std::size_t pos;

	// chunk header

	file->WriteArray("MACD",4);
	version = CURRENT_FILE_VERSION_MACD;
	file->Write(version);
	pos = file->GetPos();
	size = 0;
	file->Write(size);

	// chunk data

	const MachineKey& key = song.machine(index)->getMachineKey();
	file->Write(uint32_t(index));
	file->Write(uint32_t(key.host()));
	file->WriteString(key.dllName());
	// file->Write(uint32_t(key.index())); ?
	
	song.machine(index)->SaveFileChunk(file);

	// chunk size in header

	std::size_t const pos2 = file->GetPos();
	size = static_cast<uint32_t>(pos2 - pos) - sizeof(size);
	file->Seek(pos);
	file->Write(size);
	file->Seek(pos2);

	///\todo:
	return true;
}

bool Psy4Filter::saveINSDv0( RiffFile * file, const CoreSong& song, int index )
{
	uint32_t version, size;
	std::size_t pos;

	// chunk header

	file->WriteArray("INSD",4);
	version = CURRENT_FILE_VERSION_INSD;
	file->Write(version);
	pos = file->GetPos();
	size = 0;
	file->Write(size);

	// chunk data

	file->Write(uint32_t(index));
	song._pInstrument[index]->SaveFileChunk(file);

	// chunk size in header

	std::size_t const pos2 = file->GetPos();
	size = static_cast<uint32_t>(pos2 - pos) - sizeof(size);
	file->Seek(pos);
	file->Write(size);
	file->Seek(pos2);

	///\todo:
	return true;
}

bool Psy4Filter::saveWAVEv0( RiffFile * /*file*/, const CoreSong& /*song*/, int /*index*/ )
{
	return false;
}

}}
#endif
