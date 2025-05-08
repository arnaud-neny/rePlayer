///\file
///\interface psycle::host::Configuration.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
namespace psycle
{
	namespace host
	{
		/*abstract*/ class ConfigStorage
		{
		public:
			ConfigStorage(std::string version) {};
			virtual ~ConfigStorage() {};

			virtual std::string GetVersion() = 0;

			virtual bool CreateLocation(std::string const & location, bool overwrite=true) = 0;
			virtual bool OpenLocation(std::string const & location, bool create_if_missing=false) = 0;
			virtual void CloseLocation() = 0;
			virtual bool DeleteLocation(std::string const & location) = 0;
			virtual bool CreateGroup(std::string const & group, bool overwrite=true) = 0;
			virtual bool OpenGroup(std::string const & group, bool create_if_missing=false) = 0;
			virtual bool DeleteGroup(std::string const & key, bool fail_if_not_empty=false) = 0;
			virtual bool CloseGroup() = 0;
			virtual bool DeleteKey(std::string const & key) = 0;
			virtual std::list<std::string> GetGroups() = 0;
			virtual std::list<std::string> GetKeys() = 0;

		///\name 1 bit
		///\{
			virtual bool Read(std::string const & key, bool & x) = 0;
			virtual bool Write(std::string const & key, bool x) = 0;
		///\}

		///\name 8 bits
		///\{
			virtual bool Read(std::string const & key, uint8_t & x) = 0;
			virtual bool Read(std::string const & key, int8_t & x) = 0;
			virtual bool Read(std::string const & key, char & x) = 0; // 'char' is equivalent to either 'signed char' or 'unsigned char', but considered as a different type

			virtual bool Write(std::string const & key, uint8_t x) = 0;
			virtual bool Write(std::string const & key, int8_t x) = 0;
			virtual bool Write(std::string const & key, char x) = 0; // 'char' is equivalent to either 'signed char' or 'unsigned char', but considered as a different type
		///\}

		///\name 16 bits
		///\{
			virtual bool Read(std::string const & key, uint16_t & x) = 0;
			virtual bool Read(std::string const & key, int16_t & x) = 0;

			virtual bool Write(std::string const & key, uint16_t x) = 0;
			virtual bool Write(std::string const & key, int16_t x) = 0;
		///\}

		///\name 32 bits
		///\{
			virtual bool Read(std::string const & key, uint32_t & x) = 0;
			virtual bool Read(std::string const & key, int32_t & x) = 0;
			virtual bool Read(std::string const & key, COLORREF & x) = 0;

			virtual bool Write(std::string const & key, uint32_t x) = 0;
			virtual bool Write(std::string const & key, int32_t x) = 0;
			virtual bool Write(std::string const & key, COLORREF x) = 0;
		///\}

		///\name 64 bits
		///\{
			virtual bool Read(std::string const & key, uint64_t & x) = 0;
			virtual bool Read(std::string const & key, int64_t & x) = 0;

			virtual bool Write(std::string const & key, uint64_t x) = 0;
			virtual bool Write(std::string const & key, int64_t x) = 0;
		///\}

		///\name 32-bit floating point
		///\{
			virtual bool Read(std::string const & key, float & x) = 0;
			virtual bool Write(std::string const & key, float x) = 0;
		///\}

		///\name 64-bit floating point
		///\{
			virtual bool Read(std::string const & key, double & x) = 0;
			virtual bool Write(std::string const & key, double x) = 0;
		///\}

		///\name strings
		///\{
			virtual bool Read(std::string const & key, std::string &) = 0;
			virtual bool Read(std::string const & key, char *, std::size_t max_length) = 0;
			virtual bool Write(std::string const & key, std::string const &) = 0;
			virtual bool Read(std::string const & key, wchar_t *, std::size_t max_length) = 0;
			virtual bool Write(std::string const & key, wchar_t *) = 0;
		///\}
		///\raw data. Store decides how to save/load it.
		///\{
			virtual bool ReadRaw(std::string const & key, void *, std::size_t max_length) = 0;
			virtual bool WriteRaw(std::string const & key, void *, std::size_t bytesize) = 0;
		///\}
		};
	}
}
