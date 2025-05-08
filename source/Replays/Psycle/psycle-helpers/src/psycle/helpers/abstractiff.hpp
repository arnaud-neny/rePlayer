/**
	\file
	interface file for psycle::helpers::abstractIff psycle::helpers::BaseChunkHeader, and the different big-endian and little-endian types.
*/
#pragma once

#include "endiantypes.hpp"
#include "sampleconverter.hpp"
#include <universalis.hpp>
#include <string>
#include <fstream>

namespace psycle { namespace helpers {

using namespace universalis::stdlib;

	/******* Data Structures *******/
	typedef char IffChunkId[4];

	/*abstract*/ class BaseChunkHeader
	{
	public:
		BaseChunkHeader() {}
		BaseChunkHeader(const IffChunkId& newid) {
			copyId(newid,id);
		}
		virtual ~BaseChunkHeader(){}

		static void copyId(const IffChunkId& idOrigin, IffChunkId& idDest);
		std::string idString() const;
		bool matches(const IffChunkId& id2) const;
		virtual void setLength(uint32_t newlength) = 0;
		virtual uint32_t length() const = 0;

	public:
		IffChunkId id;
	};


	/******* Base Class for reader ******/
	class AbstractIff
	{
	public:

		AbstractIff();
		virtual ~AbstractIff();

		virtual bool isValidFile() const = 0;

		virtual void Open(const std::string& fname);
		virtual void Create(const std::string& fname, const bool overwrite);
		virtual void Close();
		virtual bool Eof() const;
		std::string const inline & file_name() const throw();
		std::streamsize fileSize();

		virtual void addNewChunk(const BaseChunkHeader& header) = 0;
		virtual const BaseChunkHeader& readHeader() = 0;
		virtual const BaseChunkHeader& findChunk(const IffChunkId& id, bool allowWrap=false) = 0;
		virtual void skipThisChunk() = 0;
		virtual void UpdateCurrentChunkSize() = 0;
	
		void ReadString(std::string &);
		void ReadString(char *, std::size_t const max_length);
		void ReadSizedString(char *, std::size_t const read_length);
		void ReadPString(std::string &);

		inline void ReadArray(uint8_t* thearray, std::size_t n);
		inline void ReadArray(int8_t* thearray, std::size_t n);
		inline void ReadArray(char* thearray, std::size_t n);
		virtual void ReadArray(uint16_t* thearray, std::size_t n) = 0;
		virtual void ReadArray(int16_t* thearray, std::size_t n) = 0;
		virtual void ReadArray(uint32_t* thearray, std::size_t n) = 0;
		virtual void ReadArray(int32_t* thearray, std::size_t n) = 0;
		virtual void ReadArray(uint64_t* thearray, std::size_t n) = 0;
		virtual void ReadArray(int64_t* thearray, std::size_t n) = 0;
		virtual void ReadArray(float* thearray, std::size_t n) = 0;
		virtual void ReadArray(double* thearray, std::size_t n) = 0;
		inline void ReadArray(Long64BE*  thearray, std::size_t n);
		inline void ReadArray(Long64LE* thearray, std::size_t n);
		inline void ReadArray(LongBE* thearray, std::size_t n);
		inline void ReadArray(LongLE* thearray, std::size_t n);
		inline void ReadArray(Long24BE* thearray, std::size_t n);
		inline void ReadArray(Long24LE* thearray, std::size_t n);
		inline void ReadArray(ShortBE* thearray, std::size_t n);
		inline void ReadArray(ShortLE* thearray, std::size_t n);
		inline void ReadArray(FloatBE* thearray, std::size_t n);
		inline void ReadArray(FloatLE* thearray, std::size_t n);
		inline void ReadArray(DoubleBE* thearray, std::size_t n);
		inline void ReadArray(DoubleLE* thearray, std::size_t n);
		///Fallback for other types
		template<typename T>
		void ReadArray(T* thearray, std::size_t n);

