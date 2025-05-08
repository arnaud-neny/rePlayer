/**
	\file
	interface file for the different big-endian and little-endian types.
*/
#pragma once

#include <universalis.hpp>


namespace psycle { namespace helpers {

using namespace universalis::stdlib;

		/***** DATA TYPES *********/
		/// big-endian 64-bit integer.
		class Long64BE
		{
		public:
			union _d{
				struct _byte{
					unsigned char hihihi;
					unsigned char hihilo;
					unsigned char hilohi;
					unsigned char hilolo;
					unsigned char lohihi;
					unsigned char lohilo;
					unsigned char lolohi;
					unsigned char lololo;
				}byte;
				uint64_t originalValue;
			}d;
			Long64BE() { d.originalValue = 0; }
			Long64BE(const uint64_t val) { changeValue(val); }
			Long64BE(const int64_t val) { changeValue(val); }
			inline void changeValue(const uint64_t);
			inline void changeValue(const int64_t);
			inline uint64_t unsignedValue() const;
			inline int64_t signedValue() const;
		};

		/// big-endian 32-bit integer.
		class LongBE
		{
		public:
			union _d{
				struct _byte{
					unsigned char hihi;
					unsigned char hilo;
					unsigned char lohi;
					unsigned char lolo;
				}byte;
				uint32_t originalValue;
			}d;
			LongBE() { d.originalValue = 0; }
			LongBE(const uint32_t val) { changeValue(val); }
			LongBE(const int32_t val) { changeValue(val); }
			inline void changeValue(const uint32_t);
			inline void changeValue(const int32_t);
			inline uint32_t unsignedValue() const;
			inline int32_t signedValue() const;
		};
		/// big-endian 24-bit intenger.
		class Long24BE
		{
		public:
			struct _byte{
				unsigned char hi;
				unsigned char lohi;
				unsigned char lolo;
			}byte;
			Long24BE() { byte.hi = 0; byte.lohi = 0; byte.lolo = 0; }
			Long24BE(const uint32_t val) { changeValue(val); }
			Long24BE(const int32_t val) { changeValue(val); }
			inline void changeValue(const uint32_t);
			inline void changeValue(const int32_t);
			inline uint32_t unsignedValue() const;
			inline int32_t signedValue() const;
		};
		/// big-endian 16-bit integer.
		class ShortBE
		{
		public:
			union _d {
				struct _byte{
					unsigned char hi;
					unsigned char lo;
				}byte;
				uint16_t originalValue;
			}d;
			ShortBE() { d.originalValue = 0; }
			ShortBE(const uint16_t val) { changeValue(val); }
			ShortBE(const int16_t val) { changeValue(val); }
			inline void changeValue(const uint16_t);
			inline void changeValue(const int16_t);
			inline uint16_t unsignedValue() const;
			inline int16_t signedValue() const;
		};

		// big-endian 32-bit fixed point value. (16.16)
		class FixedPointBE
		{
		public:
			union _d {
				struct _byte{
					char hi;
					unsigned char lo;
				}byte;
				int16_t value;
			}integer;
			union _d2 {
				struct _byte{
					unsigned char hi;
					unsigned char lo;
				}byte;
				uint16_t value;
			}decimal;
			FixedPointBE() { integer.value=0; decimal.value=0; }
			FixedPointBE(const float val) { changeValue(val); }
			inline void changeValue(const float);
			inline float value() const;
		};
		/// big-endian 32-bit float.
		class FloatBE
		{
		public:
			union _d {
				struct _byte {
					unsigned char hihi;
					unsigned char hilo;
					unsigned char lohi;
					unsigned char lolo;
				}byte;
				float originalValue;
			}d;
			FloatBE() { d.originalValue = 0; }
			FloatBE(const float val) { changeValue(val); }
			inline void changeValue(const float);
			inline float value() const;
		};
		/// big-endian 64-bit float.
		class DoubleBE
		{
		public:
			union _d {
				struct _byte {
					unsigned char hihihi;
					unsigned char hihilo;
					unsigned char hilohi;
					unsigned char hilolo;
					unsigned char lohihi;
					unsigned char lohilo;
					unsigned char lolohi;
					unsigned char lololo;
				}byte;
				double originalValue;
			}d;
			DoubleBE() { d.originalValue = 0; }
			DoubleBE(const double val) { changeValue(val); }
			inline void changeValue(const double);
			inline double value() const;
		};
		/// big-endian 80-bit IEEE float.
		class Extended
		{
		public:
			char bytes[10];
			Extended() { changeValue(0.0); }
			Extended(const double val) { changeValue(val); }
			inline void changeValue(const double);
			inline double value() const;
		};

