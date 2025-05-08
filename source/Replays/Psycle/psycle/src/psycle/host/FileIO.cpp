
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
// For nice and portable code, get the file from psycle-core
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

///\file
///\brief implementation file for psycle::host::RiffFile.

#include <psycle/host/detail/project.private.hpp>
#include "FileIO.hpp"
#include "Song.hpp"
#if !defined WINAMP_PLUGIN
	#include "ProgressDialog.hpp"
#else
	#include "player_plugins/winamp/fake_progressDialog.hpp"
#endif //!defined WINAMP_PLUGIN

namespace psycle
{
	namespace host
	{
		uint32_t RiffFile::FourCC(char const * null_terminated_string)
		{
			uint32_t result(0x20202020); // four spaces (padding)
			char * chars(reinterpret_cast<char *>(&result));
			for(int i(0) ; i < 4 && null_terminated_string[i] ; ++i) *chars++ = null_terminated_string[i];
			return result;
		}

		bool RiffFile::Open(std::string const & FileName)
		{
			///\todo WinAPI code removed in trunk
			DWORD bytesRead;
			szName = FileName;
			_modified = false;
			_handle = ::CreateFile(FileName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
			if(_handle == INVALID_HANDLE_VALUE) return false;
			if(!ReadFile(_handle, &_header, sizeof(_header), &bytesRead, 0))
			{
				CloseHandle(_handle);
				_handle = INVALID_HANDLE_VALUE;
				return false;
			}
			return true;
		}

		bool RiffFile::Create(std::string const & FileName, bool overwrite)
		{
			DWORD bytesWritten;
			szName = FileName;
			_modified = false;
			_handle = ::CreateFile(FileName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, overwrite ? CREATE_ALWAYS : CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
			if(_handle == INVALID_HANDLE_VALUE) return false;
			_header._id = FourCC("RIFF");
			_header._size = 0;
			if(!WriteFile(_handle, &_header, sizeof(_header), &bytesWritten, 0))
			{
				CloseHandle(_handle);
				_handle = INVALID_HANDLE_VALUE;
				return false;
			}
			return true;
		}

		bool RiffFile::Close()
		{
			if(_handle == INVALID_HANDLE_VALUE) return false;
			DWORD bytesWritten;
			if (_modified)
			{
				Seek(0);
				WriteFile(_handle, &_header, sizeof(_header), &bytesWritten, 0);
				_modified = false;
			}
			bool b = CloseHandle(_handle);
			_handle = INVALID_HANDLE_VALUE;
			return b;
		}

		bool RiffFile::ReadInternal(void* pData, std::size_t numBytes)
		{
			DWORD bytesRead;
			if(!ReadFile(_handle, pData, numBytes, &bytesRead, 0)) return false;
			return bytesRead == numBytes;
		}

		bool RiffFile::WriteInternal(void const * pData, std::size_t numBytes)
		{
			DWORD bytesWritten;
			if(!WriteFile(_handle, pData, numBytes, &bytesWritten, 0)) return false;
			_modified = true;
			_header._size += bytesWritten;
			return bytesWritten == numBytes;
		}

		bool RiffFile::Expect(const void* pData, std::size_t numBytes)
		{
			DWORD bytesRead;
			unsigned char c;
			while(numBytes-- != 0)
			{
				if(!ReadFile(_handle, &c, sizeof(c), &bytesRead, 0)) return false;
				if(c != *((char*)pData)) return false;
				pData = (char*)pData + 1;
			}
			return true;
		}

		int RiffFile::Seek(std::size_t offset)
		{
			return SetFilePointer(_handle, offset, 0, FILE_BEGIN);
		}

		int RiffFile::Skip(std::size_t numBytes)
		{
			return SetFilePointer(_handle, numBytes, 0, FILE_CURRENT); 
		}

		bool RiffFile::Eof()
		{
			DWORD bytesRead=0;
			unsigned char c;
			std::size_t pos = GetPos();
			ReadFile(_handle, &c, 1, &bytesRead, 0);
			Seek(pos);
			return(bytesRead==0);
		}

		std::size_t RiffFile::GetPos()
		{
			return SetFilePointer(_handle, 0, 0, FILE_CURRENT);
		}

		std::size_t RiffFile::FileSize()
		{
			const std::size_t init = GetPos();
			const std::size_t end = SetFilePointer(_handle,0,0,FILE_END);
			SetFilePointer(_handle,init,0,FILE_BEGIN);
			return end;
		}

		bool RiffFile::ReadString(std::string & result)
		{
			result="";
			char c;
			while(true)
			{
				if (Read(c))
				{
					if(c == 0) {
						return true;
					}
					result+=c;
				}
				else return false;
			}
		}

		bool RiffFile::ReadString(char* pData, std::size_t maxBytes)
		{
			if(maxBytes > 0)
			{
				std::memset(pData,0,maxBytes);

				char c;
				for(std::size_t index = 0; index < maxBytes-1; index++)
				{
					if (Read(c))
					{
						pData[index] = c;
						if(c == 0) return true;
					}
					else return false;
				}
				do
				{
					if(!Read(c)) return false; //\todo : return false, or return true? the string is read already. it could be EOF.
				} while(c != 0);
				return true;
			}
			return false;
		}

		bool RiffFile::WriteString(const std::string & result)
		{
			Write(result.c_str(),result.length());
			return Write('\0'); // NULL terminator
		}

		std::size_t RiffFile::WriteHeader(const char* pData, uint32_t version, uint32_t size/*=0*/)
		{
			Write(pData,4);
			Write(version);
			size_t pos = GetPos();
			Write(size);
			return pos;
		}
		uint32_t RiffFile::UpdateSize(std::size_t startpos)
		{
			size_t pos2 = GetPos(); 
			uint32_t size = static_cast<uint32_t>(pos2-startpos-sizeof(uint32_t));
			Seek(startpos);
			Write(size);
			Seek(pos2);
			return size;
		}


		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// OldPsyFile

		bool OldPsyFile::Load(Song &song,CProgressDialog& progress, bool fullopen/*=true*/)
		{
			return song.Load(this, progress, fullopen);
		}

		bool OldPsyFile::Open(std::string const & FileName)
		{
			szName = FileName;
			_file = std::fopen(FileName.c_str(), "rb");
			return _file != 0;
		}

		bool OldPsyFile::Create(std::string const & FileName, bool overwrite)
		{
			szName = FileName;
			_file = std::fopen(FileName.c_str(), "rb");
			if(_file != 0)
			{
				std::fclose(_file);
				if(!overwrite) return false;
			}
			_file = std::fopen(FileName.c_str(), "wb");
			return _file != 0;
		}

		bool OldPsyFile::Close()
		{
			if( _file != 0)
			{
				std::fflush(_file);
				bool b = !ferror(_file);
				std::fclose(_file);
				_file = 0;
				return b;
			}
			return true;
		}

		bool OldPsyFile::ReadInternal(void* pData, std::size_t numBytes)
		{
			std::size_t bytesRead = std::fread(pData, sizeof(char), numBytes, _file);
			return bytesRead == numBytes;
		}

		bool OldPsyFile::WriteInternal(void const * pData, std::size_t numBytes)
		{
			std::size_t bytesWritten = std::fwrite(pData, sizeof(char), numBytes, _file);
			if (numBytes > 1024) std::fflush(_file);
			return bytesWritten == numBytes;
		}

		bool OldPsyFile::Expect(const void* pData, std::size_t numBytes)
		{
			char c;
			while (numBytes-- != 0)
			{
				if(std::fread(&c, sizeof c, 1, _file) != 1) return false;
				if(c != *((char*)pData)) return false;
				pData = (char*)pData + 1;
			}
			return true;
		}

		int OldPsyFile::Seek(std::size_t offset)
		{
			if(std::fseek(_file, offset, SEEK_SET) != 0) return -1;
			return std::ftell(_file);
		}

		int OldPsyFile::Skip(std::size_t numBytes)
		{
			if(std::fseek(_file, numBytes, SEEK_CUR) != 0) return -1;
			return std::ftell(_file);
		}

		bool OldPsyFile::Eof()
		{
			return static_cast<bool>(feof(_file));
		}

		std::size_t OldPsyFile::FileSize()
		{
			int init(std::ftell(_file));
			std::fseek(_file, 0, SEEK_END);
			int end(std::ftell(_file));
			std::fseek(_file, init, SEEK_SET);
			return end;
		}

		std::size_t OldPsyFile::GetPos()
		{
			return std::ftell(_file);
		}

		bool OldPsyFile::Error()
		{
			return !ferror(_file);
		}

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// MemoryFile

		bool MemoryFile::OpenMem(std::size_t blocksize/*=65536*/)
		{
			blocksize_=blocksize;
			filesize=0;
			filepos=0;
			blockpos=0;
			blockidx=0;
			memoryblocks_.push_back(new char[blocksize_]);
			return true;
		}
		bool MemoryFile::Close() {
			std::vector<void*>::iterator ite;
			for (ite=memoryblocks_.begin();ite!=memoryblocks_.end();++ite) 
			{
				delete *ite;
			}
			memoryblocks_.clear();
			filesize=0;
			filepos=0;
			blockpos=0;
			blockidx=0;
			return true;
		}
		int MemoryFile::Seek(std::size_t bytes)
		{
			if (bytes >= filesize) {
				blockidx = memoryblocks_.size()-1;
				blockpos = filesize - blocksize_*(blockidx);
			}
			else {
				std::size_t targetblock = bytes/blocksize_;
				blockidx = targetblock;
				blockpos = bytes - blocksize_*(blockidx);
			}
			//TODO: is this what is returned by seek?
			filepos = blockpos + blocksize_*(blockidx);
			return filepos;
		}
		int MemoryFile::Skip(std::size_t bytes)
		{
			return Seek(GetPos()+bytes);
		}
		bool MemoryFile::Expect(const void * pData, std::size_t numBytes)
		{
			char c;
			while(numBytes-- != 0)
			{
				if(!ReadInternal(&c,1)) return false;
				if(c != *((char*)pData)) return false;
				pData = (char*)pData + 1;
			}
			return true;
		}
		bool MemoryFile::ReadInternal(void * pData, std::size_t numBytes)
		{
			bool ok=true;
			char* outdata = static_cast<char*>(pData);
			if (filepos+numBytes >= filesize) {
				numBytes=filesize-filepos-1;
				ok=false;
			}
			while (numBytes > 0) {
				char* block = static_cast<char*>(memoryblocks_[blockidx]);
				while (numBytes > 0 && blockpos < blocksize_) {
					*outdata=block[blockpos];
					blockpos++;
					outdata++;
					filepos++;
					numBytes--;
				}
				if (blockpos >= blocksize_) {
					blockidx++;
					blockpos=0;
				}
			}
			return ok;
		}
		bool MemoryFile::WriteInternal(void const * pData, std::size_t numBytes)
		{
			char const* indata = static_cast<char const*>(pData);
			while (numBytes > 0) {
				char* block = static_cast<char*>(memoryblocks_[blockidx]);
				while (numBytes > 0 && blockpos < blocksize_) {
					block[blockpos]=*indata;
					blockpos++;
					indata++;
					filepos++;
					numBytes--;
				}
				if (blockpos >= blocksize_) {
					blockidx++;
					blockpos=0;
					if(blockidx >= memoryblocks_.size() ) {
						memoryblocks_.push_back(new char[blocksize_]);
					}
				}
			}
			if (filepos >= filesize) { filesize=filepos; }

			return true;
		}

	}
}
