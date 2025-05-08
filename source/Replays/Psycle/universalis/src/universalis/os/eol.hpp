// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2000-2011 members of the psycle project http://psycle.sourceforge.net ; johan boule <bohan@jabber.org>

#pragma once

/// end-of-line character sequence on the host platform.
#if defined DIVERSALIS__OS__MICROSOFT
	#define UNIVERSALIS__OS__EOL "\r\n"
#elif defined DIVERSALIS__OS__APPLE && DIVERSALIS__OS__VERSION__MAJOR < 10 // before bsd-unix (darwin)
	#define UNIVERSALIS__OS__EOL "\r"
#else
	#define UNIVERSALIS__OS__EOL "\n"
#endif
