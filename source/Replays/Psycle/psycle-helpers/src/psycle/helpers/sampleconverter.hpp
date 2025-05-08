#pragma once
#include <universalis.hpp>
#include <psycle/helpers/math.hpp>

namespace psycle
{
	namespace helpers
	{
		using namespace universalis::stdlib;

		/// usage: integerconverter<uint8_t,int16_t,uint8toint16>(bla,bla2,10);
		template<typename in_type, typename out_type, out_type (*converter_func)(in_type)>
		void integerconverter(in_type* in, out_type* out, std::size_t numsamples) {
			for(std::size_t io = 0 ; io < numsamples ; ++io){
				*out=converter_func(*in);
				out++;
				in++;
			}
		}

		/// usage: floatconverter<float,int16_t,floattoint16>(bla,bla2,10, 32767.f);
		template<typename in_type, typename out_type, out_type (*converter_func)(in_type, double)>
		void floatconverter(in_type* in, out_type* out, std::size_t numsamples, double multi) {
			for(std::size_t io = 0 ; io < numsamples ; ++io){
				*out=converter_func(*in, multi);
				out++;
				in++;
			}
		}

		//unpack 4 int24 values (in 12bytes, out 16bytes).
		inline void unpackint24(uint32_t *in, int32_t *out);
		//pack 4 int24 values (in 16bytes, out 12bytes).
		inline void packint24(int32_t *in, uint32_t *out);


		//Converter functions:
		/// usage: integerconverter<uint8_t,int8_t,assignconverter<uint8_t, uint8_t>(bla,bla2,10);
		template<typename in_type, typename out_type>
		inline out_type assignconverter(in_type a) { return a; }

			
		/// usage: integerconverter<Long24BE,Long24LE,endianessconverter<Long24BE, Long24LE>(bla,bla2,10);
		/// Requires helpers/file/endiantypes.hpp
		template<typename in_type, typename out_type>
		inline out_type endianessconverter(in_type a) { out_type b(a.unsignedValue()); return b; }

		inline uint8_t int16touint8(int16_t);
		inline uint8_t int24touint8(int32_t);
		inline uint8_t int32touint8(int32_t);
		inline uint8_t floattouint8(float val, double multi=127.f); // 7bits
		inline uint8_t doubletouint8(double val, double multi=127.f); // 7bits
		inline int16_t uint8toint16(uint8_t);
		inline int16_t int24toint16(int32_t);
		inline int16_t int32toint16(int32_t);
		inline int16_t floattoint16(float val, double multi=32767.f); // 15bits
		inline int16_t doubletoint16(double val, double multi=32767.f); // 15bits
		inline int32_t uint8toint24(uint8_t);
		inline int32_t int16toint24(int16_t);
		inline int32_t int32toint24(int32_t);
		inline int32_t floattoint24(float val, double multi=8388607.0);//23bits
		inline int32_t doubletoint24(double val, double multi=8388607.0);//23bits
		inline int32_t uint8toint32(uint8_t);
		inline int32_t int16toint32(int16_t);
		inline int32_t int24toint32(int32_t);
		inline int32_t floattoint32(float val, double multi=2147483647.0);//31bits
		inline int32_t doubletoint32(double val, double multi=2147483647.0);//31bits
		inline float uint8tofloat(uint8_t val, double multi=0.0078125f); // 1/128
		inline float int16tofloat(int16_t val, double multi=0.000030517578125f); // 1/32768
		inline float int24tofloat(int32_t val, double multi=0.00000011920928955078125); // 1/8388608
		inline float int32tofloat(int32_t val, double multi=0.0000000004656612873077392578125); // 1/2147483648
		inline double uint8todouble(uint8_t val, double multi=0.0078125f); // 1/128
		inline double int16todouble(int16_t val, double multi=0.000030517578125f); // 1/32768
		inline double int24todouble(int32_t val, double multi=0.00000011920928955078125); // 1/8388608
		inline double int32todouble(int32_t val, double multi=0.0000000004656612873077392578125); // 1/2147483648


		//Inline Implementation.

		uint8_t int16touint8(int16_t val) {
			return static_cast<uint8_t>((static_cast<uint16_t>(val) + 32768U) >> 8);
		}
		uint8_t int24touint8(int32_t val) {
			return static_cast<uint8_t>((val + 8388608) >> 16);
		}
		uint8_t int32touint8(int32_t val) {
			return static_cast<uint8_t>((static_cast<uint32_t>(val) + 2147483648U) >> 24);
		}
		uint8_t floattouint8(float val, double multi) {
			return math::rint<int8_t,float>(std::floor(val*multi))+128;
		}
		uint8_t doubletouint8(double val, double multi) {
			return math::rint<int8_t,double>(std::floor(val*multi))+128;
		}
		int16_t uint8toint16(uint8_t val) {
			return static_cast<int16_t>((static_cast<int16_t>(val) << 8) - 32768);
		}
		int16_t int24toint16(int32_t val) {
			return static_cast<int16_t>(val >> 8);
		}
		int16_t int32toint16(int32_t val) {
			return static_cast<int16_t>(val >> 16);
		}
		int16_t floattoint16(float val, double multi) {
			return math::rint<int16_t,float>(std::floor(val*multi));
		}
		int16_t doubletoint16(double val, double multi) {
			return math::rint<int16_t,double>(std::floor(val*multi));
		}
		int32_t uint8toint24(uint8_t val) {
			return (static_cast<int32_t>(val) << 16) - 8388608;
		}
		int32_t int16toint24(int16_t val){
			return val << 8;
		}
		int32_t int32toint24(int32_t val){
			return val >> 8;
		}
		int32_t floattoint24(float val, double multi) {
			return math::rint<int32_t,float>(std::floor(val*multi));
		}
		int32_t doubletoint24(double val, double multi) {
			return math::rint<int32_t,double>(std::floor(val*multi));
		}
		int32_t uint8toint32(uint8_t val) {
			return (static_cast<int32_t>(val) << 24) - 2147483648;
		}
		int32_t int16toint32(int16_t val) {
			return val << 16;
		}
		int32_t int24toint32(int32_t val) {
			return val << 8;
		}
		int32_t floattoint32(float val, double multi) {
			return math::rint<int32_t,float>(std::floor(val*multi));
		}
		int32_t doubletoint32(double val, double multi) {
			return math::rint<int32_t,double>(std::floor(val*multi));
		}
		float uint8tofloat(uint8_t val, double multi) {
			return val*multi-1.f;
		}
		float int16tofloat(int16_t val, double multi) {
			return val*multi;
		}
		float int24tofloat(int32_t val, double multi) {
			return val*multi;
		}
		float int32tofloat(int32_t val, double multi) {
			return val*multi;
		}

		void unpackint24(uint32_t *in, int32_t *out)
		{
			out[0]=  (in[0] &0xFFFFFF);
			out[1]= ((in[1] &0xFFFF) << 8) | (in[0] >> 24);
			out[2]= ((in[2] &0xFF) << 16)  | (in[1] >> 16);
			out[3]=                          (in[2] >> 8 );
		}
		void packint24(int32_t *in, uint32_t *out)
		{
			out[0]=(in[0] &0xFFFFFF) | ((in[1] &0xFF) >> 16);
			out[1]=(in[1] &0xFFFF)   | ((in[2] &0xFFFF) >>  8);
			out[2]=(in[2] &0xFF)     |  (in[3] &0xFFFFFF);
		}
	}
}

