// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2012 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include <universalis/detail/project.hpp>
#if __cplusplus >= 201103L
	#include <chrono>
	namespace universalis { namespace stdlib { namespace chrono {
		using std::chrono::nanoseconds;
		using std::chrono::microseconds;
		using std::chrono::milliseconds;
		using std::chrono::seconds;
		using std::chrono::minutes;
		using std::chrono::hours;
		using std::chrono::time_point;
	}}}
#else
	#include <universalis/stdlib/ratio.hpp>
	#include <universalis/stdlib/cstdint.hpp>
	#include <boost/operators.hpp>
	namespace universalis { namespace stdlib { namespace chrono {

		namespace detail {
			template<typename Final, typename Rep, typename Period = ratio<1> >
			class basic_duration
			: private
				boost::equality_comparable<Final,
				boost::less_than_comparable<Final,
				boost::additive<Final,
				boost::multiplicative<Final, Rep
				> > > >
			{
				public:
					typedef Rep rep;
					typedef Period period;
					basic_duration(rep r = 0) : rep_(r) {}
					rep count() const { return rep_; }
					
					Final const static zero() { return Final(); }

					bool operator == (Final const & that) const { return this->rep_ == that.rep_; }
					bool operator < (Final const & that) const { return this->rep_ <  that.rep_; }
					Final & operator += (Final const & that) { this->rep_ += that.rep_; return static_cast<Final&>(*this); }
					Final & operator -= (Final const & that) { this->rep_ -= that.rep_; return static_cast<Final&>(*this); }
					Final operator - () const { Final f(-this->rep_); return f; }
					Final & operator /= (rep const & d) { rep_ /= d; return static_cast<Final&>(*this); }
					Final & operator *= (rep const & m) { rep_ *= m; return static_cast<Final&>(*this); }
				protected:
					typedef basic_duration<Final, Rep, Period> basic_duration_type;
				private:
					rep rep_;
			};
		}

		class nanoseconds : public detail::basic_duration<nanoseconds, int64_t, nano> {
			public:
				nanoseconds(rep ns = 0) : basic_duration_type(ns) {}
		};

		class microseconds : public detail::basic_duration<microseconds, int64_t, micro> {
			public:
				microseconds(rep us = 0) : basic_duration_type(us) {}
				operator nanoseconds() const { nanoseconds ns(count() * (nanoseconds::period::den / period::den)); return ns; }
		};

		class milliseconds : public detail::basic_duration<milliseconds, int64_t, milli> {
			public:
				milliseconds(rep ms = 0) : basic_duration_type(ms) {}
				operator nanoseconds() const { nanoseconds ns(count() * (nanoseconds::period::den / period::den)); return ns; }
				operator microseconds() const { microseconds us(count() * (microseconds::period::den / period::den)); return us; }
		};

		class seconds : public detail::basic_duration<seconds, int64_t> {
			public:
				seconds(rep s = 0) : basic_duration_type(s) {}
				operator nanoseconds() const { nanoseconds ns(count() * nanoseconds::period::den); return ns; }
				operator microseconds() const { microseconds us(count() * microseconds::period::den); return us; }
				operator milliseconds() const { milliseconds ms(count() * milliseconds::period::den); return ms; }
		};

		class minutes : public detail::basic_duration<minutes, int, ratio<60> > {
			public:
				minutes(rep m = 0) : basic_duration_type(m) {}
				operator nanoseconds() const { nanoseconds ns(count() * nanoseconds::period::den * period::num); return ns; }
				operator microseconds() const { microseconds us(count() * microseconds::period::den * period::num); return us; }
				operator milliseconds() const { milliseconds ms(count() * milliseconds::period::den * period::num); return ms; }
				operator seconds() const { seconds s(count() * period::num); return s; }
		};

		class hours : public detail::basic_duration<hours, int, ratio<3600> > {
			public:
				hours(rep h = 0) : basic_duration_type(h) {}
				operator nanoseconds() const { nanoseconds ns(count() * nanoseconds::period::den * period::num); return ns; }
				operator microseconds() const { microseconds us(count() * microseconds::period::den * period::num); return us; }
				operator milliseconds() const { milliseconds ms(count() * milliseconds::period::den * period::num); return ms; }
				operator seconds() const { seconds s(count() * period::num); return s; }
				operator minutes() const { minutes m(count() * (period::num / minutes::period::num)); return m; }
		};
		
		template<typename Clock, typename Duration = typename Clock::duration>
		struct time_point
		: private
			boost::equality_comparable<time_point<Clock, Duration>,
			boost::less_than_comparable<time_point<Clock, Duration>
			//boost::additive<Duration,
			> >
		{
			typedef Clock clock;
			typedef Duration duration;
			typedef typename duration::rep rep;
			typedef typename duration::period period;
			
			time_point() : duration_() {}
			explicit time_point(duration const & d) : duration_(d) {}

			template<typename Duration2>
			time_point(time_point<clock, Duration2> const & other) : duration_(other.time_since_epoch()) {}
			
			duration time_since_epoch() const { return duration_; }
			
			bool operator == (time_point const & that) const { return this->duration_ == that.duration_; }
			bool operator < (time_point const & that) const { return this->duration_ <  that.duration_; }
			duration operator - (time_point const & that) const { duration d = this->duration_ - that.duration_; return d; }

			template<typename Duration2>
			time_point operator + (Duration2 const & d) const { time_point t(duration_ + d); return t; }
			template<typename Duration2>
			time_point operator - (Duration2 const & d) const { time_point t(duration_ - d); return t; }
			template<typename Duration2>
			time_point & operator += (Duration2 const & d) { duration_ += d; return *this; }
			template<typename Duration2>
			time_point & operator -= (Duration2 const & d) { duration_ -= d; return *this; }

			private: duration duration_;
		};
	}}}
