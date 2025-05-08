///\file
///\implementation psycle::host::Registry.

#include <psycle/host/detail/project.private.hpp>
#include "Registry.hpp"
namespace psycle
{
	namespace host
	{
		Registry::Registry(root_t tree_root, std::string version)
			: ConfigStorage(version), config_root(), current_group()
		{
			if (tree_root == HKCU) {
				hk_root = HKEY_CURRENT_USER;
			} else if (tree_root == HKLM) {
				hk_root = HKEY_LOCAL_MACHINE;
			}
			version_config_ = version;
		}

		Registry::~Registry()
		{
			CloseLocation();
		}

		bool Registry::CreateLocation(std::string const & location, bool overwrite)
		{
			//Creates the specified registry key. If the key already exists, the function opens it.
			result result = ::RegCreateKeyEx(hk_root, location.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &config_root, 0);
			if (result == ERROR_SUCCESS)
			{
				if(overwrite)
				{
					///\todo.
				}
				current_group = config_root;
				Write("ConfigVersion",version_config_);
				current_group = NULL;
				return true;
			}
			else return false;
		}
		bool Registry::OpenLocation(std::string const & location, bool create_if_missing)
		{
			result result = ::RegOpenKeyEx(hk_root, location.c_str(), 0, KEY_READ, &config_root);
			if(result == ERROR_FILE_NOT_FOUND && create_if_missing) 
			{
				result = ::RegCreateKeyEx(hk_root, location.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &config_root, 0);
				::RegCloseKey(config_root);
				config_root = NULL;
				version_config_ = "";
				result = ::RegOpenKeyEx(hk_root, location.c_str(), 0, KEY_READ, &config_root);
			}
			if(result == ERROR_SUCCESS) 
			{
				current_group = config_root;
				Read("ConfigVersion",version_config_);
				current_group = NULL;
			}
			return (result == ERROR_SUCCESS);
		}
		void Registry::CloseLocation()
		{
			::RegCloseKey(current_group);
			current_group = NULL;
			::RegCloseKey(config_root);
			config_root = NULL;
		}
		bool Registry::DeleteLocation(std::string const & location)
		{
			if(config_root != NULL) {
				CloseLocation();
			}
			TCHAR szDelKey[MAX_PATH*2];
			strncpy(szDelKey,location.c_str(),MAX_PATH*2);
			return RegDelnodeRecurse(hk_root, szDelKey);
		}
		//*************************************************************
		//  http://msdn.microsoft.com/en-us/library/windows/desktop/ms724235%28v=vs.85%29.aspx
		//  RegDelnodeRecurse()
		//
		//  Purpose:    Deletes a registry key and all its subkeys / values.
		//
		//  Parameters: hKeyRoot    -   Root key
		//              lpSubKey    -   SubKey to delete
		//
		//  Return:     TRUE if successful.
		//              FALSE if an error occurs.
		//
		//*************************************************************
		BOOL Registry::RegDelnodeRecurse (HKEY hKeyRoot, LPTSTR lpSubKey)
		{
			LPTSTR lpEnd;
			LONG lResult;
			DWORD dwSize;
			TCHAR szName[MAX_PATH];
			HKEY hKey;
			FILETIME ftWrite;

			// First, see if we can delete the key without having
			// to recurse.

			lResult = RegDeleteKey(hKeyRoot, lpSubKey);
			if (lResult == ERROR_SUCCESS) {
				return TRUE;
			}

			if ( RegOpenKeyEx (hKeyRoot, lpSubKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
				return lResult;
			}

			// Check for an ending slash and add one if it is missing.
			lpEnd = lpSubKey + lstrlen(lpSubKey);
			if (*(lpEnd - 1) != TEXT('\\')) 
			{
				*lpEnd =  TEXT('\\');
				lpEnd++;
				*lpEnd =  TEXT('\0');
			}

			// Enumerate the keys

			dwSize = MAX_PATH;
			lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL, NULL, NULL, &ftWrite);

			if (lResult == ERROR_SUCCESS) 
			{
				do {

					strncpy(lpEnd, szName, MAX_PATH);

					if (!RegDelnodeRecurse(hKeyRoot, lpSubKey)) {
						break;
					}

					dwSize = MAX_PATH;

					lResult = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL,
										   NULL, NULL, &ftWrite);

				} while (lResult == ERROR_SUCCESS);
			}