		/// little-endian 64-bit integer.
		class Long64LE
		{
		public:
			union _d{
				struct _byte{
					unsigned char lololo;
					unsigned char lolohi;
					unsigned char lohilo;
					unsigned char lohihi;
					unsigned char hilolo;
					unsigned char hilohi;
					unsigned char hihilo;
					unsigned char hihihi;
				}byte;
				uint64_t originalValue;
			}d;
			Long64LE() { d.originalValue = 0; }
			Long64LE(const uint64_t val) { changeValue(val); }
			Long64LE(const int64_t val) { changeValue(val); }
			inline void changeValue(const uint64_t);
			inline void changeValue(const int64_t);
			inline uint64_t unsignedValue() const;
			inline int64_t signedValue() const;
		};

		/// little-endian 32-bit integer.
		class LongLE
		{
		public:
			union _d{
				struct _byte{
					unsigned char lolo;
					unsigned char lohi;
					unsigned char hilo;
					unsigned char hihi;
				}byte;
				uint32_t originalValue;
			}d;
			LongLE() { d.originalValue = 0; }
			LongLE(const uint32_t val) { changeValue(val); }
			LongLE(const int32_t val) { changeValue(val); }
			inline void changeValue(const uint32_t);
			inline void changeValue(const int32_t);
			inline uint32_t unsignedValue() const;
			inline int32_t signedValue() const;
		};

		/// little-endian 24-bit integer.
		class Long24LE
		{
		public:
			struct _byte{
				unsigned char lolo;
				unsigned char lohi;
				unsigned char hi;
			}byte;
			Long24LE() { byte.hi = 0; byte.lohi = 0; byte.lolo = 0; }
			Long24LE(const uint32_t val) { changeValue(val); }
			Long24LE(const int32_t val) { changeValue(val); }
			inline void changeValue(const uint32_t);
			inline void changeValue(const int32_t);
			inline uint32_t unsignedValue() const;
			inline int32_t signedValue() const;
		};

		/// little-endian 16-bit integer.
		class ShortLE
		{
		public:
			union _d {
				struct _byte{
					unsigned char lo;
					unsigned char hi;
				}byte;
				uint16_t originalValue;
			}d;
			ShortLE() { d.originalValue = 0; }
			ShortLE(const uint16_t val) { changeValue(val); }
			ShortLE(const int16_t val) { changeValue(val); }
			inline void changeValue(const uint16_t);
			inline void changeValue(const int16_t);
			inline uint16_t unsignedValue() const;
			inline int16_t signedValue() const;
		};
		/// little-endian 32-bit float.
		class FloatLE
		{
		public:
			union _d {
				struct _byte {
					unsigned char lolo;
					unsigned char lohi;
					unsigned char hilo;
					unsigned char hihi;
				}byte;
				float originalValue;
			}d;
			FloatLE() { d.originalValue = 0; }
			FloatLE(const float val) { changeValue(val); }
			inline void changeValue(const float);
			inline float value() const;
		};
		/// little-endian 64-bit float.
		class DoubleLE
		{
		public:
			union _d {
				struct _byte {
					unsigned char lololo;
					unsigned char lolohi;
					unsigned char lohilo;
					unsigned char lohihi;
					unsigned char hilolo;
					unsigned char hilohi;
					unsigned char hihilo;
					unsigned char hihihi;
				}byte;
				double originalValue;
			}d;
			DoubleLE() { d.originalValue = 0; }
			DoubleLE(const double val) { changeValue(val); }
			inline void changeValue(const double);
			inline double value() const;
		};


		//
		// Inline code
		//////////////////////////////////////
		void Long64BE::changeValue(const uint64_t val) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			d.byte.hihihi=  val >> 56;
			d.byte.hihilo= (val >> 48) &0xFF;
			d.byte.hilohi= (val >> 40) &0xFF;
			d.byte.hilolo= (val >> 32) &0xFF;
			d.byte.lohihi= (val >> 24) &0xFF;
			d.byte.lohilo= (val >> 16) &0xFF;
			d.byte.lolohi= (val >> 8) &0xFF;
			d.byte.lololo= (val) &0xFF;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			d.originalValue = val;
#endif
		}
		void Long64BE::changeValue(const int64_t val) {
			changeValue(static_cast<uint64_t>(val));
		}

