
#include <psycle/host/Global.hpp>
#include <foobar2000/SDK/foobar2000.h>

// Forward declaration
static pfc::string_formatter g_get_component_about();

// Declaration of your component's version information
// Since foobar2000 v1.0 having at least one of these in your DLL is mandatory to let the troubleshooter tell different versions of your component apart.
// Note that it is possible to declare multiple components within one DLL, but it's strongly recommended to keep only one declaration per DLL.
// As for 1.1, the version numbers are used by the component update finder to find updates; for that to work, you must have ONLY ONE declaration per DLL. If there are multiple declarations, the component is assumed to be outdated and a version number of "0" is assumed, to overwrite the component with whatever is currently on the site assuming that it comes with proper version numbers.
DECLARE_COMPONENT_VERSION_COPY("Psycle Module Decoder","1.10",
	g_get_component_about()
);


// This will prevent users from renaming your component around (important for proper troubleshooter behaviors) or loading multiple instances of it.
VALIDATE_COMPONENT_FILENAME("foo_psy.dll");


// Declare .PSY as a supported file type to make it show in "open file" dialog etc.
DECLARE_FILE_TYPE("Psycle songs","*.PSY");

/**
 * Generates the "About" text during component initialization.
 * The About text includes version information about the dynamically linked
 * TAK decoder library.
 */
static pfc::string_formatter g_get_component_about()
{
	pfc::string_formatter about;
	about << "Psycle Module Decoder for foobar2000.";
	about << "\n\nBased on Psycle engine " << PSYCLE__VERSION;
	about << "\n\nDeveloped by Psycledelics. 2001-2011";
	about << "\nhttp://psycle.pastnotecut.org/";
	about << "\n\nOriginal port of the plugin from winamp to foobar2000 by KarlKoX.

	return about;
}
