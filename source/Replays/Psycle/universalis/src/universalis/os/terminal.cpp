// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 1999-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

///\implementation universalis::os::loggers

#include <universalis/detail/project.private.hpp>
#include "terminal.hpp"
#include "loggers.hpp"
#if defined DIVERSALIS__OS__MICROSOFT
	#include "exception.hpp"
	#include <universalis/stdlib/exception.hpp>
	#include <universalis/compiler/typenameof.hpp>
	#include "include_windows_without_crap.hpp"
	#include <wincon.h>
	#include <iostream>
	#if defined DIVERSALIS__COMPILER__MICROSOFT
		#include <cstdio> // std::freopen
		#include <io.h> // _open_osfhandle
		#include <fcntl.h> // _O_TEXT for _open_osfhandle
	#endif
	#include <sstream>
	#include <limits>
	#include <cassert>
	#if defined DIVERSALIS__COMPILER__FEATURE__AUTO_LINK
		#pragma comment(lib, "user32") // for the MessageBox function
	#endif
#endif

namespace universalis { namespace os {

terminal::terminal()
#if !defined DIVERSALIS__OS__MICROSOFT
{} // we do nothing when the operating system is not microsoft's
#else
: allocated_(false)
{
	// ok, the following code looks completly weird,
	// but that's actually the "simplest" way one can allocate a "console" in a gui application the microsoft way.
	try {
		////////////////////////////////////////////////////////////////////
		// allocates a new console window if we don't have one attached yet

		::HANDLE console_window(::GetConsoleWindow());
		if(!console_window) {
			if(!::AllocConsole()) {
				std::ostringstream s;
				s << "could not allocate a console window: " << exceptions::desc();
				throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
			}
			allocated_ = true;
			console_window = ::GetConsoleWindow();
		}
		if(!console_window) {
			std::ostringstream s;
			s << "could not get a console window: " << exceptions::desc();
			throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
		}

		/////////////////////////////////////////////////////////////////////////
		// allocates the standard file streams for the operating sytem i/o layer

		// standard output file stream
		::HANDLE os_output(::GetStdHandle(STD_OUTPUT_HANDLE));
		if(!os_output) { // if we don't have a console attached yet, allocates a console for the operating sytem i/o layer
			if(!::AllocConsole()) {
				std::ostringstream s;
				s << "could not allocate a console at the operating system layer: " << exceptions::desc();
				throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
			}
			allocated_ = true;
			os_output = ::GetStdHandle(STD_OUTPUT_HANDLE);
		}
		if(!os_output) {
			std::ostringstream s;
			s << "could not allocate a standard output file stream at the operating system layer: " << exceptions::desc();
			throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
		}

		// standard error file stream
		::HANDLE os_error(::GetStdHandle(STD_ERROR_HANDLE));
		if(!os_error) {
			std::ostringstream s;
			s << "could not allocate a standard error file stream at the operating system layer: " << exceptions::desc();
			throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
		}

		// standard input file stream
		::HANDLE os_input(::GetStdHandle(STD_INPUT_HANDLE));
		if(!os_input) {
			std::ostringstream s;
			s << "could not allocate a standard input file stream at the operating system layer: " << exceptions::desc();
			throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
		}
		
		if(allocated_) {
			/////////////////////////////////////////////////////////////////
			// allocates the standard file streams for the runtime i/o layer

			// standard output file stream
			if(os_output) {
				#if defined DIVERSALIS__COMPILER__MICROSOFT
					int file_descriptor(::_open_osfhandle(/* microsoft messed the type definition of handles, we *must* hard cast! */ reinterpret_cast<intptr_t>(os_output), _O_TEXT /* opens file in text (translated) mode */));
					if(file_descriptor == -1) {
							std::ostringstream s;
							s << "could not allocate a standard output file descriptor at the runtime layer: " << stdlib::exceptions::desc();
							throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
					} else {
						//TODO: where is this handle supposed to be closed?
						const FILE * const file(::_fdopen(file_descriptor, "w"));
						if(!file) {
								std::ostringstream s;
								s << "could not open the standard output file stream at the runtime layer: " << stdlib::exceptions::desc();
								throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
						}
						*stdout = *file;
					}
					if(::setvbuf(stdout, 0, _IONBF, 0)) {
							std::ostringstream s;
							s << "could not set a buffer for the standard output file stream at the runtime layer: " << stdlib::exceptions::desc();
							throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
					}
				#else
					// todo
				#endif
			}

			// standard error file stream
			if(os_error) {
				#if defined DIVERSALIS__COMPILER__MICROSOFT
					int file_descriptor(::_open_osfhandle(/* microsoft messed the type definition of handles, we *must* hard cast! */ reinterpret_cast<intptr_t>(os_error), _O_TEXT /* opens file in text (translated) mode */));
					if(file_descriptor == -1) {
							std::ostringstream s;
							s << "could not allocate a standard error file descriptor at the runtime layer: " << stdlib::exceptions::desc();
							throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
					}
					//TODO: where is this handle supposed to be closed?
					const FILE * const file(::_fdopen(file_descriptor, "w"));
					if(!file) {
							std::ostringstream s;
							s << "could not open the standard error file stream at the runtime layer: " << stdlib::exceptions::desc();
							throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
					}
					*stderr = *file;
					if(::setvbuf(stderr, 0, _IONBF, 0)) {
							std::ostringstream s;
							s << "could not set a buffer for the standard error file stream at the runtime layer: " << stdlib::exceptions::desc();
							throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
					}
				#else
					// todo
				#endif
			}

			// standard input file stream
			if(os_input) {
				#if defined DIVERSALIS__COMPILER__MICROSOFT
					int file_descriptor(::_open_osfhandle(/* microsoft messed the type definition of handles, we *must* hard cast! */ reinterpret_cast<intptr_t>(os_input), _O_TEXT /* opens file in text (translated) mode */));
					if(file_descriptor == -1) {
							std::ostringstream s;
							s << "could not allocate a standard input file descriptor at the runtime layer: " << stdlib::exceptions::desc();
							throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
					}
					//TODO: where is this handle supposed to be closed?
					const FILE * const file(::_fdopen(file_descriptor, "r"));
					if(!file) {
							std::ostringstream s;
							s << "could not open the standard input file stream at the runtime layer: " << stdlib::exceptions::desc();
							throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
					}
					*stdin = *file;
					if(::setvbuf(stdin, 0, _IONBF, 0)) {
							std::ostringstream s;
							s << "could not set a buffer for the standard input file stream at the runtime layer: " << stdlib::exceptions::desc();
							throw exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION);
					}
				#else
					// todo
				#endif
			}

			/////////////////////////////////////////////////////////////////////////
			// allocates the standard file streams for the standard i/o stream layer

			std::ios::sync_with_stdio(); // makes cout, wcout, cin, wcin, wcerr, cerr, wclog and clog point to console as well
		}

		//////////////////////////////////
		// sets console window properties

		if(::HANDLE console = os_output) { // ::GetStdHandle(STD_OUTPUT_HANDLE)
			::CONSOLE_SCREEN_BUFFER_INFO buffer;
			::GetConsoleScreenBufferInfo(console, &buffer);
			{ // colors
				unsigned short const attributes(BACKGROUND_BLUE | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
				::SetConsoleTextAttribute(console, attributes);
				{
					int const width(buffer.dwSize.X), height(buffer.dwSize.Y);
					::COORD const coord = {0, 0};
					unsigned long int length;
					::FillConsoleOutputAttribute(console, attributes, width * height, coord, &length);
				}
			}
			{ // buffer size
				int const width(256), height(1024);
				if(buffer.dwSize.X < width) buffer.dwSize.X = width;
				if(buffer.dwSize.Y < height) buffer.dwSize.Y = height;
				if(!::SetConsoleScreenBufferSize(console, buffer.dwSize)) {
					::GetConsoleScreenBufferInfo(console, &buffer);
					int const width(80), height(50); // on non nt systems, we can only have such a ridiculous size!
					if(buffer.dwSize.X < width) buffer.dwSize.X = width;
					if(buffer.dwSize.Y < height) buffer.dwSize.Y = height;
					if(!::SetConsoleScreenBufferSize(console, buffer.dwSize))
					{
						// huh? giving up.
					}
				}
			}
			{ // cursor
				::CONSOLE_CURSOR_INFO cursor;
				::GetConsoleCursorInfo(console, &cursor); 
				cursor.dwSize = 100;
				cursor.bVisible = true;
				::SetConsoleCursorInfo(console, &cursor);
			}
		}
	} catch(std::exception const & e) {
		#if defined DIVERSALIS__COMPILER__MICROSOFT
			std::freopen("conout$", "w", stdout);
		#else
			// todo
		#endif
		std::ios::sync_with_stdio(); // makes cout, wcout, cin, wcin, wcerr, cerr, wclog and clog point to console as well

		std::ostringstream types; types << compiler::typenameof(*this) << ", threw " << compiler::typenameof(e);
		{
			std::ostringstream s; s << types.str() << ": " << e.what();
			std::printf /* we use sprintf because std::ios::sync_with_stdio() might have failed too */ (s.str().c_str());
		}
		{
			std::ostringstream title; title << "error in " << types.str();
			std::ostringstream message; message << "could not allocate a console!\n" << types.str() << '\n' << e.what();
			::MessageBox(NULL, message.str().c_str(), title.str().c_str(), MB_OK | MB_ICONWARNING);
		}
		throw;
	}
	if(allocated_) std::cout << "console allocated\n";
}
#endif

terminal::~terminal() throw() {
	#if defined DIVERSALIS__OS__MICROSOFT
		if(allocated_) {
			std::cout <<
				"There is no parent process owning this terminal console window, so, it will close when this process terminates.\n"
				"Press enter to terminate and close this window ...\n";
			std::cin.get();
			::FreeConsole();
		}
	#endif
}

void terminal::output(const int & logger_level, const std::string & string) {
	scoped_lock lock(mutex_);
	#if !defined DIVERSALIS__OS__MICROSOFT
		std::cout << "\e[1m" << "logger: " << logger_level << string;
		// ansi terminal
		int const static color [] = {0, 2, 6, 1, 5, 3, 4, 7};
		std::cout << /* no \e ? */ "\033[1;3" << color[logger_level % sizeof color] << "m" << string << "\033[0m\n";
	#else
		::HANDLE output_handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
		if(!output_handle) {
			// oh dear!
			// fallback to std::cerr and std::clog
			std::cerr << /* UNIVERSALIS__COMPILER__LOCATION << ": " */ "no standard output.\n";
			std::clog << "logger: " << logger_level << ": " << string;
		}
		::WORD const base_attributes(BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY);
		unsigned short attributes(base_attributes);
		switch(logger_level) {
			case loggers::levels::trace:
				attributes |= FOREGROUND_BLUE;
				break;
			case loggers::levels::information:
				attributes |= FOREGROUND_GREEN;
				break;
			case loggers::levels::warning:
				attributes |= FOREGROUND_RED | FOREGROUND_GREEN;
				break;
			case loggers::levels::exception:
				attributes |= FOREGROUND_RED;
				break;
			case loggers::levels::crash:
				attributes |= FOREGROUND_RED | FOREGROUND_INTENSITY;
				break;
			default:
				attributes |= 0;
		};
		::SetConsoleTextAttribute(output_handle, attributes);
		std::size_t const length(string.length());
		assert(length <= std::numeric_limits< ::DWORD >::max());
		::DWORD length_written;
		::WriteConsole(output_handle, string.c_str(), static_cast< ::DWORD >(length), &length_written, 0);
		assert(length_written == length);
		// [bohan] "reset" the attributes before new line because otherwize we have
		// [bohan] the cells of the whole next line set with attributes, up to the rightmost column.
		// [bohan] i haven't checked, but it is possible that this only happens when the buffer scrolls due to the new line.
		attributes = base_attributes;
		::WriteConsole(output_handle, "\n", 1, &length_written, 0);
		assert(length_written = 1);
		// beep on problems
		switch(logger_level) {
			case loggers::levels::warning:
			case loggers::levels::exception:
			case loggers::levels::crash:
				::WriteConsole(output_handle, "\a", 1, &length_written, 0);
		}
	#endif
}

}}
