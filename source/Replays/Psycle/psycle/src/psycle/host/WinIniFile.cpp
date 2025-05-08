///\file
///\implementation psycle::host::Registry.

#include <psycle/host/detail/project.private.hpp>
#include "WinIniFile.hpp"
#include <psycle/helpers/hexstring_to_binary.hpp>
#include <psycle/helpers/hexstring_to_integer.hpp>
namespace psycle
{
	namespace host
	{
		WinIniFile::WinIniFile(std::string version)
			: ConfigStorage(version)
		{
			version_config_ = version;
		}

		WinIniFile::~WinIniFile()
		{
			CloseLocation();
		}

		bool WinIniFile::CreateLocation(std::string const & location, bool overwrite)
		{
			filename = location;
			if (overwrite) { DeleteLocation(location); }
			bool done = WritePrivateProfileString("Version", "ConfigVersion", version_config_.c_str(), filename.c_str()); 
			current_group.clear();
			return done;
		}
		bool WinIniFile::OpenLocation(std::string const & location, bool create_if_missing)
		{
			char result[1024];
			filename = location;
			bool done = GetPrivateProfileString("Version", "ConfigVersion", NULL, result, 1024, filename.c_str());
			current_group.clear();
			if (done) version_config_ = result;
			return done;
		}
		void WinIniFile::CloseLocation()
		{
			current_group.clear();
			config_root.clear();
		}
		bool WinIniFile::DeleteLocation(std::string const & location)
		{
			if(!current_group.empty()) {
				CloseLocation();
			}
			if(!location.empty()) {
				return DeleteFile(location.c_str());
			}
			return false;
		}
		bool WinIniFile::CreateGroup(std::string const & group, bool overwrite)
		{
			current_group = group;
			if(overwrite) {
				return DeleteGroup(group);
			}
			return true;
		}
		bool WinIniFile::OpenGroup(std::string const & group, bool create_if_missing)
		{
			current_group = group;
			return true;
		}
		bool WinIniFile::CloseGroup()
		{
			current_group.clear();
			return true;
		}

		bool WinIniFile::DeleteGroup(std::string const & group, bool fail_if_not_empty)
		{
			bool done = WritePrivateProfileString(group.c_str(), NULL, NULL, filename.c_str());
			return done;
		}

		bool WinIniFile::DeleteKey(std::string const & key)
		{
			bool done = WritePrivateProfileString(current_group.c_str(), key.c_str(), NULL, filename.c_str());
			return done;
		}
		///\todo: finish
		 std::list<std::string> WinIniFile::GetGroups(){std::list<std::string> list; return list;}
		 /*
DWORD WINAPI GetPrivateProfileSectionNames(
  __out  LPTSTR lpszReturnBuffer,
  __in   DWORD nSize,
  __in   LPCTSTR lpFileName
);		 */
		 std::list<std::string> WinIniFile::GetKeys(){std::list<std::string> list; return list;}
		 /*
DWORD WINAPI GetPrivateProfileString(
  __in   LPCTSTR lpAppName,
  __in   LPCTSTR lpKeyName,
  __in   LPCTSTR lpDefault,
  __out  LPTSTR lpReturnedString,
  __in   DWORD nSize,
  __in   LPCTSTR lpFileName
);
lpKeyName [in]
If this parameter is NULL, all key names in the section specified by the lpAppName parameter are copied to the buffer specified by the lpReturnedString parameter.

*/
		 bool WinIniFile::Read(std::string const & key, bool & x){ 
			 UINT y = GetPrivateProfileInt(current_group.c_str(), key.c_str(), 0x87654321, filename.c_str());
			 if (y != 0x87654321) x = (y==1);
			 return y!=0x87654321;
		 }
		 bool WinIniFile::Write(std::string const & key, bool x){ uint32_t y = x?1:0; return Write(key, y); }