		inline void Read(uint8_t & x);
		inline void Read(int8_t & x);
		virtual void Read(uint16_t & x)=0;
		virtual void Read(int16_t & x)=0;
		virtual void Read(uint32_t & x)=0;
		virtual void Read(int32_t & x)=0;
		virtual void Read(uint64_t & x)=0;
		virtual void Read(int64_t & x)=0;
		virtual void Read(float & x)=0;
		virtual void Read(double & x)=0;
		inline void Read(char & x);
		inline void Read(bool & x);
		inline void Read(IffChunkId & id);
		inline void Read(Long64BE & val);
		inline void Read(Long64LE & val);
		inline void Read(LongBE & val);
		inline void Read(LongLE & val);
		inline void Read(Long24BE & val);
		inline void Read(Long24LE & val);
		inline void Read(ShortBE & val);
		inline void Read(ShortLE & val);
		inline void Read(FloatBE & val);
		inline void Read(FloatLE & val);
		inline void Read(DoubleBE & val);
		inline void Read(DoubleLE & val);

		void WriteString(const std::string &string);
		void WriteString(const char * const data);
		void WriteSizedString(const char * const data, std::size_t const length);

		inline void WriteArray(uint8_t const* thearray, std::size_t n);
		inline void WriteArray(int8_t const* thearray, std::size_t n);
		inline void WriteArray(char const* thearray, std::size_t n);
		virtual void WriteArray(uint16_t const* thearray, std::size_t n) = 0;
		virtual void WriteArray(int16_t const* thearray, std::size_t n) = 0;
		virtual void WriteArray(uint32_t const* thearray, std::size_t n) = 0;
		virtual void WriteArray(int32_t const* thearray, std::size_t n) = 0;
		virtual void WriteArray(uint64_t const* thearray, std::size_t n) = 0;
		virtual void WriteArray(int64_t const* thearray, std::size_t n) = 0;
		virtual void WriteArray(float const* thearray, std::size_t n) = 0;
		virtual void WriteArray(double const* thearray, std::size_t n) = 0;
		inline void WriteArray(Long64BE const* thearray, std::size_t n);
		inline void WriteArray(Long64LE const* thearray, std::size_t n);
		inline void WriteArray(LongBE const* thearray, std::size_t n);
		inline void WriteArray(LongLE const* thearray, std::size_t n);
		inline void WriteArray(Long24BE const* thearray, std::size_t n);
		inline void WriteArray(Long24LE const* thearray, std::size_t n);
		inline void WriteArray(ShortBE const* thearray, std::size_t n);
		inline void WriteArray(ShortLE const* thearray, std::size_t n);
		inline void WriteArray(FloatBE const* thearray, std::size_t n);
		inline void WriteArray(FloatLE const* thearray, std::size_t n);
		inline void WriteArray(DoubleBE const* thearray, std::size_t n);
		inline void WriteArray(DoubleLE const* thearray, std::size_t n);
		///Fallback for other types
		template<typename T>
		void WriteArray(T const* thearray, std::size_t n);

		inline void Write(uint8_t x);
		inline void Write(int8_t x);
		virtual void Write(uint16_t x)=0;
		virtual void Write(int16_t x)=0;
		virtual void Write(uint32_t x)=0;
		virtual void Write(int32_t x)=0;
		virtual void Write(uint64_t x)=0;
		virtual void Write(int64_t x)=0;
		virtual void Write(float x)=0;
		virtual void Write(double x)=0;
		inline void Write(const char x);
		inline void Write(const bool x);
		inline void Write(const IffChunkId& id);
		inline void Write(const Long64BE& val);
		inline void Write(const Long64LE& val);
		inline void Write(const LongBE& val);
		inline void Write(const LongLE& val);
		inline void Write(const Long24BE& val);
		inline void Write(const Long24LE& val);
		inline void Write(const ShortBE& val);
		inline void Write(const ShortLE& val);
		inline void Write(const FloatBE& val);
		inline void Write(const FloatLE& val);
		inline void Write(const DoubleBE& val);
		inline void Write(const DoubleLE& val);
	protected:
		inline bool isWriteMode() const {return write_mode;}
		std::streampos GetPos(void);
		inline std::streamsize GuessedFilesize() const { return filesize; }
		std::streampos Seek(std::streampos const bytes);
		std::streampos Skip(std::streampos const bytes);