		uint64_t Long64BE::unsignedValue() const {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			return (static_cast<uint64_t>(d.byte.hihihi) << 56) | (static_cast<uint64_t>(d.byte.hihilo) << 48)
				| (static_cast<uint64_t>(d.byte.hilohi) << 40) | (static_cast<uint64_t>(d.byte.hilolo) << 32)
				| (d.byte.lohihi << 24) | (d.byte.lohilo << 16) | (d.byte.lolohi << 8) | (d.byte.lololo);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			return d.originalValue;
#endif
		}
		int64_t Long64BE::signedValue() const {
			return static_cast<int64_t>(unsignedValue());
		}

void LongBE::changeValue(const uint32_t val) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			d.byte.hihi = (val >> 24)&0xFF;
			d.byte.hilo = (val >> 16)&0xFF;
			d.byte.lohi = (val >> 8)&0xFF;
			d.byte.lolo = val&0xFF;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			d.originalValue = val;
#endif
		}
		void LongBE::changeValue(const int32_t val) {
			changeValue(static_cast<uint32_t>(val));
		}

		uint32_t LongBE::unsignedValue() const {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			return (d.byte.hihi << 24) | (d.byte.hilo << 16) | (d.byte.lohi << 8) | d.byte.lolo;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			return d.originalValue;
#endif
		}
		int32_t LongBE::signedValue() const {
			return static_cast<int32_t>(unsignedValue());
		}

		void Long24BE::changeValue(const uint32_t val) {
			byte.hi = (val >> 16)&0xFF;
			byte.lohi = (val >> 8)&0xFF;
			byte.lolo = val&0xFF;
		}
		void Long24BE::changeValue(const int32_t val) {
			changeValue(static_cast<uint32_t>(val));
		}

		uint32_t Long24BE::unsignedValue() const {
			return (byte.hi << 16) | (byte.lohi << 8) | byte.lolo;
		}
		int32_t Long24BE::signedValue() const {
			return static_cast<int32_t>(unsignedValue());
		}


		void ShortBE::changeValue(const uint16_t val) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			d.byte.hi = (val >> 8)&0xFF;
			d.byte.lo = val&0xFF;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			d.originalValue = val;
#endif
		}
		void ShortBE::changeValue(const int16_t val) {
			changeValue(static_cast<uint16_t>(val));
		}
		uint16_t ShortBE::unsignedValue() const {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			return (d.byte.hi << 8) | d.byte.lo;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			return d.originalValue;
#endif
		}
		int16_t ShortBE::signedValue() const {
			return static_cast<int16_t>(unsignedValue());
		}

void FixedPointBE::changeValue(const float val) {
			float floor = std::floor(val);
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			int newint = static_cast<short>(std::floor(val));
			int newdec = static_cast<unsigned short>((val-floor)*65536);
			integer.byte.hi = newint >> 8;
			integer.byte.lo = newint &0xFF;
			decimal.byte.hi = newdec >> 8;
			decimal.byte.lo = newdec &0xFF;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			integer.value = static_cast<short>(std::floor(val));
			decimal.value = static_cast<unsigned short>((val-floor)*65536);
#endif
		}
		float FixedPointBE::value() const {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			return static_cast<float>((integer.byte.hi << 8) | integer.byte.lo) + ((decimal.byte.hi << 8) | decimal.byte.lo)*0.0000152587890625f;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			return static_cast<float>(integer.value) + decimal.value*0.0000152587890625f;
#endif
		}

		void FloatBE::changeValue(const float valf) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			const uint32_t val = *reinterpret_cast<const uint32_t*>(&valf);
			d.byte.hihi=  val >> 24;
			d.byte.hilo= (val >> 16) &0xFF;
			d.byte.lohi= (val >> 8) &0xFF;
			d.byte.lolo= (val) &0xFF;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			d.originalValue=valf;
#endif
		}
		float FloatBE::value() const {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			uint32_t val = (d.byte.hihi << 24) | (d.byte.hilo << 16) | (d.byte.lohi << 8) | (d.byte.lolo);
			return *reinterpret_cast<float*>(&val);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			return d.originalValue;
#endif
		}
		void DoubleBE::changeValue(const double vald) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			const uint64_t val = *reinterpret_cast<const uint64_t*>(&vald);
			d.byte.hihihi=  val >> 56;
			d.byte.hihilo= (val >> 48) &0xFF;
			d.byte.hilohi= (val >> 40) &0xFF;
			d.byte.hilolo= (val >> 32) &0xFF;
			d.byte.lohihi= (val >> 24) &0xFF;
			d.byte.lohilo= (val >> 16) &0xFF;
			d.byte.lolohi= (val >> 8) &0xFF;
			d.byte.lololo= (val) &0xFF;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			d.originalValue=vald;
