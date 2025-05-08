#pragma once
#include <psycle/host/detail/project.hpp>

///\file
///\brief the version number of the psycle host application.

/// M.m.p , where:
/// 	- M = major version number.
/// 	- m = minor version number:
/// 		- if even, then it's a stable release.
/// 		- if odd, then it's an unstable build or release candidate for next stable minor version number.
/// 	- p = patch number:
/// 		- if the minor version number is even (stable release), then this patch number is used for bug fixes, so, it's hopefully rarely incremented.
/// 		- if the minor version number is odd (unstable builds), then this patch number is incremented very often, perhaps each source commit.

#define PSYCLE__NAME "psycle"
#define PSYCLE__TITLE "Psycle Modular Music Creation Studio"
#define PSYCLE__BRANCH ""
#define PSYCLE__COPYRIGHT "Copyright(c) 2000-2014 Psycledelics (https://psycle.sourceforge.net)"
#define PSYCLE__LICENSE "Some parts GPL, others public domain"
#define PSYCLE__VERSION__MAJOR 1
#define PSYCLE__VERSION__MINOR 12
#define PSYCLE__VERSION__PATCH 2
#define PSYCLE__VERSION__QUALITY "beta"
#define PSYCLE__SOURCE_URL "Subversion $URL: file:///svn/p/psycle/code/trunk/psycle/src/psycle/host/Version.hpp $"

/// identifies what source version the build comes from.
#define PSYCLE__VERSION \
	PSYCLE__BRANCH " " \
	UNIVERSALIS__COMPILER__STRINGIZE(PSYCLE__VERSION__MAJOR) "." \
	UNIVERSALIS__COMPILER__STRINGIZE(PSYCLE__VERSION__MINOR) "." \
	UNIVERSALIS__COMPILER__STRINGIZE(PSYCLE__VERSION__PATCH) " " \
	PSYCLE__VERSION__QUALITY

#define PSYCLE__VERSION__NUMBER \
	PSYCLE__VERSION__MAJOR * 10000 + \
	PSYCLE__VERSION__MINOR * 100 + \
	PSYCLE__VERSION__PATCH * 10


/// identifies both what sources the build comes from, and what build options were used.
#define PSYCLE__BUILD__IDENTIFIER(EOL) \
	"Version: " PSYCLE__VERSION EOL \
	"Build config options:" EOL "\t- " PSYCLE__BUILD__CONFIG(EOL "\t- ") EOL \
	"Built on: " PSYCLE__BUILD__DATE EOL \
	"From sources: " PSYCLE__SOURCE_URL

#if defined __DATE__ && defined __TIME__
	#define PSYCLE__BUILD__DATE __DATE__ ", " __TIME__
#else
	/// __DATE__ and __TIME__ don't seem to work with msvc's resource compiler. It works with mingw's.
	#define PSYCLE__BUILD__DATE "a sunny day"
#endif

// #if defined _M_X64
// 	#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
// #elif defined _M_IX86
// 	#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
// #elif defined _M_IA64
// 	#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
// #else
// 	#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
// #endif
