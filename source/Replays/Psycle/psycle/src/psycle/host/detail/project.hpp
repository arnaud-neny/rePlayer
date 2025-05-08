///\file
///\brief public project-wide definitions. file included first by every header.
#pragma once
#include "config.hpp"
#include <universalis.hpp>

/**************************************************************************************************/
/// string describing the configuration of the build

#define PSYCLE__BUILD__CONFIG(EOL) \
	"Compiler build tool chain = " DIVERSALIS__COMPILER__VERSION__STRING EOL \
	"RMS VU = " UNIVERSALIS__COMPILER__STRINGIZE(PSYCLE__CONFIGURATION__RMS_VUS) EOL \
	"Debugging = " PSYCLE__BUILD__CONFIG__DEBUG

	/// value to show in the string describing the configuration of the build.
	#ifdef NDEBUG
		#define PSYCLE__BUILD__CONFIG__DEBUG "off"
	#else
		#define PSYCLE__BUILD__CONFIG__DEBUG "on"
	#endif

/**************************************************************************************************/
// namespace aliases

#ifndef DIVERSALIS__COMPILER__RESOURCE
	namespace universalis {
		namespace stdlib {}
		namespace os { namespace loggers {} }
	}
	namespace psycle {
		namespace helpers {
			namespace math {}
		}
		namespace host {
			namespace loggers = universalis::os::loggers;
			using namespace helpers;
			using namespace helpers::math;
			using namespace universalis::stdlib;
			namespace std {
				// gross hack, merge the two beasts together >_>
				using namespace ::std;
				using namespace universalis::stdlib;
				using ::std::exception;
			}
		}
	}
#endif