		inline void ReadBE(uint64_t & x);
		inline void ReadBE(int64_t & x);
		inline void ReadBE(uint32_t & x);
		inline void ReadBE(int32_t & x);
		inline void ReadBE(uint16_t & x);
		inline void ReadBE(int16_t & x);
		inline void ReadBE(float & x);
		inline void ReadBE(double & x);
		inline void ReadLE(uint64_t & x);
		inline void ReadLE(int64_t & x);
		inline void ReadLE(uint32_t & x);
		inline void ReadLE(int32_t & x);
		inline void ReadLE(uint16_t & x);
		inline void ReadLE(int16_t & x);
		inline void ReadLE(float & x);
		inline void ReadLE(double & x);

		inline void WriteBE(uint64_t x);
		inline void WriteBE(int64_t x);
		inline void WriteBE(uint32_t x);
		inline void WriteBE(int32_t x);
		inline void WriteBE(uint16_t x);
		inline void WriteBE(int16_t x);
		inline void WriteBE(float x);
		inline void WriteBE(double x);
		inline void WriteLE(uint64_t x);
		inline void WriteLE(int64_t x);
		inline void WriteLE(uint32_t x);
		inline void WriteLE(int32_t x);
		inline void WriteLE(uint16_t x);
		inline void WriteLE(int16_t x);
		inline void WriteLE(float x);
		inline void WriteLE(double x);

		inline void ReadRaw (void * data, std::size_t const bytes);
		inline void WriteRaw(const void * const data, std::size_t const bytes);
		bool Expect(const IffChunkId& id);
		bool Expect(const void * const data, std::size_t const bytes);
		void UpdateFormSize(std::streamoff headerposition, uint32_t numBytes);

		//For audio reading and conversion
		template<typename sample_type>
		inline void readDeinterleaveSamples(void** pSamps, uint16_t chans, uint32_t samples);
		template<typename file_type, typename platform_type>
		inline void readDeinterleaveSamplesendian(void** pSamps, uint16_t chans, uint32_t samples);

		template<typename in_type, typename out_type, out_type (*converter_func)(in_type)>
		void ReadWithintegerconverter(out_type* out, uint32_t samples);
		//Same as above, for multichan
		template<typename in_type, typename out_type, out_type (*converter_func)(in_type)>
		void ReadWithmultichanintegerconverter(out_type** out, uint16_t chans, uint32_t samples);
		//Same as above, but for endiantypes.
		template<typename in_type, typename out_type, out_type (*converter_func)(int32_t)>
		void ReadWithinteger24converter(out_type* out, uint32_t samples);
		//Same as above, for multichan
		template<typename in_type, typename out_type, out_type (*converter_func)(int32_t)>
		void ReadWithmultichaninteger24converter(out_type** out, uint16_t chans, uint32_t samples);


		template<typename in_type, typename out_type, out_type (*converter_func)(in_type, double)>
		void ReadWithfloatconverter(out_type* out, uint32_t numsamples, double multi);
		//Same as above, for multichan
		template<typename in_type, typename out_type, out_type (*converter_func)(in_type, double)>
		void ReadWithmultichanfloatconverter(out_type** out, uint16_t chans, uint32_t numsamples, double multi);


		template<typename sample_type>
		inline void WriteDeinterleaveSamples(void** pSamps, uint16_t chans, uint32_t samples);
		template<typename file_type, typename platform_type>
		inline void WriteDeinterleaveSamplesendian(void** pSamps, uint16_t chans, uint32_t samples);

