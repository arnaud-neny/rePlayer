// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2008-2013 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#ifndef PSYCLE__HELPERS__RING_BUFFER__INCLUDED
#define PSYCLE__HELPERS__RING_BUFFER__INCLUDED
#pragma once
#include <universalis.hpp>
#include <universalis/cpu/memory_barriers.hpp>
#include <cstddef>
#include <cassert>
#include <algorithm>
#if __cplusplus >= 201103L
	#include <atomic>
#endif
namespace psycle { namespace helpers { namespace ring_buffers {

// see boost circular buffer    http://www.boost.org/doc/libs/1_46_1/libs/circular_buffer/doc/circular_buffer.html
// see portaudio memory barrier http://portaudio.com/trac/browser/portaudio/trunk/src/common/pa_memorybarrier.h
// see portaudio ring buffer    http://portaudio.com/trac/browser/portaudio/trunk/src/common/pa_ringbuffer.c

/******************************************************************************************/
/// lock-free ring buffer concept.
///
/// This ring buffer is useful for push-based multi-threaded processing.
/// It helps optimising thread-synchronisation by using lock-free atomic primitives
/// (normally implemented with specific CPU instructions),
/// instead of relying on OS-based synchronisation facilities,
/// which incur the overhead of context switching and re-scheduling delay.
///
/// This class does not store the actual buffer data, but only a read position and a write position.
/// This separation of concerns makes it possible to use this class with any kind of data access interface for the buffer.
#if 0
	class ring {
		public:
			ring(std::size_t size) { assert("power of 2" && !(size & (size - 1))); }
			std::size_t size() const;
			void commit_read(std::size_t count);
			void commit_write(std::size_t count);
			void avail_for_read(std::size_t max, std::size_t & begin, std::size_t & size1, std::size_t & size2) const;
			void avail_for_write(std::size_t max, std::size_t & begin, std::size_t & size1, std::size_t & size2) const;
	};
#endif

/******************************************************************************************/
/// ring buffer using c++11 atomic types
#if __cplusplus >= 201103L
class ring_buffer_with_atomic_stdlib {
	std::size_t const size_, size_mask_, size_mask2_;
	std::atomic<std::size_t> read_, write_;
	
	public:
		ring_buffer_with_atomic_stdlib(std::size_t size)
		: size_(size), size_mask_(size - 1), size_mask2_(size * 2 - 1), read_(0), write_(0) {
			assert("power of 2" && !(size & size_mask_));
		}
		
		std::size_t size() const { return size_; }
		
		void commit_read(std::size_t count) {
			auto const read = read_.load(std::memory_order_relaxed);
			read_.store((read + count) & size_mask2_, std::memory_order_release);
		}

		void commit_write(std::size_t count) {
			auto const write = write_.load(std::memory_order_relaxed);
			write_.store((write + count) & size_mask2_, std::memory_order_release);
		}

		void avail_for_read(
			std::size_t max,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF begin,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF size1,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF size2
		) const {
			auto const read = read_.load(std::memory_order_relaxed);
			begin = read & size_mask_;
			auto const write = write_.load(std::memory_order_acquire);
			auto const avail = std::min(max, (write - read) & size_mask2_);
			if(begin + avail > size_) {
				size1 = size_ - begin;
				size2 = avail - size1;
			} else {
				size1 = avail;
				size2 = 0;
			}
		}

		void avail_for_write(
			std::size_t max,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF begin,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF size1,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF size2
		) const {
			auto const write = write_.load(std::memory_order_relaxed);
			begin = write & size_mask_;
			auto const read = read_.load(std::memory_order_acquire);
			auto const avail = std::min(max, size_ - ((write - read) & size_mask2_));
			if(begin + avail > size_) {
				size1 = size_ - begin;
				size2 = avail - size1;
			} else {
				size1 = avail;
				size2 = 0;
			}
		}
};
#endif // __cplusplus >= 201103L

/******************************************************************************************/
/// ring buffer using explict memory barriers
class ring_buffer_with_explicit_memory_barriers {
	std::size_t const size_, size_mask_, size_mask2_;
	std::size_t read_, write_;

	public:
		ring_buffer_with_explicit_memory_barriers(std::size_t size)
		: size_(size), size_mask_(size - 1), size_mask2_(size * 2 - 1), read_(), write_() {
			assert("power of 2" && !(size & size_mask_));
		}
		
		std::size_t size() const { return size_; }
		
		void commit_read(std::size_t count) {
			universalis::cpu::memory_barriers::write();
			read_ = (read_ + count) & size_mask2_;
		}