			lpEnd--;
			*lpEnd = TEXT('\0');

			RegCloseKey (hKey);

			// Try again to delete the key.
			lResult = RegDeleteKey(hKeyRoot, lpSubKey);
			if (lResult == ERROR_SUCCESS) 
				return TRUE;

			return FALSE;
		}

		bool Registry::CreateGroup(std::string const & group, bool overwrite)
		{
			::RegCloseKey(current_group);
			current_group = NULL;
			result result = ::RegCreateKeyEx(config_root, group.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &current_group, 0);
			if (result == ERROR_SUCCESS)
			{
				if(overwrite)
				{
					///\todo.
				}
				return true;
			}
			else return false;
		}
		bool Registry::OpenGroup(std::string const & group, bool create_if_missing)
		{
			::RegCloseKey(current_group);
			current_group = NULL;
			result result = ::RegOpenKeyEx(config_root, group.c_str(), 0, KEY_READ, &current_group);
			if(result == ERROR_FILE_NOT_FOUND && create_if_missing)
			{
				result = ::RegCreateKeyEx(config_root, group.c_str(), 0, 0, 0, KEY_ALL_ACCESS, 0, &current_group, 0);
				::RegCloseKey(current_group);
				current_group = NULL;
				result = ::RegOpenKeyEx(config_root, group.c_str(), 0, KEY_READ, &current_group);
			}
			return (result == ERROR_SUCCESS);
		}
		bool Registry::CloseGroup()
		{
			result result = ::RegCloseKey(current_group);
			current_group = NULL;
			return (result == ERROR_SUCCESS);
		}

		bool Registry::DeleteGroup(std::string const & key, bool fail_if_not_empty)
		{
			return (::RegDeleteKey(config_root, key.c_str()) == ERROR_SUCCESS);
		}

		bool Registry::DeleteKey(std::string const & key)
		{
			return (::RegDeleteValue(current_group, key.c_str()) == ERROR_SUCCESS);
		}
		///\todo: finish
		 std::list<std::string> Registry::GetGroups(){std::list<std::string> list; return list;}
		 std::list<std::string> Registry::GetKeys(){std::list<std::string> list; return list;}
		 bool Registry::Read(std::string const & key, bool & x){ 
			 bool x2;
			 bool result = ERROR_SUCCESS==QueryValue(key, x2, REG_BINARY);
			 if (result) x = x2;
			 return result;
		 }
		 bool Registry::Write(std::string const & key, bool x){return ERROR_SUCCESS==SetValue(key, x, REG_BINARY);}

		 bool Registry::Read(std::string const & key, uint8_t & x){
			 uint32_t x2;
			 bool result = ERROR_SUCCESS==QueryValue(key, x2, REG_DWORD);
			 if (result) x = x2;
			 return result;
		 }
		 bool Registry::Read(std::string const & key, int8_t & x){
			 uint32_t x2;
			 bool result =  ERROR_SUCCESS==QueryValue(key, x2, REG_DWORD); 
			 if (result) x = x2;
			 return result; 
		 }
		 // 'char' is equivalent to either 'signed char' or 'unsigned char', but considered as a different type
		 bool Registry::Read(std::string const & key, char & x){
			 uint32_t x2;
			 bool result =  ERROR_SUCCESS==QueryValue(key, x2, REG_DWORD);
			 if (result) x = x2;
			 return result;
		 }
		 bool Registry::Write(std::string const & key, uint8_t x){uint32_t x2 = x; return ERROR_SUCCESS==SetValue(key, x2, REG_DWORD);}
		 bool Registry::Write(std::string const & key, int8_t x){uint32_t x2 = x; return ERROR_SUCCESS==SetValue(key, x2, REG_DWORD);}
		 bool Registry::Write(std::string const & key, char x){uint32_t x2 = x; return ERROR_SUCCESS==SetValue(key, x2, REG_DWORD);} // 'char' is equivalent to either 'signed char' or 'unsigned char', but considered as a different type

