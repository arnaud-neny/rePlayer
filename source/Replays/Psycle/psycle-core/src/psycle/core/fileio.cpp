// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

///\implementation psycle::core::RiffFile

#include <psycle/core/detail/project.private.hpp>
#include "fileio.h"

#include <stdexcept>
#include <cstring>

namespace psycle { namespace core {

bool RiffFile::ReadString(std::string & result) {
	result = "";
	for(;;) {
		char c;
		if(!ReadChunk(&c, sizeof c)) return false;
		if(!c) return true;
		result += c;
	}
}

bool RiffFile::ReadString(char * data, std::size_t max_length) {
	if(max_length <= 0) return false;
	std::memset(data, 0, max_length);
	int c = EOF;
	for(std::size_t index = 0; index < max_length; ++index) if((c = stream_.get()) != EOF) {
		data[index] = c;
		if(c == '\0') return true;
	}
	#if 0 ///\todo : put this back! it was for the case where the string was longer than max_length
	{
		char c;
		do {
			if(!Read(c)) return false; //\todo : return false, or return true? the string is read already. it could be EOF.
		} while(c);
	}
	#endif
	return c == EOF;
}
bool RiffFile::WriteString(std::string const & inString) {
	WriteChunk(inString.c_str(), inString.length()+1);
	return true;
}

bool RiffFile::Open(std::string const & filename) {
	is_write_mode_ = false;
	file_name_ = filename;
	stream_.open(file_name_.c_str(), std::ios_base::in | std::ios_base::binary);
	if(!stream_.is_open()) return false;
	stream_.seekg (0, std::ios::beg);
	return 1;
}

bool RiffFile::Create(std::string const & filename, bool overwrite) {
	is_write_mode_ = true;
	file_name_ = filename;
	if(!overwrite) {
		std::fstream filetest(file_name_.c_str(), std::ios_base::in | std::ios_base::binary);
		if(filetest.is_open()) {
			filetest.close();
			return false;
		}
	}
	stream_.open(file_name_.c_str (), std::ios_base::out | std::ios_base::trunc |std::ios_base::binary);
	return stream_.is_open();
}

void RiffFile::Close() {
	stream_.close();
}

bool RiffFile::Error() {
	return !stream_.good();
}

bool RiffFile::ReadChunk(void * data, std::size_t bytes) {
	if(stream_.eof()) return false;
	stream_.read(reinterpret_cast<char*>(data), bytes);
	if(stream_.eof()) return false;
	if(stream_.bad()) return false;
	return true;
}

bool RiffFile::WriteChunk(void const * data, std::size_t bytes) {
	stream_.write(reinterpret_cast<char const *>(data), bytes);
	if(stream_.bad()) return false;
	return true;
}

bool RiffFile::Expect(void * data, std::size_t bytes) {
	char * chars(reinterpret_cast<char*>(data));
	std::size_t count(bytes);
	while(count--) {
		unsigned char c;
		stream_.read(reinterpret_cast<char*>(&c), sizeof c);
		if(stream_.eof()) return false;
		if(c != *chars) return false;
		++chars;
	}
	return true;
}

std::size_t RiffFile::Seek(std::ptrdiff_t bytes) {
	if(is_write_mode_)
		stream_.seekp(bytes, std::ios::beg);
	else
		stream_.seekg(bytes, std::ios::beg);
	if(stream_.eof()) throw std::runtime_error("seek failed");
	return GetPos();
}

std::size_t RiffFile::Skip(std::ptrdiff_t bytes) {
	if(is_write_mode_)
		stream_.seekp(bytes, std::ios::cur);
	else
		stream_.seekg(bytes, std::ios::cur);
	if(stream_.eof()) throw std::runtime_error("seek failed");
	return GetPos();
}

bool RiffFile::Eof() {
	return stream_.eof();
}

std::size_t RiffFile::FileSize() {
	// save pos
	std::size_t curPos = GetPos();
	// goto end of file
	stream_.seekg(0, std::ios::end);
	// read the filesize
	std::size_t fileSize = stream_.tellg();
	// go back to saved pos
	stream_.seekg(curPos);
	return fileSize;
}

std::size_t RiffFile::GetPos() {
	return is_write_mode_ ?
		stream_.tellp() :
		stream_.tellg();
}

#if 0 // unfinished
/*********************************************************************************/
// MemoryFile

MemoryFile::MemoryFile() {}
MemoryFile::~MemoryFile(){}

bool MemoryFile::OpenMem(std::ptrdiff_t blocksize) { return false; }
bool MemoryFile::CloseMem() { return false; }
std::size_t MemoryFile::FileSize() { return 0; }
std::size_t MemoryFile::GetPos() { return 0; }
std::size_t MemoryFile::Seek(std::ptrdiff_t bytes) { return 0; }
std::size_t MemoryFile::Skip(std::ptrdiff_t bytes) { return 0; }
bool MemoryFile::ReadString(char *, std::size_t max_length) { return false; }
bool MemoryFile::WriteChunk(void const *, std::size_t){ return false; }
bool MemoryFile::ReadChunk (void       *, std::size_t){ return false; }
bool MemoryFile::Expect    (void       *, std::size_t){ return false; }
#endif

}}
