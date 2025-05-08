///\file
///\brief various signal processing utility functions and classes, psycle::helpers::dsp::Cubic amongst others.
#pragma once
#include "math/erase_all_nans_infinities_and_denormals.hpp"
#include <cmath>
// #include <soxr.h>

#if defined BOOST_AUTO_TEST_CASE
	#include <universalis/os/aligned_alloc.hpp>
	#include <universalis/os/clocks.hpp>
	#include <sstream>
#endif
namespace psycle { namespace helpers { /** various signal processing utility functions. */ namespace dsp {

/// cubic and linear tables resolution. Note: higher than 10 does not improve the result.
#define CUBIC_RESOLUTION_LOG 10
static const int CUBIC_RESOLUTION = 1<<CUBIC_RESOLUTION_LOG;

// either or both of resolution and zeros can be fine-tuned to find a tolerable compromise between quality and memory/cpu usage
// resolution lowers the aliasing noise above the filter point. Zeros make the filter cutoff sharper (and determine the amount of pre and post samples needed). 
/// sinc table values per zero crossing
#define SINC_RESOLUTION_LOG 14
/// sinc table zero crossings (per side) -- too low and it aliases, too high uses lots of cpu. Recommended to be even.
//  Note: If increasing this value, also adapt requiredPreSamples(), requiredPostSamples() ,
//  XMSampler::WaveDataController::RefillBuffer() and XMSampler::WaveDataController::PreWork()
static const int SINC_ZEROS = 16;
/// IF SINC_ZEROS is power of two, define OPTIMIZED_RES_SHIFT to allow the algorithm to do shift instead of multiply
#define OPTIMIZED_RES_SHIFT 4
#define USE_SINC_DELTA SINC_RESOLUTION_LOG < 10
static const int SINC_RESOLUTION = 1<<SINC_RESOLUTION_LOG;
static const int SINC_TABLESIZE = SINC_RESOLUTION * SINC_ZEROS;

static const int SOXR_RESAMPLE_POINTS = 10;

using namespace universalis::stdlib;
	/****************************************************************************/

	/// sample interpolator.
	class resampler {
		public:
			/// interpolator work function types

			// data = input signal to be resampled already pointing at the offset indicated by offset.
			// offset = sample offset (integer) [info to avoid go out of bounds on sample reading ]
			// res = decimal part of the offset (between point y0 and y1) to get, as a 32bit int.
			// length = sample length [info to avoid go out of bounds on sample reading ]
			// resampler_data = resampler specific data. Needed for sinc and sox resamplers. 
			// Obtain it by calling at GetResamplerData(speed), and call at UpdateSpeed(speed) when the same sample changes speed.
			// when done with the sample, call DispodeResamplerData(resampler_data)
			typedef float (*work_func_type)(int16_t const * data, uint64_t offset, uint32_t res, uint64_t length, void* resampler_data);
			//Version without checks in data limits. Use only when you guarantee that data has enough samples for the resampling algorithm.
			// data = input signal to be resampled already pointing at the integer offset.
			// res = decimal part of the offset (between point y0 and y1) to get, as a 32bit int.
			typedef float (*work_unchecked_func_type)(int16_t const * data, uint32_t res, void* resampler_data);
			//Float version without checks in data limits. Use only when you guarantee that data has enough samples for the resampling algorithm.
			// data = input signal to be resampled already pointing at the integer offset.
			// res = decimal part of the offset (between point y0 and y1) to get, as a 32bit int.
			typedef float (*work_float_unchecked_func_type)(float const * data, uint32_t res, void* resampler_data);
			//Float version with limits
			// data = input signal to be resampled (pointing at the start of data)
			// offset = exact sample offset including decimal part of the offset (between point y0 and y1) to get (i.e. 3.41).
			// length = sample length [info to avoid go out of bounds on sample reading ]
			// loopBeg pointer to the start of the loop (if no loop, use same pointer than data)
			// loopEnd pointer to the end of the loop ( i.e. if 10 positions, then (array+9)) (if no loop use data[length-1])
			typedef float (*work_float_func_type)(float const * data, float offset, uint64_t length, void* resampler_data, float const * loopBeg, float const * loopEnd);
			
	/// sample interpolator kinds.
			struct quality { enum type {
				zero_order,
				linear,
				spline,
				sinc,
				soxr
			};};

			/// constructor
			resampler() : work(zoh), work_float(zoh_float), quality_(quality::zero_order) {}
			virtual ~resampler() {}

			/// sets the kind of interpolation.
			virtual void quality(quality::type) = 0;
			virtual quality::type quality() const = 0;

			virtual void * GetResamplerData() const = 0;
			virtual void UpdateSpeed(void * resampler_data, double speed) const = 0;
			virtual void DisposeResamplerData(void * resampler_data) const = 0;
			virtual int requiredPresamples() const = 0;
			virtual int requiredPostSamples() const = 0;

			/// work function corresponding to the selected kind.
			work_func_type work;
			work_unchecked_func_type work_unchecked;
			work_float_unchecked_func_type work_float_unchecked;
			work_float_func_type work_float;
			