		 bool Registry::Read(std::string const & key, uint16_t & x){
			 uint32_t x2;
			 bool result =  ERROR_SUCCESS==QueryValue(key, x2, REG_DWORD);
			 if (result) x = x2;
			 return result; 
		 }
		 bool Registry::Read(std::string const & key, int16_t & x){
			 uint32_t x2;
			 bool result =  ERROR_SUCCESS==QueryValue(key, x2, REG_DWORD);
			 if (result) x = x2;
			 return result;
		 }
		 bool Registry::Write(std::string const & key, uint16_t x){uint32_t x2 = x; return ERROR_SUCCESS==SetValue(key, x2, REG_DWORD);}
		 bool Registry::Write(std::string const & key, int16_t x){uint32_t x2 = x; return ERROR_SUCCESS==SetValue(key, x2, REG_DWORD);}

		 bool Registry::Read(std::string const & key, uint32_t & x){
			 uint32_t x2;
			 bool result =  ERROR_SUCCESS==QueryValue(key, x2, REG_DWORD);
			 if (result) x = x2;
			 return result;
		 }
		 bool Registry::Read(std::string const & key, int32_t & x){
			 int32_t x2;
			 bool result =  ERROR_SUCCESS==QueryValue(key, x2, REG_DWORD);
			 if (result) x = x2;
			 return result;
		 }
		 bool Registry::Read(std::string const & key, COLORREF & x){
			 COLORREF x2;
			 bool result =  ERROR_SUCCESS==QueryValue(key, x2, REG_DWORD);
			 if (result) x = x2;
			 return result;
		 }
		 bool Registry::Write(std::string const & key, uint32_t x){return ERROR_SUCCESS==SetValue(key, x, REG_DWORD);}
		 bool Registry::Write(std::string const & key, int32_t x){return ERROR_SUCCESS==SetValue(key, x, REG_DWORD);}
		 bool Registry::Write(std::string const & key, COLORREF x){return ERROR_SUCCESS==SetValue(key, x, REG_DWORD);}

		 bool Registry::Read(std::string const & key, uint64_t & x){
			 uint64_t x2;
			 bool result =  ERROR_SUCCESS==QueryValue(key, x2, REG_QWORD);
			 if (result) x = x2;
			 return result;
		 }
		 bool Registry::Read(std::string const & key, int64_t & x){
			 int64_t x2;
			 bool result =  ERROR_SUCCESS==QueryValue(key, x2, REG_QWORD);
			 if (result) x = x2;
			 return result;
		 }
		 bool Registry::Write(std::string const & key, uint64_t x){return ERROR_SUCCESS==SetValue(key, x, REG_QWORD);}
		 bool Registry::Write(std::string const & key, int64_t x){return ERROR_SUCCESS==SetValue(key, x, REG_QWORD);}

		 bool Registry::Read(std::string const & key, float & x){
			 float x2;
			 bool result =  ERROR_SUCCESS==QueryValue(key, x2, REG_DWORD);
			 if (result) x = x2;
			 return result;
		 }
		 bool Registry::Write(std::string const & key, float x){return ERROR_SUCCESS==SetValue(key, x, REG_DWORD);}