		template<typename in_type, typename out_type, out_type (*converter_func)(in_type)>
		void WriteWithintegerconverter(in_type* in, uint32_t samples);
		//Same as above, for multichan
		template<typename in_type, typename out_type, out_type (*converter_func)(in_type)>
		void WriteWithmultichanintegerconverter(in_type** in, uint16_t chans, uint32_t samples);


	private:
		bool write_mode;
		std::string file_name_;
		std::fstream _stream;
		std::streamsize filesize;
	};
	
	//
	// Inline code
	//////////////////////////////////////
	std::string const & AbstractIff::file_name() const throw() {
		return file_name_;
	}

	void AbstractIff::Read(uint8_t & x) {
		ReadRaw(&x,1);
	}
	void AbstractIff::Read(int8_t & x) {
		ReadRaw(&x,1);
	}
	void AbstractIff::Read(char & x) {
		ReadRaw(&x,1);
	}
	void AbstractIff::Read(bool & x) {
		uint8_t c;
		Read(c);
		x = c;
	}
	void AbstractIff::ReadBE(uint64_t & x) {
		Long64BE x1;
		Read(x1);
		x = x1.unsignedValue();
	}
	void AbstractIff::ReadBE(int64_t & x) {
		Long64BE x1;
		Read(x1);
		x = x1.signedValue();
	}
	void AbstractIff::ReadBE(uint32_t & x) {
		LongBE x1;
		Read(x1);
		x = x1.unsignedValue();
	}
	void AbstractIff::ReadBE(int32_t & x) {
		LongBE x1;
		Read(x1);
		x = x1.signedValue();
	}
	void AbstractIff::ReadBE(uint16_t & x) {
		ShortBE x1;
		Read(x1);
		x = x1.unsignedValue();
	}
	void AbstractIff::ReadBE(int16_t & x) {
		ShortBE x1;
		Read(x1);
		x = x1.signedValue();
	}
	void AbstractIff::ReadBE(float & x) {
		FloatBE x1;
		Read(x1);
		x = x1.value();
	}
	void AbstractIff::ReadBE(double & x) {
		DoubleBE x1;
		Read(x1);
		x = x1.value();
	}
	void AbstractIff::ReadLE(uint64_t & x) {
		Long64LE x1;
		Read(x1);
		x = x1.unsignedValue();
	}
	void AbstractIff::ReadLE(int64_t & x) {
		Long64LE x1;
		Read(x1);
		x = x1.signedValue();
	}
	void AbstractIff::ReadLE(uint32_t & x) {
		LongLE x1;
		Read(x1);
		x = x1.unsignedValue();
	}
	void AbstractIff::ReadLE(int32_t & x) {
		LongLE x1;
		Read(x1);
		x = x1.signedValue();
	}
	void AbstractIff::ReadLE(uint16_t & x) {
		ShortLE x1;
		Read(x1);
		x = x1.unsignedValue();
	}
	void AbstractIff::ReadLE(int16_t & x) {
		ShortLE x1;
		Read(x1);
		x = x1.signedValue();
	}
	void AbstractIff::ReadLE(float & x) {
		FloatLE x1;
		Read(x1);
		x = x1.value();
	}
	void AbstractIff::ReadLE(double & x) {
		DoubleLE x1;
		Read(x1);
		x = x1.value();
	}

