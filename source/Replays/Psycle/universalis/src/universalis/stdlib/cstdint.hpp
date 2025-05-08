// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2012 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\file \brief cstdint standard header

#pragma once
#include <universalis/detail/project.hpp>
#if __cplusplus >= 201103L
	#include <cstdint>
	namespace universalis { namespace stdlib { namespace detail {
		namespace cstdint_impl = std;
	}}}
#else
	#include <boost/cstdint.hpp>
	namespace universalis { namespace stdlib { namespace detail {
		namespace cstdint_impl = boost;
	}}}
#endif

namespace universalis { namespace stdlib {
	using detail::cstdint_impl::int8_t;
	using detail::cstdint_impl::int_least8_t;
	using detail::cstdint_impl::int_fast8_t;
	using detail::cstdint_impl::uint8_t;
	using detail::cstdint_impl::uint_least8_t;
	using detail::cstdint_impl::uint_fast8_t;

	using detail::cstdint_impl::int16_t;
	using detail::cstdint_impl::int_least16_t;
	using detail::cstdint_impl::int_fast16_t;
	using detail::cstdint_impl::uint16_t;
	using detail::cstdint_impl::uint_least16_t;
	using detail::cstdint_impl::uint_fast16_t;

	using detail::cstdint_impl::int32_t;
	using detail::cstdint_impl::int_least32_t;
	using detail::cstdint_impl::int_fast32_t;
	using detail::cstdint_impl::uint32_t;
	using detail::cstdint_impl::uint_least32_t;
	using detail::cstdint_impl::uint_fast32_t;

	using detail::cstdint_impl::int64_t;
	using detail::cstdint_impl::int_least64_t;
	using detail::cstdint_impl::int_fast64_t;
	using detail::cstdint_impl::uint64_t;
	using detail::cstdint_impl::uint_least64_t;
	using detail::cstdint_impl::uint_fast64_t;

	using detail::cstdint_impl::intmax_t;
	using detail::cstdint_impl::uintmax_t;
}}
