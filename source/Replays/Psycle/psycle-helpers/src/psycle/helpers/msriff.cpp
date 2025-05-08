/**
	\file
	implementation file for psycle::helpers::eaiff
*/
#include "msriff.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace psycle { namespace helpers {

	//const RiffChunkId MsRiff::grabbag = {' ',' ',' ',' '};
	const IffChunkId MsRiff::RIFF = {'R','I','F','F'};
	const IffChunkId MsRiff::RIFX = {'R','I','F','X'};
	//sizeof(RiffChunkHeader) does not give 8.
	const std::size_t RiffChunkHeader::SIZEOF = 8;
	const std::size_t RifxChunkHeader::SIZEOF = 8;

//////////////////////////////////////////////////////////////////////////
		bool MsRiff::isValidFile() const { return isValid;}

		void MsRiff::Open(const std::string& fname) {
			isValid=false;
			isLittleEndian=true;
			AbstractIff::Open(fname);
			readHeader();
			if (currentHeaderLE.matches(RIFF)) {
				isLittleEndian=true;
				isValid=true;
				currentHeader=&currentHeaderLE;
			}
			else if (currentHeaderLE.matches(RIFX)) {
				isLittleEndian=false;
				//Read it again as big endian
				Seek(0);
				readHeader();
				isValid=true;
				currentHeader=&currentHeaderBE;
			}
		}
		void MsRiff::Create(const std::string& fname, bool overwrite, bool littleEndian) {
			AbstractIff::Create(fname, overwrite);
			isLittleEndian=littleEndian;
			isValid=true;
			if(littleEndian) {
				RiffChunkHeader mainhead(RIFF,0);
				currentHeader=&currentHeaderLE;
				addNewChunk(mainhead);
			}
			else {
				RifxChunkHeader mainhead(RIFX,0);
				currentHeader=&currentHeaderBE;
				addNewChunk(mainhead);
			}
		}
		void MsRiff::Close() { 
			if (isWriteMode() && isValid) {
				std::streamsize size = fileSize();
				UpdateFormSize(0,size-RiffChunkHeader::SIZEOF);
			}
			AbstractIff::Close(); 
		}
		bool MsRiff::Eof() const { return AbstractIff::Eof(); }

		void MsRiff::addNewChunk(const BaseChunkHeader& header) {
			if (GetPos()%2 > 0) Write('\0');
			headerPosition=GetPos();
			Write(header.id);
			Write(header.length());
			BaseChunkHeader::copyId(header.id, currentHeader->id);
			currentHeader->setLength(header.length());
		}


		const BaseChunkHeader& MsRiff::readHeader() {
			char skippedchar = '\0';
			if(GetPos()%2 > 0 && tryPaddingFix==false) {
				Read(skippedchar);
			}
			tryPaddingFix=false;

			headerPosition = static_cast<uint32_t>(GetPos());
			if (isLittleEndian) {
				Read(currentHeaderLE.id);
				Read(currentHeaderLE.ulength);
			}
			else {
				Read(currentHeaderBE.id);
				Read(currentHeaderBE.ulength);
			}
			if(skippedchar >= ' ' && skippedchar < '~' && //If the skipped character is a good candidate for ID and
					(currentHeaderLE.id[3] < ' ' || currentHeaderLE.id[3] > '~' //the last char doesn't look like an id character
					|| currentHeaderLE.ulength.unsignedValue()+GetPos() > GuessedFilesize() // or the size is unusually high
					|| currentHeaderLE.ulength.unsignedValue() == 0)) { // or the high is 0
				tryPaddingFix=true;
			}
			return *currentHeader;
		}

		const BaseChunkHeader& MsRiff::findChunk(const IffChunkId& id, bool allowWrap) {
			while(!Eof()) {
				readHeader();
				if (currentHeader->matches(id)) return *currentHeader;
				if (tryPaddingFix) {
					Seek(headerPosition-static_cast<std::streampos>(1));
					continue;
				}
				else if (currentHeader->length() > 0) {
					Skip(currentHeader->length());
				}
			}
			if (allowWrap && Eof()) {
				Seek(12);//Skip RIFF/RIFX chunk
				return findChunk(id,false);
			}
			RiffChunkHeader head(id,0);
			throw std::runtime_error(std::string("couldn't find chunk ") + head.id);
		}
		void MsRiff::skipThisChunk() {
			if (headerPosition+static_cast<std::streampos>(RiffChunkHeader::SIZEOF+currentHeader->length()) >= GuessedFilesize()) {
				Seek(GuessedFilesize());//Seek to end (so we can have EOF)
			}
			else {
				Seek(headerPosition + static_cast<std::streamoff>(RiffChunkHeader::SIZEOF + currentHeader->length()));
			}
		}
		void MsRiff::UpdateCurrentChunkSize()
		{
			std::streampos chunksize = GetPos();
			chunksize-=headerPosition+static_cast<std::streamoff>(RiffChunkHeader::SIZEOF);
			UpdateFormSize(headerPosition, chunksize);
		}
}}