	void AbstractIff::Read(IffChunkId& id) {
		ReadRaw((char*)id, sizeof(IffChunkId));
	}
	void AbstractIff::Read(Long64BE& val) {
		ReadRaw(&val.d.byte,8);
	}
	void AbstractIff::Read(Long64LE& val) {
		ReadRaw(&val.d.byte,8);
	}
	void AbstractIff::Read(LongBE& val) {
		ReadRaw(&val.d.byte,4);
	}
	void AbstractIff::Read(LongLE& val) {
		ReadRaw(&val.d.byte,4);
	}
	void AbstractIff::Read(Long24BE& val) {
		ReadRaw(&val.byte,3);
	}
	void AbstractIff::Read(Long24LE& val) {
		ReadRaw(&val.byte,3);
	}
	void AbstractIff::Read(ShortBE& val) {
		ReadRaw(&val.d.byte,2);
	}
	void AbstractIff::Read(ShortLE& val) {
		ReadRaw(&val.d.byte,2);
	}
	void AbstractIff::Read(FloatBE& val) {
		ReadRaw(&val.d.byte,4);
	}
	void AbstractIff::Read(FloatLE& val) {
		ReadRaw(&val.d.byte,4);
	}
	void AbstractIff::Read(DoubleBE& val) {
		ReadRaw(&val.d.byte,8);
	}
	void AbstractIff::Read(DoubleLE& val) {
		ReadRaw(&val.d.byte,8);
	}

	void AbstractIff::Write(uint8_t x) {
		WriteRaw(&x,1);
	}

	void AbstractIff::Write(int8_t x) {
		WriteRaw(&x,1);
	}

	void AbstractIff::Write(char x) {
		WriteRaw(&x,1);
	} 

	void AbstractIff::Write(bool x) {
		uint8_t c = x; Write(c);
	}
	void AbstractIff::WriteBE(uint64_t x) {
		Long64BE x2(x);
		Write(x2);
	}
	void AbstractIff::WriteBE(int64_t x) {
		Long64BE x2(x);
		Write(x2);
	}
	void AbstractIff::WriteBE(uint32_t x) {
		LongBE x2(x);
		Write(x2);
	}
	void AbstractIff::WriteBE(int32_t x) {
		LongBE x2(x);
		Write(x2);
	}
	void AbstractIff::WriteBE(uint16_t x) {
		ShortBE x2(x);
		Write(x2);
	}
	void AbstractIff::WriteBE(int16_t x) {
		ShortBE x2(x);
		Write(x2);
	}
	void AbstractIff::WriteBE(float x) {
		FloatBE x2(x);
		Write(x2);
	}
	void AbstractIff::WriteBE(double x) {
		DoubleBE x1(x);
		Write(x1);
	}
	void AbstractIff::WriteLE(uint64_t x) {
		Long64LE x2(x);
		Write(x2);
	}
	void AbstractIff::WriteLE(int64_t x) {
		Long64LE x2(x);
		Write(x2);
	}
	void AbstractIff::WriteLE(uint32_t x) {
		LongLE x2(x);
		Write(x2);
	}
	void AbstractIff::WriteLE(int32_t x) {
		LongLE x2(x);
		Write(x2);
	}
	void AbstractIff::WriteLE(uint16_t x) {
		ShortLE x2(x);
		Write(x2);
	}
	void AbstractIff::WriteLE(int16_t x) {
		ShortLE x2(x);
		WriteRaw(&x2.d.byte, 2);
	}
	void AbstractIff::WriteLE(float x) {
		FloatLE x2(x);
		Write(x2);
	}
	void AbstractIff::WriteLE(double x) {
		DoubleLE x1(x);
		Write(x1);
	}

	void AbstractIff::Write(const IffChunkId& id) {
		WriteArray(id, sizeof(IffChunkId));
	}
	void AbstractIff::Write(const Long64BE& val) {
		WriteRaw(&val.d.byte, 8);
	}
	void AbstractIff::Write(const Long64LE& val) {
		WriteRaw(&val.d.byte, 8);
	}
	void AbstractIff::Write(const LongBE& val) {
		WriteRaw(&val.d.byte, 4);
	}
	void AbstractIff::Write(const LongLE& val) {
		WriteRaw(&val.d.byte, 4);
	}
	void AbstractIff::Write(const Long24BE& val) {
		WriteRaw(&val.byte, 3);
	}
	void AbstractIff::Write(const Long24LE& val) {
		WriteRaw(&val.byte, 3);
	}
	void AbstractIff::Write(const ShortBE& val) {
		WriteRaw(&val.d.byte, 2);
	}
	void AbstractIff::Write(const ShortLE& val) {
		WriteRaw(&val.d.byte, 2);
	}
	void AbstractIff::Write(const FloatBE& val) {
		WriteRaw(&val.d.byte, 2);
	}
	void AbstractIff::Write(const FloatLE& val) {
		WriteRaw(&val.d.byte, 2);
	}
	void AbstractIff::Write(const DoubleBE& val) {
		WriteRaw(&val.d.byte, 2);
	}
	void AbstractIff::Write(const DoubleLE& val) {
		WriteRaw(&val.d.byte, 2);
	}