		 bool WinIniFile::Read(std::string const & key, uint8_t & x){
			 UINT y = GetPrivateProfileInt(current_group.c_str(), key.c_str(), 0x87654321, filename.c_str());
			 if (y != 0x87654321) x = y;
			 return y!=0x87654321;
		 }
		 bool WinIniFile::Read(std::string const & key, int8_t & x){
			 UINT y = GetPrivateProfileInt(current_group.c_str(), key.c_str(), 0x87654321, filename.c_str());
			 if (y != 0x87654321) x = y;
			 return y!=0x87654321;
		 }
		 // 'char' is equivalent to either 'signed char' or 'unsigned char', but considered as a different type
		 bool WinIniFile::Read(std::string const & key, char & x){
			 UINT y = GetPrivateProfileInt(current_group.c_str(), key.c_str(), 0x87654321, filename.c_str());
			 if (y != 0x87654321) x = y;
			 return y!=0x87654321;
		 }
		 bool WinIniFile::Write(std::string const & key, uint8_t x){ uint32_t y = x; return Write(key, y); }
		 bool WinIniFile::Write(std::string const & key, int8_t x){ int32_t y = x; return Write(key, y); }
		 bool WinIniFile::Write(std::string const & key, char x){ int32_t y = x; return Write(key, y); }

		 bool WinIniFile::Read(std::string const & key, uint16_t & x){
			 UINT y = GetPrivateProfileInt(current_group.c_str(), key.c_str(), 0x87654321, filename.c_str());
			 if (y != 0x87654321) x = y;
			 return y!=0x87654321;
		 }
		 bool WinIniFile::Read(std::string const & key, int16_t & x){
			 UINT y = GetPrivateProfileInt(current_group.c_str(), key.c_str(), 0x87654321, filename.c_str());
			 if (y != 0x87654321) x = y;
			 return y!=0x87654321;
		 }
		 bool WinIniFile::Write(std::string const & key, uint16_t x){ uint32_t y = x; return Write(key, y); }
		 bool WinIniFile::Write(std::string const & key, int16_t x){ int32_t y = x; return Write(key, y); }

		 // The default number is a valid number in this case, but expected to be unusual enough.
		 bool WinIniFile::Read(std::string const & key, uint32_t & x){
			 UINT y = GetPrivateProfileInt(current_group.c_str(), key.c_str(), 0x87654321, filename.c_str());
			 if (y != 0x87654321) x = y;
			 return y!=0x87654321;
		 }
		// The default number is a valid number in this case, but expected to be unusual enough.
		 bool WinIniFile::Read(std::string const & key, int32_t & x){
			 UINT y = GetPrivateProfileInt(current_group.c_str(), key.c_str(), 0x87654321, filename.c_str());
			 if (y != 0x87654321) x = y;
			 return y!=0x87654321;
		 }
		 bool WinIniFile::Read(std::string const & key, COLORREF & x){
			char result[32];
			bool done = GetPrivateProfileString(current_group.c_str(), key.c_str(), NULL, result, 32, filename.c_str());
			if (done) x = static_cast<COLORREF>(hexstring_to_integer(result));
			return done;
		 }
		 bool WinIniFile::Write(std::string const & key, uint32_t x){
			 char result[32];
			 std::sprintf(result, "%u",x);
			 bool done = WritePrivateProfileString(current_group.c_str(), key.c_str(), result, filename.c_str());
			 return done;
		 }
		 bool WinIniFile::Write(std::string const & key, int32_t x){
			 char result[32];
			 std::sprintf(result, "%d",x);
			 bool done = WritePrivateProfileString(current_group.c_str(), key.c_str(), result, filename.c_str());
			 return done;
		 }
		 bool WinIniFile::Write(std::string const & key, COLORREF x){
			 std::ostringstream out;
			 out << std::hex << static_cast<uint32_t>(x);
			 bool done = WritePrivateProfileString(current_group.c_str(), key.c_str(), out.str().c_str(), filename.c_str());
			 return done;
		 }

