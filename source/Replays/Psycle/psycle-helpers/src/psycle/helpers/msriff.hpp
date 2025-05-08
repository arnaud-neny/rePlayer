/**
	\file
	interface file for psycle::helpers::MsRiff.
	
*/
#pragma once
#include "abstractiff.hpp"

namespace psycle { namespace helpers {

	using namespace universalis::stdlib;

	/******* Data Structures *******/
	class RiffChunkHeader : public BaseChunkHeader
	{
		friend class MsRiff;
	protected:
		LongLE ulength;
	public:
		RiffChunkHeader(){}
		RiffChunkHeader(const IffChunkId& newid, uint32_t newsize) 
			: BaseChunkHeader(newid){ setLength(newsize);}
		virtual ~RiffChunkHeader(){}
		virtual void setLength(uint32_t newlength) { ulength.changeValue(newlength); }
		virtual uint32_t length() const { return ulength.unsignedValue(); }
		//sizeof(RiffChunkHeader) does not return 8.
		static const std::size_t SIZEOF;
	};
	class RifxChunkHeader : public BaseChunkHeader
	{
		friend class MsRiff;
	protected:
		LongBE ulength;
	public:
		RifxChunkHeader(){};
		RifxChunkHeader(const IffChunkId& newid, uint32_t newsize)
			: BaseChunkHeader(newid) { setLength(newsize); }
		virtual ~RifxChunkHeader(){};
		virtual void setLength(uint32_t newlength) { ulength.changeValue(newlength); }
		virtual uint32_t length() const { return ulength.unsignedValue(); }
		//sizeof(RifxChunkHeader) does not return 8.
		static const std::size_t SIZEOF;
	};


	/*********  IFF file reader comforming to Microsoft RIFF/RIFX specifications ****/
	class MsRiff : public AbstractIff
	{
	public:
		MsRiff():headerPosition(0)
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			, isLittleEndian(true)
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			, isLittleEndian(false)
#endif
			, isValid(false)
			, tryPaddingFix(false){}
		virtual ~MsRiff(){}

		virtual bool isValidFile() const;

		virtual void Open(const std::string& fname);
		virtual void Create(const std::string& fname, bool overwrite, 
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			bool littleEndian=true
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			bool littleEndian=false
#endif
		);
		virtual void Close();
		virtual bool Eof() const;

		virtual void addNewChunk(const BaseChunkHeader& header);
		virtual const BaseChunkHeader& readHeader();
		virtual const BaseChunkHeader& findChunk(const IffChunkId& id, bool allowWrap=false);

		virtual void skipThisChunk();
		virtual void UpdateCurrentChunkSize();