		void commit_write(std::size_t count) {
			universalis::cpu::memory_barriers::write();
			write_ = (write_ + count) & size_mask2_;
		}

		void avail_for_read(
			std::size_t max,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF begin,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF size1,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF size2
		) const {
			begin = read_ & size_mask_;
			universalis::cpu::memory_barriers::read();
			std::size_t const avail = std::min(max, (write_ - read_) & size_mask2_);
			if(begin + avail > size_) {
				size1 = size_ - begin;
				size2 = avail - size1;
			} else {
				size1 = avail;
				size2 = 0;
			}
		}

		void avail_for_write(
			std::size_t max,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF begin,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF size1,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF size2
		) const {
			begin = write_ & size_mask_;
			universalis::cpu::memory_barriers::read();
			std::size_t const avail = std::min(max, size_ - ((write_ - read_) & size_mask2_));
			if(begin + avail > size_) {
				size1 = size_ - begin;
				size2 = avail - size1;
			} else {
				size1 = avail;
				size2 = 0;
			}
		}
};

/******************************************************************************************/
/// ring buffer using compiler volatile
/// WARNING: It doesn't work reliably on cpu archs with weak memory ordering, and it also suffers from undeterministic wakeups which make it slower.
class ring_buffer_with_compiler_volatile {
	std::size_t const size_, size_mask_, size_mask2_;
	std::size_t volatile read_, write_;
	
	public:
		ring_buffer_with_compiler_volatile(std::size_t size)
		: size_(size), size_mask_(size - 1), size_mask2_(size * 2 - 1), read_(), write_() {
			assert("power of 2" && !(size & size_mask_));
		}
		
		std::size_t size() const { return size_; }
		
		void commit_read(std::size_t count) {
			read_ = (read_ + count) & size_mask2_;
		}

		void commit_write(std::size_t count) {
			write_ = (write_ + count) & size_mask2_;
		}

		void avail_for_read(
			std::size_t max,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF begin,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF size1,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF size2
		) const {
			std::size_t const read = read_;
			std::size_t const write = write_;
			begin = read & size_mask_;
			std::size_t const avail = std::min(max, (write - read) & size_mask2_);
			if(begin + avail > size_) {
				size1 = size_ - begin;
				size2 = avail - size1;
			} else {
				size1 = avail;
				size2 = 0;
			}
		}

		void avail_for_write(
			std::size_t max,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF begin,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF size1,
			std::size_t & UNIVERSALIS__COMPILER__RESTRICT_REF size2
		) const {
			std::size_t const write = write_;
			begin = write & size_mask_;
			std::size_t const avail = std::min(max, size_ - ((write - read_) & size_mask2_));
			if(begin + avail > size_) {
				size1 = size_ - begin;
				size2 = avail - size1;
			} else {
				size1 = avail;
				size2 = 0;
			}
		}
};

}}}