	void AbstractIff::ReadRaw (void * data, std::size_t const bytes)
	{
		if (_stream.eof()) throw std::runtime_error("Read failed");
		_stream.read(reinterpret_cast<char*>(data) ,bytes);
		if (_stream.eof() || _stream.bad()) throw std::runtime_error("Read failed");
	}

	void AbstractIff::WriteRaw(const void * const data, std::size_t const bytes)
	{
		_stream.write(reinterpret_cast<const char*>(data), bytes);
		if (_stream.bad()) throw std::runtime_error("write failed");
	}

	void AbstractIff::ReadArray(uint8_t* thearray, std::size_t n)
	{
		ReadRaw(thearray,n);
	}
	void AbstractIff::ReadArray(int8_t* thearray, std::size_t n)
	{
		ReadRaw(thearray,n);
	}
	void AbstractIff::ReadArray(char* thearray, std::size_t n)
	{
		ReadRaw(thearray,n);
	}
	void AbstractIff::ReadArray(Long64BE* thearray, std::size_t n)
	{
		ReadRaw(thearray,n*8);
	}
	void AbstractIff::ReadArray(Long64LE* thearray, std::size_t n)
	{
		ReadRaw(thearray,n*8);
	}
	void AbstractIff::ReadArray(LongBE* thearray, std::size_t n)
	{
		ReadRaw(thearray,n*4);
	}
	void AbstractIff::ReadArray(LongLE* thearray, std::size_t n)
	{
		ReadRaw(thearray,n*4);
	}
	void AbstractIff::ReadArray(Long24BE* thearray, std::size_t n)
	{
		ReadRaw(thearray,n*3);
	}
	void AbstractIff::ReadArray(Long24LE* thearray, std::size_t n)
	{
		ReadRaw(thearray,n*3);
	}
	void AbstractIff::ReadArray(ShortBE* thearray, std::size_t n)
	{
		ReadRaw(thearray,n*2);
	}
	void AbstractIff::ReadArray(ShortLE* thearray, std::size_t n)
	{
		ReadRaw(thearray,n*2);
	}
	void AbstractIff::ReadArray(FloatBE* thearray, std::size_t n)
	{
		ReadRaw(thearray,n*4);
	}
	void AbstractIff::ReadArray(FloatLE* thearray, std::size_t n)
	{
		ReadRaw(thearray,n*4);
	}
	void AbstractIff::ReadArray(DoubleBE* thearray, std::size_t n)
	{
		ReadRaw(thearray,n*8);
	}
	void AbstractIff::ReadArray(DoubleLE* thearray, std::size_t n)
	{
		ReadRaw(thearray,n*8);
	}
	template<typename T>
	void AbstractIff::ReadArray(T* thearray, std::size_t n) {
		for(std::size_t i=0;i<n;i++) Read(thearray[i]);
	}