		protected:
			/// kind of interpolation.
			quality::type quality_;
			
			/// interpolation work function which does nothing. Also known as Zero order hold interpolation
			static float zoh(int16_t const * data, uint64_t /*offset*/, uint32_t /*res*/, uint64_t /*length*/, void* /*resampler_data*/) {
				return *data;
			}
			//Version without checks in data limits. Use only when you guarantee that data has enough samples for the resampling algorithm.
			static float zoh_unchecked(int16_t const * data, uint32_t /*res*/, void* /*resampler_data*/) {
				return *data;
			}
			static float zoh_float_unchecked(float const * data, uint32_t /*res*/, void* /*resampler_data*/) {
				return *data;
			}
			//float version
			static float zoh_float(float const * data, float offset, uint64_t /*length*/, void* /*resampler_data*/, float const * /*loopBeg*/, float const * /*loopEnd*/) {
				int iOsc = std::floor(offset);
				return data[iOsc];
			}
	};

	/// cubic sample interpolator.
	class cubic_resampler : public resampler {

		public:
			/// constructor.
			cubic_resampler();
			virtual ~cubic_resampler(){}

			/*override*/ quality::type quality() const { return quality_; }

			/*override*/ void quality(quality::type quality);
			/*override*/ void * GetResamplerData() const;
			/*override*/ void UpdateSpeed(void * resampler_data, double speed) const;
			/*override*/ void DisposeResamplerData(void * resampler_data) const;
			/*override*/ int requiredPresamples() const;
			/*override*/ int requiredPostSamples() const;
			
			//Call it synchronized
			static void testreinit () {initTables();}
			static int windowtype;

		protected:
			struct sinc_data_t {
				bool enabled;
				double fcpi;
				double fcpidivperiodsize;
			};

			static void initTables();

			// static soxr_t new_soxr();

			/// interpolation work function which does linear interpolation.
			static float linear(int16_t const * data, uint64_t offset, uint32_t res, uint64_t length, void* resampler_data);
			static float linear_unchecked(int16_t const * data, uint32_t res, void* resampler_data);
			static float linear_float_unchecked(float const * data, uint32_t res, void* resampler_data);
			static float linear_float(float const * data, float offset, uint64_t length, void* resampler_data, float const * loopBeg, float const * loopEnd);

			/// interpolation work function which does spline interpolation.
			static float spline(int16_t const * data, uint64_t offset, uint32_t res, uint64_t length, void* resampler_data);
			static float spline_unchecked(int16_t const * data, uint32_t res, void* resampler_data);
			static float spline_float_unchecked(float const * data, uint32_t res, void* resampler_data);
			static float spline_float(float const * data, float offset, uint64_t length, void* resampler_data, float const * loopBeg, float const * loopEnd);

			/// Interpolation work function using a windowed sinc.
			static float sinc(int16_t const * data, uint64_t offset, uint32_t res, uint64_t length, void* resampler_data);
			static float sinc_unchecked(int16_t const * data, uint32_t res, void* resampler_data);
			static float sinc_float_unchecked(float const * data, uint32_t res, void* resampler_data);
			inline static float sinc_filtered(int16_t const * data, uint32_t res, int leftExtent, int rightExtent, sinc_data_t* resampler_data);
			inline static float sinc_float_filtered(float const * data, uint32_t res, int leftExtent, int rightExtent, sinc_data_t* resampler_data);
			inline static float sinc_internal(int16_t const * data, uint32_t res, int leftExtent, int rightExtent);
			inline static float sinc_float_internal(float const * data, uint32_t res, int leftExtent, int rightExtent);
			static float sinc_float(float const * data, float offset, uint64_t length, void* resampler_data, float const * loopBeg, float const * loopEnd);

			/// Interpolation using SOXR variable rate resampling.
			static float soxr(int16_t const * data, uint64_t offset, uint32_t res, uint64_t length, void* resampler_data);

		private:
			//table of linear interpolation
			static UNIVERSALIS__COMPILER__DOALIGN(16,float l_table_[CUBIC_RESOLUTION]);

			//tables of cubic interpolation
			static UNIVERSALIS__COMPILER__DOALIGN(16,float cubic_table_[CUBIC_RESOLUTION*4]);

			/// Table of the windowed sinc function. 
			//If using the optimized path, it is reordered for better performance.
			static UNIVERSALIS__COMPILER__DOALIGN(16,float sinc_windowed_table[SINC_TABLESIZE]);
#if USE_SINC_DELTA
			/// Table of the deltas between sinc values.
			/// used to improve sinc when the sinc_table is small
			static UNIVERSALIS__COMPILER__DOALIGN(16,float sinc_delta_[SINC_TABLESIZE]);
#endif
			//Table to use in the filter-aware sinc resampling.
			static UNIVERSALIS__COMPILER__DOALIGN(16,float window_div_x[SINC_TABLESIZE]);
			static bool initialized;
	};

}}}