#endif

/******************************************************************************************/
#ifdef BOOST_AUTO_TEST_CASE
	namespace universalis { namespace stdlib { namespace chrono { namespace test {
		BOOST_AUTO_TEST_CASE(duration_test) {
			{
				hours const h(1);
				BOOST_CHECK(h.count() == 1);

				minutes const m(h);
				BOOST_CHECK(m == h);
				BOOST_CHECK(m.count() == 60);

				seconds const s(m);
				BOOST_CHECK(s == h);
				BOOST_CHECK(s == m);
				BOOST_CHECK(s.count() == 60 * 60);

				milliseconds const ms(s);
				BOOST_CHECK(ms == h);
				BOOST_CHECK(ms == m);
				BOOST_CHECK(ms == s);
				BOOST_CHECK(ms.count() == 60 * 60 * 1000);

				microseconds const us(ms);
				BOOST_CHECK(us == h);
				BOOST_CHECK(us == m);
				BOOST_CHECK(us == s);
				BOOST_CHECK(us == ms);
				BOOST_CHECK(us.count() == 60 * 60 * 1000 * 1000LL);

				nanoseconds const ns(us);
				BOOST_CHECK(ns == h);
				BOOST_CHECK(ns == m);
				BOOST_CHECK(ns == s);
				BOOST_CHECK(ns == ms);
				BOOST_CHECK(ns == us);
				BOOST_CHECK(ns.count() == 60 * 60 * 1000 * 1000LL * 1000);
			} {
				hours const h(1);
				minutes m(1);
				m += h;
				BOOST_CHECK(m == minutes(1) + hours(1));
				BOOST_CHECK((m - h).count() == 1);
				BOOST_CHECK(m.count() == 61);
			} {
				seconds const s1(1), s2(2);
				BOOST_CHECK(s1 <  s2);
				BOOST_CHECK(s1 <= s2);
				BOOST_CHECK(s1 <= s1);
				BOOST_CHECK(s1 == s1);
				BOOST_CHECK(s1 != s2);
				BOOST_CHECK(s1 >= s1);
				BOOST_CHECK(s2 >= s1);
				BOOST_CHECK(s2 >  s1);
				BOOST_CHECK(s1 + s1 == s2);
				BOOST_CHECK(s1 * 2 == s2);
			}
		}
		
		BOOST_AUTO_TEST_CASE(time_point_test) {
			hours const h(1);
			time_point<void, nanoseconds> const t0;
			time_point<void, nanoseconds> t1(t0 + h);
			BOOST_CHECK(t1 == t0 + h);
			BOOST_CHECK(t1 - t0 == h);
			t1 -= h;
			BOOST_CHECK(t1 == t0);
		}
	}}}}
#endif