		 bool WinIniFile::Read(std::string const & key, uint64_t & x){
			char result[32];
			bool done = GetPrivateProfileString(current_group.c_str(), key.c_str(), NULL, result, 32, filename.c_str());
			if (done) {
				std::istringstream in(result);
				in >> x;
			}
			return done;
		 }
		 bool WinIniFile::Read(std::string const & key, int64_t & x){
			char result[32];
			bool done = GetPrivateProfileString(current_group.c_str(), key.c_str(), NULL, result, 32, filename.c_str());
			if(done) {
				std::istringstream in(result);
				in >> x;
			}
			return done;
		 }
		 bool WinIniFile::Write(std::string const & key, uint64_t x) {
			 char result[32];
			 std::sprintf(result, "%I64u", x);
			 bool done = WritePrivateProfileString(current_group.c_str(), key.c_str(), result, filename.c_str());
			 return done;
		 }
		 bool WinIniFile::Write(std::string const & key, int64_t x){
			 char result[32];
			 std::sprintf(result, "%I64d", x);
			 bool done = WritePrivateProfileString(current_group.c_str(), key.c_str(), result, filename.c_str());
			 return done;
		 }

		 bool WinIniFile::Read(std::string const & key, float & x){
			char result[32];
			bool done = GetPrivateProfileString(current_group.c_str(), key.c_str(), NULL, result, 32, filename.c_str());
			if(done) x = atof(result);
			return done;
		 }
		 bool WinIniFile::Write(std::string const & key, float x){
			 char result[32];
			 std::sprintf(result, "%f", x);
			 bool done = WritePrivateProfileString(current_group.c_str(), key.c_str(), result, filename.c_str());
			 return done;
		 }

		 bool WinIniFile::Read(std::string const & key, double & x){
			char result[32];
			bool done = GetPrivateProfileString(current_group.c_str(), key.c_str(), NULL, result, 32, filename.c_str());
			if(done) x = atof(result);
			return done;
		 }
		 bool WinIniFile::Write(std::string const & key, double x){
 			 char result[32];
			 std::sprintf(result, "%f", x);
			 bool done = WritePrivateProfileString(current_group.c_str(), key.c_str(), result, filename.c_str());
			 return done;
		}
		 bool WinIniFile::Read(std::string const & key, std::string &s) { 
			 char result[4096];
			 bool done = GetPrivateProfileString(current_group.c_str(), key.c_str(), NULL, result, 4096, filename.c_str());
			 if (done) s = result;
			 return done;
		 }
		 bool WinIniFile::Read(std::string const & key, char *buffer, std::size_t max_length) {
			char result[4096];
			bool done = GetPrivateProfileString(current_group.c_str(), key.c_str(), NULL, result, 4096, filename.c_str());
			if (done) strncpy(buffer,result, max_length);
			return done;
		 }
		 bool WinIniFile::Write(std::string const & key, std::string const &s)
		 {
			 bool done = WritePrivateProfileString(current_group.c_str(), key.c_str(), s.c_str(), filename.c_str());
			 return done;
		 }

		bool WinIniFile::Read(std::string const & key, wchar_t *string, std::size_t max_length_bytes)
		{
			char result[8192];
			bool done = GetPrivateProfileString(current_group.c_str(), key.c_str(), NULL, result, 8192, filename.c_str());
			if(done) {
				done = MultiByteToWideChar(CP_UTF8,NULL,result, -1, string, max_length_bytes);
			}
			 return done;
		}
		bool WinIniFile::Write(std::string const & key, wchar_t *string)
		{
			int wlen = wcslen(string);
			char* res = new char[wlen*2+1];
			WideCharToMultiByte(CP_UTF8,NULL,string, -1, res, wlen*2+1, NULL, NULL);
			 bool done = WritePrivateProfileString(current_group.c_str(), key.c_str(), res, filename.c_str());
			 delete[] res;
			 return done;
		}


		bool WinIniFile::ReadRaw(std::string const & key, void *data, std::size_t max_length)
		{
			char result[65535];
			bool done = GetPrivateProfileString(current_group.c_str(), key.c_str(), NULL, result, 65535, filename.c_str());
			if (done) hexstring_to_binary(result, data, max_length);
			return done;
		}
		bool WinIniFile::WriteRaw(std::string const & key, void *data, std::size_t bytesize)
		{
			std::string pre = binary_to_hexstring(data, bytesize);
			bool done = WritePrivateProfileString(current_group.c_str(), key.c_str(), pre.c_str(), filename.c_str());
			return done;
		}
	}
}
