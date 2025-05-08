/**
	\file
	implementation file for psycle::helpers::abstractIff
*/
#include "abstractiff.hpp"
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace psycle { namespace helpers {

	void BaseChunkHeader::copyId(const IffChunkId& idOrigin, IffChunkId& idDest) {
		idDest[0]=idOrigin[0]; idDest[1]=idOrigin[1];
		idDest[2]=idOrigin[2]; idDest[3]=idOrigin[3];
	}
	std::string BaseChunkHeader::idString() const {
		const char id2[5] = {id[0],id[1],id[2],id[3],'\0'}; return id2;
	}
	bool BaseChunkHeader::matches(const IffChunkId& id2) const {
		return (id[0] == id2[0]) && (id[1] == id2[1]) && (id[2] == id2[2]) && (id[3] == id2[3]);
	}

	AbstractIff::AbstractIff() {
	}

	AbstractIff::~AbstractIff() {
	}

	void AbstractIff::Open(const std::string & fname) {
		write_mode = false;
		file_name_ = fname;
		_stream.open(file_name_.c_str(), std::ios_base::in | std::ios_base::binary);
		if (!_stream.is_open ()) throw std::runtime_error("stream open failed");
		filesize = fileSize();
		_stream.seekg (0, std::ios::beg);
	}

	void AbstractIff::Create(const std::string & fname, bool const overwrite) {
		write_mode = true;
		file_name_ = fname;
		filesize = 0;
		if(!overwrite)
		{
			std::fstream filetest(file_name_.c_str (), std::ios_base::in | std::ios_base::binary);
			if (filetest.is_open ())
			{
				filetest.close();
				throw std::runtime_error("stream already exists");
			}
		}
		_stream.open(file_name_.c_str (), std::ios_base::out | std::ios_base::trunc |std::ios_base::binary);
	}

	void AbstractIff::Close() {
		_stream.close();
	}

	bool AbstractIff::Eof() const {
		int thissize = filesize;
		if (write_mode) { thissize = const_cast<AbstractIff*>(this)->fileSize(); }
		//good checks whether none of the error flags (eofbit, failbit and badbit) are set
		return (const_cast<AbstractIff*>(this)->GetPos() >= (std::streampos)filesize || !_stream.good());
	}

	std::streamsize AbstractIff::fileSize() {
		std::streampos curPos = GetPos();
		_stream.seekg(0, std::ios::end);         // goto end of file
		std::streampos fileSize = _stream.tellg();  // read the filesize
		_stream.seekg(curPos);                   // go back to begin of file
		return fileSize;
	}

	void AbstractIff::ReadString(std::string &result) {
		result.clear();
		char c = '\0';

		Read(c);
		while(c != '\0') {
			result += c;
			Read(c);
		}
	}
	void AbstractIff::ReadString(char *data, std::size_t const max_length) {
		if (max_length <= 0 ) {
			throw std::runtime_error("erroneous max_length passed to ReadString");
		}

		memset(data,0,max_length);
		std::string temp;
		ReadString(temp);
		strncpy(data,temp.c_str(), std::min(temp.length(),max_length)-1);
	}

	void AbstractIff::ReadSizedString(char *data, std::size_t const read_length) {
		memset(data,0,read_length);
		ReadRaw(data,read_length);
	}

	void AbstractIff::ReadPString(std::string &result) {
		uint8_t size;
		char temp[257];
		Read(size);
		ReadSizedString(temp,size);
		if(GetPos()%2 > 0) Skip(1);
		temp[size]='\0';
		result=temp;
	}


	void AbstractIff::WriteString(const std::string &string) {
		WriteRaw(string.c_str(),string.length());
	}

	void AbstractIff::WriteString(const char * const data) {
		std::size_t idx = 0;
		while(data[idx] != '\0') ++idx;
		WriteRaw(data, idx);
	}

	void AbstractIff::WriteSizedString(const char * const data, std::size_t const length) {
		if (length <= 0 ) {
			throw std::runtime_error("erroneous max_length passed to WriteString");
		}
		std::size_t idx = 0;
		while(data[idx] != '\0' && idx < length) ++idx;
		WriteRaw(data, idx);
		char c = '\0';
		while(idx < length) Write(c);
	}



	std::streampos AbstractIff::GetPos(void) {
		return  (write_mode) ? _stream.tellp() : _stream.tellg();
	}
	std::streampos AbstractIff::Seek(std::streampos const bytes){
		if (write_mode) {
			_stream.seekp(bytes, std::ios::beg);
		} else {
			_stream.seekg(bytes, std::ios::beg);
		}
		if (_stream.eof()) throw std::runtime_error("seek failed");
		return GetPos();
	}

	std::streampos AbstractIff::Skip(std::streampos const bytes) {
		if (write_mode) {
			_stream.seekp(bytes, std::ios::cur);
		} else { 
			_stream.seekg(bytes, std::ios::cur);
		}
		if (_stream.eof()) throw std::runtime_error("skip failed");
		return GetPos();
	}


	bool AbstractIff::Expect(const IffChunkId& id) 
	{
		return Expect(id,4);
	}

	bool AbstractIff::Expect(const void * const data, std::size_t const bytes)
	{
		const char * chars(reinterpret_cast<const char*>(data));
		std::size_t count(bytes);
		while(count--)
		{
			unsigned char c;
			Read(c);
			if(c != *chars) return false;
			++chars;
		}
		return true;
	}

	void AbstractIff::UpdateFormSize(std::streamoff headerposition, uint32_t numBytes)
	{
		std::streampos currentPos = GetPos();
		Seek(headerposition + sizeof(IffChunkId));
		Write(numBytes);
		Seek(currentPos);
	}


}}