#endif
		}
		double DoubleBE::value() const {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			uint64_t val = (static_cast<uint64_t>(d.byte.hihihi) << 56) | (static_cast<uint64_t>(d.byte.hihilo) << 48)
				| (static_cast<uint64_t>(d.byte.hilohi) << 40) | (static_cast<uint64_t>(d.byte.hilolo) << 32)
				| (d.byte.lohihi << 24) | (d.byte.lohilo << 16) | (d.byte.lolohi << 8) | (d.byte.lololo);
			return *reinterpret_cast<double*>(&val);
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			return d.originalValue;
#endif
		}

		void Extended::changeValue(double num) {
			// http://www.onicos.com/staff/iz/formats/ieee.c  (c) Apple 
			int sign;
			int expon;
			double fMant, fsMant;
			unsigned long hiMant, loMant;

			if (num < 0) {
				sign = 0x8000;
				num *= -1;
			} else {
				sign = 0;
			}

			if (num == 0) {
				expon = 0; hiMant = 0; loMant = 0;
			}
			else {
				fMant = frexp(num, &expon);
				if ((expon > 16384) || !(fMant < 1)) {    /* Infinity or NaN */
					expon = sign|0x7FFF; hiMant = 0; loMant = 0; /* infinity */
				}
				else {    /* Finite */
					expon += 16382;
					if (expon < 0) {    /* denormalized */
						fMant = ldexp(fMant, expon);
						expon = 0;
					}
					expon |= sign;
					fMant = ldexp(fMant, 32);          
					fsMant = floor(fMant); 
					hiMant = static_cast<unsigned long>(fsMant);
					fMant = ldexp(fMant - fsMant, 32); 
					fsMant = floor(fMant); 
					loMant = static_cast<unsigned long>(fsMant);
				}
			}
		    
			bytes[0] = expon >> 8;
			bytes[1] = expon;
			bytes[2] = hiMant >> 24;
			bytes[3] = hiMant >> 16;
			bytes[4] = hiMant >> 8;
			bytes[5] = hiMant;
			bytes[6] = loMant >> 24;
			bytes[7] = loMant >> 16;
			bytes[8] = loMant >> 8;
			bytes[9] = loMant;
		}

		double Extended::value() const {
			// http://www.onicos.com/staff/iz/formats/ieee.c  (c) Apple 
			double    f;
			int    expon;
			unsigned long hiMant, loMant;
		    
			expon   = ((bytes[0] & 0x7F) << 8) | (bytes[1] & 0xFF);
			hiMant  =    ((unsigned long)(bytes[2] & 0xFF) << 24)
					|    ((unsigned long)(bytes[3] & 0xFF) << 16)
					|    ((unsigned long)(bytes[4] & 0xFF) << 8)
					|    ((unsigned long)(bytes[5] & 0xFF));
			loMant  =    ((unsigned long)(bytes[6] & 0xFF) << 24)
					|    ((unsigned long)(bytes[7] & 0xFF) << 16)
					|    ((unsigned long)(bytes[8] & 0xFF) << 8)
					|    ((unsigned long)(bytes[9] & 0xFF));

			if (expon == 0 && hiMant == 0 && loMant == 0) {
				f = 0;
			}
			else if (expon == 0x7FFF) {    /* Infinity or NaN */
				f = HUGE_VAL;
			}
			else {
				expon -= 16383;
				f  = ldexp(static_cast<float>(hiMant), expon-=31);
				f += ldexp(static_cast<float>(loMant), expon-=32);
			}

			if (bytes[0] & 0x80)
				return -f;
			else
				return f;
		}


		void Long64LE::changeValue(const uint64_t val) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			d.originalValue = val;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			d.byte.hihihi=  val >> 56;
			d.byte.hihilo= (val >> 48) &0xFF;
			d.byte.hilohi= (val >> 40) &0xFF;
			d.byte.hilolo= (val >> 32) &0xFF;
			d.byte.lohihi= (val >> 24) &0xFF;
			d.byte.lohilo= (val >> 16) &0xFF;
			d.byte.lolohi= (val >> 8) &0xFF;
			d.byte.lololo= (val) &0xFF;
