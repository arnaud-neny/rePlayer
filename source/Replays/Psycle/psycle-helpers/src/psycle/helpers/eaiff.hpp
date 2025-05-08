/**
	\file
	interface file for psycle::helpers::EA's iffFile.
	based on information from http://www.borg.com/~jglatt/tech/aboutiff.htm
	See also: http://wiki.amigaos.net/index.php/EA_IFF_85_Standard_for_Interchange_Format_Files
*/
#pragma once
#include "abstractiff.hpp"
namespace psycle { namespace helpers {

using namespace universalis::stdlib;

	/******* Data Structures *******/
	class IffChunkHeader : public BaseChunkHeader
	{
		friend class EaIff;
	protected:
		LongBE ulength;
	public:
		IffChunkHeader() : BaseChunkHeader() {}
		IffChunkHeader(const IffChunkId& newid, uint32_t newsize) 
			: BaseChunkHeader(newid) { setLength(newsize); }
		virtual ~IffChunkHeader(){}
		virtual void setLength(uint32_t newlength) { ulength.changeValue(newlength); }
		virtual uint32_t length() const { return ulength.unsignedValue(); }
		//sizeof(IffChunkHeader) does not return 8.
		static const std::size_t SIZEOF;
	};


	/*********  IFF file reader comforming to EA's specifications ****/
	class EaIff : public AbstractIff
	{
	public:
		EaIff():headerPosition(0),isValid(false){}
		virtual ~EaIff(){}

		virtual bool isValidFile() const;

		virtual void Open(const std::string& fname);
		virtual void Create(const std::string& fname, const bool overwrite);
		virtual void Close();
		virtual bool Eof() const;

		virtual void addNewChunk(const BaseChunkHeader& header);
		virtual const IffChunkHeader& readHeader();
		virtual const IffChunkHeader& findChunk(const IffChunkId& id, bool allowWrap=false);
		virtual void skipThisChunk();
		virtual void UpdateCurrentChunkSize();

		virtual void addFormChunk(const IffChunkId& id);
		virtual void addCatChunk(const IffChunkId& id, bool endprevious=true);
		virtual void addListChunk(const IffChunkId& id, bool endprevious=true);
		virtual void addListProperty(const IffChunkId& contentId, const IffChunkId& propId);
		virtual void addListProperty(const IffChunkId& contentId, const IffChunkId& propId, void const *data, uint32_t dataSize);

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
		inline void Read(uint16_t & x) { ReadBE(x); }
		inline void Read(int16_t & x) { ReadBE(x); }
		inline void Read(uint32_t & x) { ReadBE(x); }
		inline void Read(int32_t & x) { ReadBE(x); }
		inline void Read(uint64_t & x) { ReadBE(x); }
		inline void Read(int64_t & x) { ReadBE(x); }
		inline void Read(float & x) { ReadBE(x); }
		inline void Read(double & x) { ReadBE(x); }
		inline void Read(char & x) { AbstractIff::Read(x); }
		inline void Read(bool & x) { AbstractIff::Read(x); }
		inline void Read(IffChunkId & id) { AbstractIff::Read(id); }
		inline void Read(Long64BE & val){ AbstractIff::Read(val); }
		inline void Read(Long64LE & val){ AbstractIff::Read(val); }
		inline void Read(LongBE & val){ AbstractIff::Read(val); }
		inline void Read(LongLE & val){ AbstractIff::Read(val); }
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

		inline void Write(uint8_t x) { AbstractIff::Write(x); }
		inline void Write(int8_t x) { AbstractIff::Write(x); }
		inline void Write(uint16_t x) { WriteBE(x); }
		inline void Write(int16_t x) { WriteBE(x); }
		inline void Write(uint32_t x) { WriteBE(x); }
		inline void Write(int32_t x) { WriteBE(x); }
		inline void Write(uint64_t x) { WriteBE(x); }
		inline void Write(int64_t x) { WriteBE(x); }
		inline void Write(float x) { WriteBE(x); }
		inline void Write(double x) { WriteBE(x); }
		inline void Write(char x) { AbstractIff::Write(x); }
		inline void Write(bool x) { AbstractIff::Write(x); }
		inline void Write(const Long64BE& val) { AbstractIff::Write(val); }
		inline void Write(const Long64LE& val) { AbstractIff::Write(val); }
		inline void Write(const IffChunkId& id){ AbstractIff::Write(id); }
		inline void Write(const LongBE& val){ AbstractIff::Write(val); }
		inline void Write(const LongLE& val){ AbstractIff::Write(val); }
		inline void Write(const Long24BE& val){ AbstractIff::Write(val); }
		inline void Write(const Long24LE& val){ AbstractIff::Write(val); }
		inline void Write(const ShortBE& val){ AbstractIff::Write(val); }
		inline void Write(const ShortLE& val){ AbstractIff::Write(val); }
		inline void Write(const FloatBE& val) { AbstractIff::Write(val); }
		inline void Write(const FloatLE& val) { AbstractIff::Write(val); }
		inline void Write(const DoubleBE& val) { AbstractIff::Write(val); }
		inline void Write(const DoubleLE& val) { AbstractIff::Write(val); }

		static const IffChunkId grabbag;
		static const IffChunkId FORM;
		static const IffChunkId LIST;
		static const IffChunkId CAT;

	protected:
		void WriteHeader(const IffChunkHeader& header);

		IffChunkHeader currentHeader;
		uint32_t headerPosition;
		bool isValid;
	};


	///Inline code
	//
	void EaIff::ReadArray(uint16_t* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Read(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		ReadRaw(thearray,n*2);
#endif
	}
	void EaIff::ReadArray(int16_t* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Read(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		ReadRaw(thearray,n*2);
#endif
	}
	void EaIff::ReadArray(uint32_t* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Read(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		ReadRaw(thearray,n*4);
#endif
	}
	void EaIff::ReadArray(int32_t* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Read(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		ReadRaw(thearray,n*4);
#endif
	}
	void EaIff::ReadArray(uint64_t* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Read(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		ReadRaw(thearray,n*8);
#endif
	}
	void EaIff::ReadArray(int64_t* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Read(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		ReadRaw(thearray,n*8);
#endif
	}
	void EaIff::ReadArray(float* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Read(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		ReadRaw(thearray,n*4);
#endif
	}
	void EaIff::ReadArray(double* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Read(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		ReadRaw(thearray,n*8);
#endif
	}


	void EaIff::WriteArray(uint16_t const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Write(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		WriteRaw(thearray,n*2);
#endif
	}
	void EaIff::WriteArray(int16_t const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Write(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		WriteRaw(thearray,n*2);
#endif
	}
	void EaIff::WriteArray(uint32_t const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Write(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		WriteRaw(thearray,n*4);
#endif
	}
	void EaIff::WriteArray(int32_t const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Write(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		WriteRaw(thearray,n*8);
#endif
	}
	void EaIff::WriteArray(uint64_t const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Write(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		WriteRaw(thearray,n*8);
#endif
	}
	void EaIff::WriteArray(int64_t const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Write(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		WriteRaw(thearray,n*8);
#endif
	}
	void EaIff::WriteArray(float const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Write(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		WriteRaw(thearray,n*4);
#endif
	}
	void EaIff::WriteArray(double const* thearray, std::size_t n)
	{
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
		for(std::size_t i=0;i<n;i++) Write(thearray[i]);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
		WriteRaw(thearray,n*8);
#endif
	}
}}