/******************************************************************************************/
#if defined BOOST_AUTO_TEST_CASE && __cplusplus >= 201103L
	#include <universalis/compiler/typenameof.hpp>
	#include <universalis/os/sched.hpp>
	#include <thread>
	#include <utility>
	#include <random>
	#include <functional>
	#include <chrono>
	#include <sstream>
	namespace psycle { namespace helpers { namespace ring_buffers { namespace test {
		template<typename Ring_Buffer, typename Rand_Gen>
		void writer_loop(std::size_t buf[], Ring_Buffer & ring_buffer, Rand_Gen & rand_gen, std::size_t elements_to_process) {
			std::size_t counter = 0;
			while(counter < elements_to_process) {
				std::size_t begin, size1, size2;
				ring_buffer.avail_for_write(rand_gen(), begin, size1, size2);
				if(size1) {
					for(std::size_t i = begin, e = begin + size1; i < e; ++i) buf[i] = ++counter;
					if(size2) {
						for(std::size_t i = 0; i < size2; ++i) buf[i] = ++counter;
						ring_buffer.commit_write(size1 + size2);
					} else ring_buffer.commit_write(size1);
				}
			}
		}

		template<typename Ring_Buffer, typename Rand_Gen>
		void reader_loop(std::size_t buf[], Ring_Buffer & ring_buffer, Rand_Gen & rand_gen, std::size_t elements_to_process) {
			std::size_t counter = 0, errors = 0;
			while(counter < elements_to_process) {
				std::size_t begin, size1, size2;
				ring_buffer.avail_for_read(rand_gen(), begin, size1, size2);
				if(size1) {
					for(std::size_t i = begin, e = begin + size1; i < e; ++i) errors += buf[i] != ++counter;
					if(size2) {
						for(std::size_t i = 0; i < size2; ++i) errors += buf[i] != ++counter;
						ring_buffer.commit_read(size1 + size2);
					} else ring_buffer.commit_read(size1);
				}
			}
			BOOST_CHECK(!errors);
		}

		template<typename Ring_Buffer>
		void test(
			std::random_device::result_type writer_rand_gen_seed,
			std::random_device::result_type reader_rand_gen_seed
		) {
			std::size_t const size = 256;
			std::size_t const cpu_avail =
				// note: std::thread::hardware_concurrency() is not impacted by the process' scheduler affinity mask.
				universalis::os::sched::process().affinity_mask().active_count();
			std::size_t const elements_to_process =
				// note: On single-cpu system, the lock-free ring buffer is very slow
				//       because each thread completes its full quantum before the scheduler decides to switch (10 to 15 ms)
				//       So we shorten it if there's only one cpu available.
				//       std::thread::hardware_concurrency() is not impacted by the taskset command.
				//       What we want is the process' scheduler affinity mask.
				cpu_avail == 1 ?
				100 * 1000 :
				100 * 1000 * 1000;
			std::size_t buf[size];
			Ring_Buffer ring_buffer(size);
			typedef std::mt19937 rand_gen_engine;
			typedef std::uniform_int_distribution<std::size_t> rand_gen_distrib;
			std::size_t const rand_gen_dist_lower = ring_buffer.size() / 4;
			std::size_t const rand_gen_dist_upper = ring_buffer.size() / 2;
			auto writer_rand_gen = std::bind(rand_gen_distrib(rand_gen_dist_lower, rand_gen_dist_upper), rand_gen_engine(reader_rand_gen_seed));
			auto reader_rand_gen = std::bind(rand_gen_distrib(rand_gen_dist_lower, rand_gen_dist_upper), rand_gen_engine(writer_rand_gen_seed));
			{
				std::ostringstream s;
				s <<
					"____________________________\n\n"
					"ring typename: " << universalis::compiler::typenameof(ring_buffer) << "\n"
					"ring buffer size: " << size << "\n"
					"number of cpu avail: " << cpu_avail << "\n"
					"elements to process: " << double(elements_to_process) << "\n"
					"rand gen distrib range: " << rand_gen_dist_lower << ' ' << rand_gen_dist_upper << "\n"
					"writer rand gen typename: " << universalis::compiler::typenameof<decltype(writer_rand_gen)>() << "\n"
					"reader rand gen typename: " << universalis::compiler::typenameof<decltype(reader_rand_gen)>() << "\n"
					"writer rand gen seed: " << writer_rand_gen_seed << "\n"
					"reader rand gen seed: " << reader_rand_gen_seed;
				BOOST_MESSAGE(s.str());
			}
			BOOST_MESSAGE("running ... ");
			typedef std::chrono::high_resolution_clock clock;
			auto const t0 = clock::now();
			std::thread writer_thread(writer_loop<Ring_Buffer, decltype(writer_rand_gen)>, buf, std::ref(ring_buffer), std::ref(writer_rand_gen), elements_to_process);
			std::thread reader_thread(reader_loop<Ring_Buffer, decltype(reader_rand_gen)>, buf, std::ref(ring_buffer), std::ref(reader_rand_gen), elements_to_process);
			writer_thread.join();
			reader_thread.join();
			auto const t1 = clock::now();
			BOOST_MESSAGE("done.");
			auto const duration = std::chrono::nanoseconds(t1 - t0).count() * 1e-9;
			{
				std::ostringstream s;
				s <<
					"duration: " << duration << " seconds\n"
					"troughput: " << elements_to_process / duration << " elements/second\n";
				BOOST_MESSAGE(s.str());
			}
		}

		BOOST_AUTO_TEST_CASE(ring_buffer_test) {
			std::random_device rand_dev;
			auto const writer_rand_gen_seed = rand_dev();
			auto const reader_rand_gen_seed = rand_dev();
			test<ring_buffer_with_atomic_stdlib>(writer_rand_gen_seed, reader_rand_gen_seed);
			test<ring_buffer_with_explicit_memory_barriers>(writer_rand_gen_seed, reader_rand_gen_seed);
			test<ring_buffer_with_compiler_volatile>(writer_rand_gen_seed, reader_rand_gen_seed);
		}
	}}}}
#endif

#endif