		 bool Registry::Read(std::string const & key, double & x){
			 float x2;
			 bool result =  ERROR_SUCCESS==QueryValue(key, x2, REG_QWORD);
			 if (result) x = x2;
			 return result;
		 }
		 bool Registry::Write(std::string const & key, double x){return ERROR_SUCCESS==SetValue(key, x, REG_QWORD);}
		 bool Registry::Read(std::string const & key, std::string &s)
		 {
			std::size_t size;
			type type;
			// get length of string
			result error(QueryTypeAndSize(key, type, size));
			if(error != ERROR_SUCCESS) return false;
			if(type != REG_SZ) return false;
			// allocate a buffer
			char * buffer(new char[size]);
			error = ::RegQueryValueEx(current_group, key.c_str(), 0, &type, reinterpret_cast<unsigned char *>(buffer), reinterpret_cast<unsigned long int*>(&size));
			if(error == ERROR_SUCCESS) s = buffer;
			delete[] buffer;
			return (error == ERROR_SUCCESS);
		 }
		 bool Registry::Read(std::string const & key, char *buffer, std::size_t max_length)
		 {
			std::size_t size;
			type type;
			// get length of string
			result error(QueryTypeAndSize(key, type, size));
			if(error != ERROR_SUCCESS) return false;
			if(type != REG_SZ) return false;
			if(size > max_length) return false;
			error = ::RegQueryValueEx(current_group, key.c_str(), 0, &type, reinterpret_cast<unsigned char *>(buffer), reinterpret_cast<unsigned long int*>(&size));
			return (error == ERROR_SUCCESS);
		 }
		 bool Registry::Write(std::string const & key, std::string const &s)
		 { 
			 int error = ::RegSetValueEx(current_group, key.c_str(), 0, REG_SZ, reinterpret_cast<unsigned char const *>(s.c_str()), (DWORD)(s.length() + 1));
			 return (error == ERROR_SUCCESS);
		 }

		bool Registry::Read(std::string const & key, wchar_t *string, std::size_t max_length_bytes)
		{
			char result[8192];
			bool done = Read(key,result,8191);
			if(done) {
				done = MultiByteToWideChar(CP_UTF8,NULL,result, -1, string, max_length_bytes);
			}
			 return done;
		}
		bool Registry::Write(std::string const & key, wchar_t *string)
		{
			int wlen = wcslen(string);
			char* res = new char[wlen*2+1];
			int bytes = WideCharToMultiByte(CP_UTF8,NULL,string, -1, res, wlen*2+1, NULL, NULL);
			bool done = bytes>0;
			if (done) {
				done = ::RegSetValueEx(current_group, key.c_str(), 0, REG_SZ, reinterpret_cast<unsigned char const *>(res), bytes);
			}
			 delete[] res;
			 return done;
		}

		bool Registry::ReadRaw(std::string const & key, void *data, std::size_t max_length)
		{
			type type_read = REG_BINARY;
			result const error(::RegQueryValueEx(current_group, key.c_str(), 0, &type_read, reinterpret_cast<unsigned char *>(data), reinterpret_cast<unsigned long int*>(&max_length)));
			return (error == ERROR_SUCCESS);
		}
		bool Registry::WriteRaw(std::string const & key, void *data, std::size_t bytesize)
		{
			result const error(::RegSetValueEx(current_group, key.c_str(), 0, REG_BINARY , reinterpret_cast<unsigned char const *>(data), bytesize));
			return (error == ERROR_SUCCESS);
		}

		long int
		Registry::QueryTypeAndSize(std::string const & name, type & type, std::size_t & size)
		{
			BOOST_STATIC_ASSERT((sizeof(long int) == sizeof(int))); // microsoft likes using unsigned long int, aka their DWORD typedef, instead of std::size_t
			type = REG_NONE;
			size = 0;
			return ::RegQueryValueEx(current_group, name.c_str(), 0, &type, 0, reinterpret_cast<unsigned long int*>(&size));
		}


		template<typename x>
		long int Registry::QueryValue(std::string const & name, x & data, type const & type_wanted)
		{
			BOOST_STATIC_ASSERT((sizeof(long int) == sizeof(int))); // microsoft likes using unsigned long int, aka their DWORD typedef, instead of std::size_t
			std::size_t size(sizeof data);
			type type_read;
			result const error(::RegQueryValueEx(current_group, name.c_str(), 0, &type_read, reinterpret_cast<unsigned char *>(&data), reinterpret_cast<unsigned long int*>(&size)));
			return error;
		}

		template<typename x>
		long int Registry::SetValue(std::string const & name, x const & data, type const & type)
		{
			result const error(::RegSetValueEx(current_group, name.c_str(), 0, type , reinterpret_cast<unsigned char const *>(&data), sizeof data));
			if(error != ERROR_SUCCESS) loggers::warning()("could not write " + name + "to registry.");
			return error;
		}

	}
}