		//We need to override all the overloaded members, else the non-overloaded become non-accessible.
		inline void ReadArray(uint8_t* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(int8_t* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(char* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(uint16_t* thearray, std::size_t n);
		inline void ReadArray(int16_t* thearray, std::size_t n);
		inline void ReadArray(uint32_t* thearray, std::size_t n);
		inline void ReadArray(int32_t* thearray, std::size_t n);
		inline void ReadArray(uint64_t* thearray, std::size_t n);
		inline void ReadArray(int64_t* thearray, std::size_t n);
		inline void ReadArray(float* thearray, std::size_t n);
		inline void ReadArray(double* thearray, std::size_t n);
		inline void ReadArray(Long64BE*  thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(Long64LE* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(LongBE* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(LongLE* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(Long24BE* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(Long24LE* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(ShortBE* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(ShortLE* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(FloatBE* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(FloatLE* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(DoubleBE* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		inline void ReadArray(DoubleLE* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }
		///Fallback for other types
		template<typename T>
		inline void ReadArray(T* thearray, std::size_t n) { AbstractIff::ReadArray(thearray,n); }

		inline void Read(uint8_t & x) { AbstractIff::Read(x); }
		inline void Read(int8_t & x) { AbstractIff::Read(x); }
		inline void Read(uint16_t & x) { if (isLittleEndian) {ReadLE(x);} else {ReadBE(x);} }
		inline void Read(int16_t & x) { if (isLittleEndian) {ReadLE(x);} else {ReadBE(x);} }
		inline void Read(uint32_t & x) { if (isLittleEndian) {ReadLE(x);} else {ReadBE(x);} }
		inline void Read(int32_t & x) { if (isLittleEndian) {ReadLE(x);} else {ReadBE(x);} }
		inline void Read(uint64_t& x) { if (isLittleEndian) {ReadLE(x);} else {ReadBE(x);} }
		inline void Read(int64_t& x) { if (isLittleEndian) {ReadLE(x);} else {ReadBE(x);} }
		inline void Read(float & x) { if (isLittleEndian) {ReadLE(x);} else {ReadBE(x);} }
		inline void Read(double & x) { if (isLittleEndian) {ReadLE(x);} else {ReadBE(x);} }
		inline void Read(char & x) { AbstractIff::Read(x); }
		inline void Read(bool & x) { AbstractIff::Read(x); }
		inline void Read(IffChunkId & id) {AbstractIff::Read(id);}
		inline void Read(Long64BE & val) {AbstractIff::Read(val);}
		inline void Read(Long64LE & val) {AbstractIff::Read(val);}
		inline void Read(LongBE & val) {AbstractIff::Read(val);}
		inline void Read(LongLE & val) {AbstractIff::Read(val);}
		inline void Read(Long24BE & val){ AbstractIff::Read(val); }
		inline void Read(Long24LE & val){ AbstractIff::Read(val); }
		inline void Read(ShortBE & val){ AbstractIff::Read(val); }
		inline void Read(ShortLE & val){ AbstractIff::Read(val); }
		inline void Read(FloatBE & val){ AbstractIff::Read(val); }
		inline void Read(FloatLE & val){ AbstractIff::Read(val); }
		inline void Read(DoubleBE & val){ AbstractIff::Read(val); }
		inline void Read(DoubleLE & val){ AbstractIff::Read(val); }

		inline void WriteArray(uint8_t const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(int8_t const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(char const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(uint16_t const* thearray, std::size_t n);
		inline void WriteArray(int16_t const* thearray, std::size_t n);
		inline void WriteArray(uint32_t const* thearray, std::size_t n);
		inline void WriteArray(int32_t const* thearray, std::size_t n);
		inline void WriteArray(uint64_t const* thearray, std::size_t n);
		inline void WriteArray(int64_t const* thearray, std::size_t n);
		inline void WriteArray(float const* thearray, std::size_t n);
		inline void WriteArray(double const* thearray, std::size_t n);
		inline void WriteArray(Long64BE const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(Long64LE const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(LongBE const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(LongLE const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(Long24BE const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(Long24LE const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(ShortBE const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(ShortLE const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(FloatBE const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(FloatLE const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(DoubleBE const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		inline void WriteArray(DoubleLE const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }
		///Fallback for other types
		template<typename T>
		inline void WriteArray(T const* thearray, std::size_t n) { AbstractIff::WriteArray(thearray,n); }

        inline void Write(uint8_t x) { AbstractIff::Write(x); }
        inline void Write(int8_t x) { AbstractIff::Write(x); }
        inline void Write(uint16_t x) { if (isLittleEndian) {WriteLE(x);} else {WriteBE(x);} }
		inline void Write(int16_t x) { if (isLittleEndian) {WriteLE(x);} else {WriteBE(x);} }
		inline void Write(uint32_t x) { if (isLittleEndian) {WriteLE(x);} else {WriteBE(x);} }
		inline void Write(int32_t x) { if (isLittleEndian) {WriteLE(x);} else {WriteBE(x);} }
		inline void Write(uint64_t x)  { if (isLittleEndian) {WriteLE(x);} else {WriteBE(x);} }
		inline void Write(int64_t x)  { if (isLittleEndian) {WriteLE(x);} else {WriteBE(x);} }
		inline void Write(float x) { if (isLittleEndian) {WriteLE(x);} else {WriteBE(x);} }
		inline void Write(double x) { if (isLittleEndian) {WriteLE(x);} else {WriteBE(x);} }
		inline void Write(char x) { AbstractIff::Write(x); }
		inline void Write(bool x) { AbstractIff::Write(x); }
		inline void Write(const IffChunkId & id) { AbstractIff::Write(id); }
		inline void Write(const Long64BE & val) { AbstractIff::Write(val); }
		inline void Write(const Long64LE & val) { AbstractIff::Write(val); }
		inline void Write(const LongBE & val) { AbstractIff::Write(val); }
		inline void Write(const LongLE & val) { AbstractIff::Write(val); }
		inline void Write(const Long24BE& val){ AbstractIff::Write(val); }
		inline void Write(const Long24LE& val){ AbstractIff::Write(val); }
		inline void Write(const ShortBE& val){ AbstractIff::Write(val); }
		inline void Write(const ShortLE& val){ AbstractIff::Write(val); }
		inline void Write(const FloatBE& val){ AbstractIff::Write(val); }
		inline void Write(const FloatLE& val){ AbstractIff::Write(val); }
		inline void Write(const DoubleBE& val){ AbstractIff::Write(val); }
		inline void Write(const DoubleLE& val){ AbstractIff::Write(val); }

		static const IffChunkId RIFF;
		static const IffChunkId RIFX;
	protected:

		RiffChunkHeader currentHeaderLE;
		RifxChunkHeader currentHeaderBE;
		BaseChunkHeader* currentHeader;
		//RIFF only supports up to 32bits. for 64bits there's the RIFF64 format (and the W64 format...)
		std::streampos headerPosition;
		bool isLittleEndian;
		bool isValid;
		bool tryPaddingFix;
	};


	///Inline code
	//
	void MsRiff::ReadArray(uint16_t* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) ReadRaw(thearray,n*2);
		else for(std::size_t i=0;i<n;i++) Read(thearray[i]);
	}
	void MsRiff::ReadArray(int16_t* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) ReadRaw(thearray,n*2);
		else for(std::size_t i=0;i<n;i++) Read(thearray[i]);
	}
	void MsRiff::ReadArray(uint32_t* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) ReadRaw(thearray,n*4);
		else for(std::size_t i=0;i<n;i++) Read(thearray[i]);
	}
	void MsRiff::ReadArray(int32_t* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) ReadRaw(thearray,n*4);
		else for(std::size_t i=0;i<n;i++) Read(thearray[i]);
	}
	void MsRiff::ReadArray(uint64_t* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) ReadRaw(thearray,n*8);
		else for(std::size_t i=0;i<n;i++) Read(thearray[i]);
	}
	void MsRiff::ReadArray(int64_t* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) ReadRaw(thearray,n*8);
		else for(std::size_t i=0;i<n;i++) Read(thearray[i]);
	}
	void MsRiff::ReadArray(float* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) ReadRaw(thearray,n*4);
		else for(std::size_t i=0;i<n;i++) Read(thearray[i]);
	}
	void MsRiff::ReadArray(double* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) ReadRaw(thearray,n*8);
		else for(std::size_t i=0;i<n;i++) Read(thearray[i]);
	}


	void MsRiff::WriteArray(uint16_t const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) WriteRaw(thearray,n*2);
		else for(std::size_t i=0;i<n;i++) Write(thearray[i]);
	}
	void MsRiff::WriteArray(int16_t const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) WriteRaw(thearray,n*2);
		else for(std::size_t i=0;i<n;i++) Write(thearray[i]);
	}
	void MsRiff::WriteArray(uint32_t const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) WriteRaw(thearray,n*4);
		else for(std::size_t i=0;i<n;i++) Write(thearray[i]);
	}
	void MsRiff::WriteArray(int32_t const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) WriteRaw(thearray,n*4);
		else for(std::size_t i=0;i<n;i++) Write(thearray[i]);
	}
	void MsRiff::WriteArray(uint64_t const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) WriteRaw(thearray,n*8);
		else for(std::size_t i=0;i<n;i++) Write(thearray[i]);
	}
	void MsRiff::WriteArray(int64_t const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) WriteRaw(thearray,n*8);
		else for(std::size_t i=0;i<n;i++) Write(thearray[i]);
	}
	void MsRiff::WriteArray(float const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) WriteRaw(thearray,n*4);
		else for(std::size_t i=0;i<n;i++) Write(thearray[i]);
	}
	void MsRiff::WriteArray(double const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		bool raw = isLittleEndian;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		bool raw = !isLittleEndian;
#endif
		if (raw) WriteRaw(thearray,n*8);
		else for(std::size_t i=0;i<n;i++) Write(thearray[i]);
	}
}}
