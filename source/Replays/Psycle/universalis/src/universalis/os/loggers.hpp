// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2004-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once
#include "exception.hpp"
#include <universalis/compiler/typenameof.hpp>
#include <universalis/compiler/location.hpp>
#include <universalis/stdlib/mutex.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

namespace universalis { namespace os {

namespace loggers {
	class multiplex_logger;
}
		
/// logger.
class UNIVERSALIS__DECL logger {
	public:
		logger() {}
		virtual ~logger() throw() {}

		void operator()(int const level, std::string const & message) throw() { log(level, message); }
		void operator()(int const level, std::string const & message, compiler::location const & location) throw() { log(level, message, location); }

	protected:
		void            log(int const level, std::string const &, compiler::location const & location) throw();
		void            log(int const level, std::string const &) throw();
		virtual void do_log(int const level, std::string const & message, compiler::location const &) throw() = 0;
		virtual void do_log(int const level, std::string const &) throw() = 0;

	private: friend class loggers::multiplex_logger;
		stdlib::mutex mutex_;
		typedef universalis::stdlib::lock_guard<stdlib::mutex> lock_guard_type;
};

namespace loggers {

    /// empty.
    class empty_logger {
    public:
        void operator()(int const level, std::string const& message) { }
        void operator()(int const level, std::string const& message, compiler::location const& location) { }
		bool add(logger& logger) { return true; }
		bool remove(logger const& logger) { return true; }

		static empty_logger& singleton() { static empty_logger singleton_; return singleton_; }
    };


	/// logger which forwards to multiple loggers.
	class multiplex_logger : public logger, protected std::vector<logger*> {
		public:
			virtual ~multiplex_logger() throw() {}

		///\name container operations
		///\{
			public:
				UNIVERSALIS__DECL bool add   (logger       & logger);
				UNIVERSALIS__DECL bool remove(logger const & logger);
		///\}

		protected:
			/*implement*/ UNIVERSALIS__DECL void do_log(int const level, std::string const & message, compiler::location const &) throw();
			/*implement*/ UNIVERSALIS__DECL void do_log(int const level, std::string const &) throw();

		///\name singleton
		///\{
			public:                    multiplex_logger static & singleton() throw() { return singleton_; }
			private: UNIVERSALIS__DECL multiplex_logger static   singleton_;
		///\}
	};

	/// logger which outputs to a stream.
	class stream_logger : public logger {
		public:
			stream_logger(std::ostream & ostream) : ostream_(ostream) {}

		protected:
			/*implement*/ UNIVERSALIS__DECL void do_log(int const level, std::string const & message, compiler::location const &) throw();
			/*implement*/ UNIVERSALIS__DECL void do_log(int const level, std::string const &) throw();

		///\name underlying stream
		///\{
			protected: std::ostream & ostream() const throw() { return ostream_; }
			private:   std::ostream & ostream_;
		///\}

		///\name default stream singleton
		///\{
			public:                           logger static & default_logger() throw() { return default_logger_; }
			private: UNIVERSALIS__DECL stream_logger static   default_logger_;
		///\}
	};

	/// levels of importance of logger messages.
	namespace levels {
		/// levels of importance of logger messages.
		enum level {
			trace,       ///< very low level, debug, flooding output.
			information, ///< normal, informative output.
			warning,     ///< warnings.
			exception,   ///< exceptions thrown from software, via "throw some_exception;".
			crash        ///< exceptions thrown from cpu/os. They are translated into c++ exceptions, \see cpu::exception and os::exception.
		};
		
		/// the compile-time threshold level for the loggers
		level const compiled_threshold(
			#if defined UNIVERSALIS__OS__LOGGERS__LEVELS__COMPILED_THRESHOLD
				UNIVERSALIS__OS__LOGGERS__LEVELS__COMPILED_THRESHOLD
			#elif defined NDEBUG
				information
			#else
				trace
			#endif
		);
	}

	/// handling of threshold levels for a logger.
	template<typename Logger, levels::level Threshold_Level = levels::compiled_threshold>
	class logger_threshold_level {
		public:
			typedef Logger logger;
			operator logger & () const throw() { return logger::singleton(); }
			int threshold_level() const throw() { return Threshold_Level; }
			operator bool () const throw() { return Threshold_Level >= levels::compiled_threshold; }
			bool operator()() const throw() { return *this; }
			void operator()(std::string const & string) throw() { if(*this) logger::singleton()(Threshold_Level, string); }
			void operator()(std::string const & message, compiler::location const & location) throw() { if(*this) logger::singleton()(Threshold_Level, message, location); }
	};

	/// very low level, debug, flooding output.
	///\see levels::trace
	logger_threshold_level<empty_logger, levels::trace> inline & trace() throw() {
		logger_threshold_level<empty_logger, levels::trace> static once;
		return once;
	}
	
	/// normal, informative output.
	///\see levels::information
	logger_threshold_level<empty_logger, levels::information> inline & information() throw() {
		logger_threshold_level<empty_logger, levels::information> static once;
		return once;
	}
	
	/// warnings.
	///\see levels::warning
	logger_threshold_level<empty_logger, levels::warning> inline & warning() throw() {
		logger_threshold_level<empty_logger, levels::warning> static once;
		return once;
	}
	
	/// exceptions thrown from software, via "throw some_exception;".
	///\see levels::exception
	logger_threshold_level<empty_logger, levels::exception> inline & exception() throw() {
		logger_threshold_level<empty_logger, levels::exception> static once;
		return once;
	}
	
	/// exceptions thrown from cpu/os.
	/// They are translated into c++ exceptions, \see cpu::exception and os::exception.
	///\see levels::crash
	logger_threshold_level<empty_logger, levels::crash> inline & crash() throw() {
		logger_threshold_level<empty_logger, levels::crash> static once;
		return once;
	}
}

}}
