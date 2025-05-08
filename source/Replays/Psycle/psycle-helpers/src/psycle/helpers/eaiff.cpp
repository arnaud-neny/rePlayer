/**
	\file
	implementation file for psycle::helpers::eaiff
*/
#include "eaiff.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace psycle { namespace helpers {

		const IffChunkId EaIff::grabbag = {' ',' ',' ',' '};
		const IffChunkId EaIff::FORM = {'F','O','R','M'};
		const IffChunkId EaIff::LIST = {'L','I','S','T'};
		const IffChunkId EaIff::CAT = {'C','A','T',' '};
		//sizeof(IffChunkHeader) does not give 8.
		const std::size_t IffChunkHeader::SIZEOF = 8;


///////////////////////////////////////////////////////
		bool EaIff::isValidFile() const { return isValid;}

		void EaIff::Open(const std::string& fname) {
			isValid=false;
			AbstractIff::Open(fname);
			readHeader();
			if (currentHeader.matches(FORM)) {
				isValid=true;
			}
		}
		void EaIff::Create(const std::string& fname, const bool overwrite) { AbstractIff::Create(fname, overwrite); }
		void EaIff::Close() { 
			if (isWriteMode()) {
				std::streamsize size = fileSize();
				UpdateFormSize(0,size-IffChunkHeader::SIZEOF);
			}
			AbstractIff::Close(); 
		}
		bool EaIff::Eof() const { return AbstractIff::Eof(); }

		void EaIff::addNewChunk(const BaseChunkHeader& header)
		{
			if (GetPos()%2 > 0) Write('\0');
			headerPosition=static_cast<uint32_t>(GetPos());
			Write(header.id);
			Write(header.length());
			BaseChunkHeader::copyId(header.id, currentHeader.id);
			currentHeader.setLength(header.length());
		}
        void EaIff::addFormChunk(const IffChunkId& /*id*/) {
		}
        void EaIff::addListChunk(const IffChunkId& /*id*/, bool /*endprevious*/) {
		}
        void EaIff::addCatChunk(const IffChunkId& /*id*/, bool /*endprevious*/) {
		}
        void EaIff::addListProperty(const IffChunkId& /*contentId*/, const IffChunkId& /*propId*/) {
		}
        void EaIff::addListProperty(const IffChunkId& /*contentId*/, const IffChunkId& /*propId*/, void const * /*data*/, uint32_t /*dataSize*/) {
		}

		const IffChunkHeader& EaIff::readHeader() {
			if(GetPos()%2 > 0) Skip(1);
			headerPosition = static_cast<uint32_t>(GetPos());
			Read(currentHeader.id);
			Read(currentHeader.ulength);
			return currentHeader;
		}
		const IffChunkHeader& EaIff::findChunk(const IffChunkId& id, bool allowWrap) {
			while(!Eof()) {
				const IffChunkHeader& header = readHeader();
				if (header.matches(id)) return header;
				Skip(header.length());
				if (header.length()%2)Skip(1);
			};
			if (allowWrap && Eof()) {
				Seek(12);//Skip FORM chunk
				return findChunk(id,false);
			}
			IffChunkHeader head(id,0);
			throw std::runtime_error(std::string("couldn't find chunk ") + head.id);
		}
		void EaIff::skipThisChunk() {
			if (headerPosition+static_cast<std::streampos>(IffChunkHeader::SIZEOF+currentHeader.length()) >= GuessedFilesize()) {
				Seek(GuessedFilesize());//Seek to end (so we can have EOF)
			}
			else {
				Seek(headerPosition + static_cast<std::streamoff>(IffChunkHeader::SIZEOF + currentHeader.length()));
				if (GetPos() % 2 > 0) Skip(1);
			}
		}

		void EaIff::WriteHeader(const IffChunkHeader& header) {
			if (GetPos()%2 > 0) Write('\0');
			headerPosition=static_cast<uint32_t>(GetPos());
			Write(header.id);
			Write(header.ulength);
			currentHeader=header;
		}

		void EaIff::UpdateCurrentChunkSize()
		{
			std::streampos chunksize = GetPos();
			chunksize-=headerPosition+static_cast<std::streamoff>(IffChunkHeader::SIZEOF);
			UpdateFormSize(headerPosition, chunksize);
		}

}}