#endif
		}
		void Long64LE::changeValue(const int64_t val) {
			changeValue(static_cast<uint64_t>(val));
		}

		uint64_t Long64LE::unsignedValue() const {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			return d.originalValue;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			return (static_cast<uint64_t>(d.byte.hihihi) << 56) | (static_cast<uint64_t>(d.byte.hihilo) << 48)
				| (static_cast<uint64_t>(d.byte.hilohi) << 40) | (static_cast<uint64_t>(d.byte.hilolo) << 32)
				| (d.byte.lohihi << 24) | (d.byte.lohilo << 16) | (d.byte.lolohi << 8) | (d.byte.lololo);
#endif
		}
		int64_t Long64LE::signedValue() const {
			return static_cast<int64_t>(unsignedValue());
		}

		void LongLE::changeValue(const uint32_t val) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			d.originalValue = val;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			d.byte.hihi = (val >> 24)&0xFF;
			d.byte.hilo = (val >> 16)&0xFF;
			d.byte.lohi = (val >> 8)&0xFF;
			d.byte.lolo = val&0xFF;
#endif
		}
		void LongLE::changeValue(const int32_t val) {
			changeValue(static_cast<uint32_t>(val));
		}

		uint32_t LongLE::unsignedValue() const {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			return d.originalValue;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			return (d.byte.hihi << 24) | (d.byte.hilo << 16) | (d.byte.lohi << 8) | d.byte.lolo;
#endif
		}
		int32_t LongLE::signedValue() const {
			return static_cast<int32_t>(unsignedValue());
		}

		void Long24LE::changeValue(const uint32_t val) {
			byte.hi = (val >> 16)&0xFF;
			byte.lohi = (val >> 8)&0xFF;
			byte.lolo = val&0xFF;
		}
		void Long24LE::changeValue(const int32_t val) {
			changeValue(static_cast<uint32_t>(val));
		}

		uint32_t Long24LE::unsignedValue() const {
			return (byte.hi << 16) | (byte.lohi << 8) | byte.lolo;
		}
		int32_t Long24LE::signedValue() const {
			return static_cast<int32_t>(unsignedValue());
		}

		void ShortLE::changeValue(const uint16_t val) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			d.originalValue = val;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			d.byte.hi = (val >> 8)&0xFF;
			d.byte.lo = val&0xFF;
#endif
		}
		void ShortLE::changeValue(const int16_t val) {
			changeValue(static_cast<uint16_t>(val));
		}
		uint16_t ShortLE::unsignedValue() const {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			return d.originalValue;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			return (d.byte.hi << 8) | d.byte.lo;
#endif
		}
		int16_t ShortLE::signedValue() const {
			return static_cast<int16_t>(unsignedValue());
		}
void FloatLE::changeValue(const float valf) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			d.originalValue=valf;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			const uint32_t val = *reinterpret_cast<const uint32_t*>(&valf);
			d.byte.hihi=  val >> 24;
			d.byte.hilo= (val >> 16) &0xFF;
			d.byte.lohi= (val >> 8) &0xFF;
			d.byte.lolo= (val) &0xFF;
#endif
		}
		float FloatLE::value() const {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			return d.originalValue;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			uint32_t val = (d.byte.hihi << 24) | (d.byte.hilo << 16) | (d.byte.lohi << 8) | (d.byte.lolo);
			return *reinterpret_cast<float*>(&val);
#endif
		}
		
		void DoubleLE::changeValue(const double vald) {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			d.originalValue=vald;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			const uint64_t val = *reinterpret_cast<const uint64_t*>(&vald);
			d.byte.hihihi=  val >> 56;
			d.byte.hihilo= (val >> 48) &0xFF;
			d.byte.hilohi= (val >> 40) &0xFF;
			d.byte.hilolo= (val >> 32) &0xFF;
			d.byte.lohihi= (val >> 24) &0xFF;
			d.byte.lohilo= (val >> 16) &0xFF;
			d.byte.lolohi= (val >> 8) &0xFF;
			d.byte.lololo= (val) &0xFF;
#endif
		}
		double DoubleLE::value() const {
#if defined DIVERSALIS__CPU__ENDIAN__LITTLE
			return d.originalValue;
#elif defined DIVERSALIS__CPU__ENDIAN__BIG
			uint64_t val = (static_cast<uint64_t>(d.byte.hihihi) << 56) | (static_cast<uint64_t>(d.byte.hihilo) << 48)
				| (static_cast<uint64_t>(d.byte.hilohi) << 40) | (static_cast<uint64_t>(d.byte.hilolo) << 32)
				| (d.byte.lohihi << 24) | (d.byte.lohilo << 16) | (d.byte.lolohi << 8) | (d.byte.lololo);
			return *reinterpret_cast<double*>(&val);
#endif
		}

}}