	void AbstractIff::WriteArray(uint8_t const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n);
	}
	void AbstractIff::WriteArray(int8_t const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n);
	}
	void AbstractIff::WriteArray(char const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n);
	}

	void AbstractIff::WriteArray(Long64BE const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n*8);
	}
	void AbstractIff::WriteArray(Long64LE const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n*8);
	}
	void AbstractIff::WriteArray(LongBE const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n*4);
	}
	void AbstractIff::WriteArray(LongLE const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n*4);
	}
	void AbstractIff::WriteArray(Long24BE const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n*3);
	}
	void AbstractIff::WriteArray(Long24LE const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n*3);
	}
	void AbstractIff::WriteArray(ShortBE const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n*2);
	}
	void AbstractIff::WriteArray(ShortLE const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n*2);
	}
	void AbstractIff::WriteArray(FloatBE const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n*4);
	}
	void AbstractIff::WriteArray(FloatLE const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n*4);
	}
	void AbstractIff::WriteArray(DoubleBE const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n*8);
	}
	void AbstractIff::WriteArray(DoubleLE const* thearray, std::size_t n)
	{
		WriteRaw(thearray,n*8);
	}

	template<typename T>
	void AbstractIff::WriteArray(T const* thearray, std::size_t n) {
		for(std::size_t i=0;i<n;i++) Write(thearray[i]);
	}



	//Templates to use with Audio subclasses.
	///////////////////////////////////////


	template<typename sample_type>
	inline void AbstractIff::readDeinterleaveSamples(void** pSamps, uint16_t chans, uint32_t samples) {
		sample_type** samps = reinterpret_cast<sample_type**>(pSamps);
		ReadWithmultichanintegerconverter<sample_type, sample_type,assignconverter<sample_type,sample_type> >(samps, chans, samples);
	}
	template<typename file_type, typename platform_type>
	inline void AbstractIff::readDeinterleaveSamplesendian(void** pSamps, uint16_t chans, uint32_t samples) {
		platform_type** samps = reinterpret_cast<platform_type**>(pSamps);
		ReadWithmultichanintegerconverter<file_type, platform_type,endianessconverter<file_type,platform_type> >(samps, chans, samples);
	}

	template<typename in_type, typename out_type, out_type (*converter_func)(in_type)>
	void AbstractIff::ReadWithintegerconverter(out_type* out, uint32_t samples)
	{
		in_type samps[32768];
		uint32_t amount=0;
		for(uint32_t io = 0; io < samples; io+=amount) {
			amount = std::min(static_cast<uint32_t>(32768U),samples-io);
			ReadArray(samps,amount);
			in_type* psamps = samps;
			for(uint32_t b = 0 ; b < amount; ++b) {
				*out=converter_func(*psamps);
				out++;
				psamps++;
			}
		}
	}
	//Same as above, for multichan, deinterlaced
	template<typename in_type, typename out_type, out_type (*converter_func)(in_type)>
	void AbstractIff::ReadWithmultichanintegerconverter(out_type** out, uint16_t chans, uint32_t samples)
	{
		in_type samps[32768];
		uint32_t amount=0;
		for(uint32_t io = 0 ; io < samples ; io+=amount)
		{
			amount = std::min(static_cast<uint32_t>(32768U)/chans,samples-io);
			ReadArray(samps, amount*chans);
			in_type* psamps = samps;
			for (uint32_t a=0; a < amount; a++) {
				for (uint16_t b=0; b < chans; b++) {
					out[b][io+a]=converter_func(*psamps);
					psamps++;
				}
			}
		}
	}

	template<typename in_type, typename out_type, out_type (*converter_func)(int32_t)>
	void AbstractIff::ReadWithinteger24converter(out_type* out, uint32_t samples)
	{
		in_type samps[32768];
		uint32_t amount=0;
		for(uint32_t io = 0 ; io < samples ; io+=amount)
		{
			amount = std::min(static_cast<uint32_t>(32768U),samples-io);
			ReadArray(samps, amount);
			in_type* psamps = samps;
			for(uint32_t b = 0 ; b < amount; ++b) {
				*out=converter_func(psamps->signedValue());
				out++;
				psamps++;
			}
		}
	}

	//Same as above, but for multichan, deinterlaced
	template<typename in_type, typename out_type, out_type (*converter_func)(int32_t)>
	void AbstractIff::ReadWithmultichaninteger24converter(out_type** out, uint16_t chans, uint32_t samples)
	{
		in_type samps[32768];
		uint32_t amount=0;
		for(uint32_t io = 0 ; io < samples ; io+=amount)
		{
			amount = std::min(static_cast<uint32_t>(32768U)/chans,samples-io);
			ReadArray(samps, amount*chans);
			in_type* psamps = samps;
			for (uint32_t a=0; a < amount; a++) {
				for (uint16_t b=0; b < chans; b++) {
					out[b][io+a]=converter_func(psamps->signedValue());
					psamps++;
				}
			}
		}
	}



	template<typename in_type, typename out_type, out_type (*converter_func)(in_type, double)>
	void AbstractIff::ReadWithfloatconverter(out_type* out, uint32_t numsamples, double multi) {
		in_type samps[32768];
		uint32_t amount=0;
		for(uint32_t io = 0; io < numsamples; io+=amount) {
			amount = std::min(static_cast<uint32_t>(32768U),numsamples-io);
			ReadArray(samps,amount);
			in_type* psamps = samps;
			for(uint32_t b = 0 ; b < amount; ++b) {
				*out=converter_func(*psamps, multi);
				out++;
				psamps++;
			}
		}
	}

	template<typename in_type, typename out_type, out_type (*converter_func)(in_type, double)>
	void AbstractIff::ReadWithmultichanfloatconverter(out_type** out, uint16_t chans, uint32_t numsamples, double multi) {
		in_type samps[32768];
		uint32_t amount=0;
		for(uint32_t io = 0; io < numsamples; io+=amount) {
			amount = std::min(static_cast<uint32_t>(32768U)/chans,numsamples-io);
			ReadArray(samps,amount*chans);
			in_type* psamps = samps;
			for (uint32_t a=0; a < amount; a++) {
				for (uint16_t b=0; b < chans; b++) {
					out[b][io+a]=converter_func(*psamps, multi);
					psamps++;
				}
			}
		}
	}

	template<typename sample_type>
	inline void AbstractIff::WriteDeinterleaveSamples(void** pSamps, uint16_t chans, uint32_t samples) {
		sample_type** samps = reinterpret_cast<sample_type**>(pSamps);
		WriteWithmultichanintegerconverter<sample_type, sample_type,assignconverter<sample_type,sample_type> >(samps, chans, samples);
	}
	template<typename file_type, typename platform_type>
	inline void AbstractIff::WriteDeinterleaveSamplesendian(void** pSamps, uint16_t chans, uint32_t samples) {
		platform_type** samps = reinterpret_cast<platform_type**>(pSamps);
		WriteWithmultichanintegerconverter<platform_type,file_type,endianessconverter<platform_type,file_type> >(samps, chans, samples);
	}


	template<typename in_type, typename out_type, out_type (*converter_func)(in_type)>
	void AbstractIff::WriteWithintegerconverter(in_type* in, uint32_t samples)
	{
		out_type samps[32768];
		uint32_t amount=0;
		for(uint32_t io = 0; io < samples; io+=amount) {
			amount = std::min(static_cast<uint32_t>(32768U),samples-io);
			out_type* psamps = samps;
			for(uint32_t b = 0 ; b < amount; ++b) {
				*psamps=converter_func(*in);
				in++;
				psamps++;
			}
			WriteArray(samps,amount);
		}
	}
	//Same as above, for multichan, deinterlaced
	template<typename in_type, typename out_type, out_type (*converter_func)(in_type)>
	void AbstractIff::WriteWithmultichanintegerconverter(in_type** in, uint16_t chans, uint32_t samples)
	{
		out_type samps[32768];
		uint32_t amount=0;
		for(uint32_t io = 0 ; io < samples ; io+=amount)
		{
			amount = std::min(static_cast<uint32_t>(32768U)/chans,samples-io);
			out_type* psamps = samps;
			for (uint32_t a=0; a < amount; a++) {
				for (uint16_t b=0; b < chans; b++) {
					*psamps=converter_func(in[b][io+a]);
					psamps++;
				}
			}
			WriteArray(samps, amount*chans);
		}
	}



}}
