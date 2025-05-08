///\file
///\interface psycle::host::Registry.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "ConfigStorage.hpp"
#include <string>
namespace psycle
{
	namespace host
	{
		typedef unsigned long int type;
		typedef long int result;

		class Registry : public ConfigStorage
		{
		public:
			typedef enum {
				HKLM = 0,
				HKCU
			} root_t;
#if !defined COLORREF
			typedef unsigned long COLORREF;
#endif

			Registry(root_t tree_root, std::string version);
			virtual ~Registry();

			/*override*/ std::string GetVersion() { return version_config_; }

			/*override*/ bool CreateLocation(std::string const & location, bool overwrite=true);
			/*override*/ bool OpenLocation(std::string const & location, bool create_if_missing=false);
			/*override*/ void CloseLocation();
			/*override*/ bool DeleteLocation(std::string const & location);
			/*override*/ bool CreateGroup(std::string const & group, bool overwrite=true);
			/*override*/ bool OpenGroup(std::string const & group, bool create_if_missing=false);
			/*override*/ bool DeleteGroup(std::string const & key, bool fail_if_not_empty=false);
			/*override*/ bool CloseGroup();
			/*override*/ bool DeleteKey(std::string const & key);
			/*override*/ std::list<std::string> GetGroups();
			/*override*/ std::list<std::string> GetKeys();

		///\name 1 bit
		///\{
			/*override*/ bool Read(std::string const & key, bool & x);
			/*override*/ bool Write(std::string const & key, bool x);
		///\}

		///\name 8 bits
		///\{
			/*override*/ bool Read(std::string const & key, uint8_t & x);
			/*override*/ bool Read(std::string const & key, int8_t & x);
			/*override*/ bool Read(std::string const & key, char & x); // 'char' is equivalent to either 'signed char' or 'unsigned char', but considered as a different type

			/*override*/ bool Write(std::string const & key, uint8_t x);
			/*override*/ bool Write(std::string const & key, int8_t x);
			/*override*/ bool Write(std::string const & key, char x); // 'char' is equivalent to either 'signed char' or 'unsigned char', but considered as a different type
		///\}

		///\name 16 bits
		///\{
			/*override*/ bool Read(std::string const & key, uint16_t & x);
			/*override*/ bool Read(std::string const & key, int16_t & x);

			/*override*/ bool Write(std::string const & key, uint16_t x);
			/*override*/ bool Write(std::string const & key, int16_t x);
		///\}

		///\name 32 bits
		///\{
			/*override*/ bool Read(std::string const & key, uint32_t & x);
			/*override*/ bool Read(std::string const & key, int32_t & x);
			/*override*/ bool Read(std::string const & key, COLORREF & x);

			/*override*/ bool Write(std::string const & key, uint32_t x);
			/*override*/ bool Write(std::string const & key, int32_t x);
			/*override*/ bool Write(std::string const & key, COLORREF x);
		///\}

		///\name 64 bits
		///\{
			/*override*/ bool Read(std::string const & key, uint64_t & x);
			/*override*/ bool Read(std::string const & key, int64_t & x);

			/*override*/ bool Write(std::string const & key, uint64_t x);
			/*override*/ bool Write(std::string const & key, int64_t x);
		///\}

		///\name 32-bit floating point
		///\{
			/*override*/ bool Read(std::string const & key, float & x);
			/*override*/ bool Write(std::string const & key, float x);
		///\}

		///\name 64-bit floating point
		///\{
			/*override*/ bool Read(std::string const & key, double & x);
			/*override*/ bool Write(std::string const & key, double x);
		///\}

		///\name strings
		///\{
			/*override*/ bool Read(std::string const & key, std::string &);
			/*override*/ bool Read(std::string const & key, char *, std::size_t max_length);
			/*override*/ bool Write(std::string const & key, std::string const &);
		///\}
		///\raw data. Store decides how to save/load it.
		///\{
			/*override*/ bool ReadRaw(std::string const & key, void *, std::size_t max_length);
			/*override*/ bool WriteRaw(std::string const & key, void *, std::size_t bytesize);
			/*override*/ bool Read(std::string const & key, wchar_t *, std::size_t max_length_bytes);
			/*override*/ bool Write(std::string const & key, wchar_t *);
		///\}
			result QueryTypeAndSize(std::string const &, type & type, std::size_t & size);

			template<typename x>
			long int QueryValue(std::string const & name, x & data, type const & type_wanted = REG_BINARY);
			template<typename x>
			long int SetValue(std::string const & name, x const & data, type const & type = REG_BINARY);

		private:
			BOOL Registry::RegDelnodeRecurse (HKEY hKeyRoot, LPTSTR lpSubKey);

			::HKEY hk_root;
			::HKEY config_root;
			::HKEY current_group;
			std::string version_config_;
		};
	}
}
