///\file
///\brief psycle native plugin interface
#pragma once

#include <string>
#include <vector>

// *** Note ***
// Because this file may be used outside of the psycle project itself,
// we should not introduce any dependency by including
// anything that is not part of the c++ standard library.

namespace psycle { namespace plugin_interface {

/// defined for 64 Bit platforms
#if defined __LP64__ || defined __LLP64__ || defined _WIN64
	#define PSYCLE__PLUGIN__64BIT_PLATFORM
#endif

/// machine interface version.
unsigned short const MI_VERSION = 0x0013;

/// max number of pattern tracks
int const MAX_TRACKS = 64;

/// max number of samples (per channel) that the Work function may ask to return
int const MAX_BUFFER_LENGTH = 256;

///\name note values
///\{
	/// value of B-8. NOTE: C minus 1 is note 0 and C-0 is note 12!
	int const NOTE_MAX = 119;
	int const NOTE_MIDDLEC = 60; // Middle C.
	int const NOTE_A4 = 69; // Note tuned at 440Hz.
	/// value of the "off" (release) note.
	int const NOTE_NOTEOFF = 120;
	/// empty value
	int const NOTE_NONE = 255;
///\}

/// the pi constant.
/// note: this is also defined in <psycle/helpers/math/constants.hpp> but we want no dependency here
double const pi = 3.14159265358979323846;

/*////////////////////////////////////////////////////////////////////////*/

/// class to define the modificable parameters of the machine
class CMachineParameter {
	public:
		/// Short name: "Cutoff"
		char const *Name;
		/// Longer description: "Cutoff Frequency (0-7f)"
		char const *Description;
		/// recommended >= 0. If negative, minValue is represented as 0 in the pattern
		int MinValue;
		/// recommended <= 65535. Basically so that it can be represented in the pattern
		int MaxValue;
		/// flags. (see below)
		int Flags;
		/// default value for params that have MPF_STATE flag set
		int DefValue;
};

///\name CMachineParameter flags
///\{
	/// shows a line with no text nor knob
	int const MPF_NULL = 0;
	/// shows a line with the text in a centered label
	int const MPF_LABEL = 1;
	/// shows a tweakable knob and text
	int const MPF_STATE = 2;
///\}

///\name CFxCallback::CallbackFunc codes
///\{
	int const CBID_GET_WINDOW = 0;
///\}
///\name CMachineInfo::HostEvent codes
///\{
	/// Sent by the host to ask if this plugin uses the auxiliary column. return true or false.
	int const HE_NEEDS_AUX_COLUMN = 0;
///\}

/*////////////////////////////////////////////////////////////////////////*/

/// class defining the machine properties
class CMachineInfo {
	public:
		CMachineInfo(
			short APIVersion, int flags, int numParameters, CMachineParameter const * const * parameters,
			char const * name, char const * shortName, char const * author, char const * command, int numCols
		) :
			APIVersion(APIVersion), PlugVersion(0), Flags(flags), numParameters(numParameters), Parameters(parameters),
			Name(name), ShortName(shortName), Author(author), Command(command), numCols(numCols)
		{}

		CMachineInfo(
			short APIVersion, short PlugVersion, int flags, int numParameters, CMachineParameter const * const * parameters,
			char const * name, char const * shortName, char const * author, char const * command, int numCols
		) :
			APIVersion(APIVersion), PlugVersion(PlugVersion), Flags(flags), numParameters(numParameters), Parameters(parameters),
			Name(name), ShortName(shortName), Author(author), Command(command), numCols(numCols)
		{}

		/// API version. Use MI_VERSION
		short const APIVersion;
		/// plug version. Your machine version. Shown in Hexadecimal.
		short const PlugVersion;
		/// Machine flags. Defines the type of machine
		int const Flags;
		/// number of parameters.
		int const numParameters;
		/// a pointer to an array of pointers to parameter infos
		CMachineParameter const * const * const Parameters;
		/// "Name of the machine in listing"
		char const * const Name;
		/// "Name of the machine in machine Display"
		char const * const ShortName;
		/// "Name of author"
		char const * const Author;
		/// "Text to show as custom command (see Command method)"
		char const * const Command;
		/// number of columns to display in the parameters' window
		int numCols;
};

///\name CMachineInfo flags
///\{
	/// Machine is an effect (can receive audio)
	int const EFFECT = 0;
	///\todo: unused
	int const SEQUENCER = 1;
	/// Machine is a generator (does not receive audio)
	int const GENERATOR = 3;
	///\todo: unused
	int const CUSTOM_GUI = 16;
///\}

/*////////////////////////////////////////////////////////////////////////*/

/// callback functions to let plugins communicate with the host.
/// DO NOT CHANGE the order of the functions. This is an exported class!
class CFxCallback {
	public:
		virtual void MessBox(char const * /*message*/, char const * /*caption*/, unsigned int /*type*/) const = 0;
		///\todo: doc
		virtual int CallbackFunc(int /*cbkID*/, int /*par1*/, int /*par2*/, void* /*par3*/) = 0;

		/// unused vtable slot kept for binary compatibility with old closed-source plugins
		virtual float * unused0(int, int) = 0;
		/// unused vtable slot kept for binary compatibility with old closed-source plugins
		virtual float * unused1(int, int) = 0;

		virtual int GetTickLength() const = 0;
		virtual int GetSamplingRate() const = 0;
		virtual int GetBPM() const = 0;
		virtual int GetTPB() const = 0;
		/// do not move this destructor from here. Since this is an interface, the position matters.
		virtual ~CFxCallback() throw() {}
		/// Open a load (openMode=true) or save (openMode=false) dialog.
		/// filter is in MFC format: description|*.ext|description2|*ext2|| 
		/// if you indicate a directory in inoutName, it will be used. Else, you need to provide
		/// an empty string ([0]='\0') and the plugin dir will be used instead.
		/// returns true if the user pressed open/save, else return false.
		/// filter can be any size you want. inoutname has to be 1024 chars.
		virtual bool FileBox(bool openMode, char filter[], char inoutName[]) = 0;
};

/*////////////////////////////////////////////////////////////////////////*/

/// base machine class from which plugins derived.
/// Note: We keep empty definitions of the functions in the header so that
/// plugins don't need to implement everything nor link with a default implementation.
class CMachineInterface {
	public:
		virtual ~CMachineInterface() {}
		/// Initialization method called by the Host at initialization time.
		/// pCB callback pointer is available here.
		virtual void Init() {}
		/// Called by the Host each sequence tick (in psyclemfc, means each line).
		/// It is called even when the playback is stopped, so that the plugin can synchronize correctly.
		virtual void SequencerTick() {}
		/// Called by the host when the user changes a paramter from the UI or a tweak from the pattern.
		/// It is also called just after calling Init, to set each parameter to its default value, so you
		/// don't need to explicitely do so.
		virtual void ParameterTweak(int /*par*/, int /*val*/) {}

		/// Called by the host when it needs audio data. the pointers are input-output pointers
		/// (read the data in case of effects, and write the new data over). 
		/// numsamples is the amount of samples (per channel) to generate and tracks is mostly unused. It carries
		/// the current number of tracks of the song.
		virtual void Work(float * /*psamplesleft*/, float * /*psamplesright*/, int /*numsamples*/, int /*tracks*/) {}

		/// Called by the host when the user presses the stop button.
		virtual void Stop() {}

		///\name Export / Import
		///\{
			/// Called by the host when loading a song or preset.
			/// The pointer contains the data saved by the plugin with GetData().
			/// It is called after all parameters have been set with ParameterTweak.
			virtual void PutData(void * /*data*/) {}
			/// Called by the host when saving a song or preset. Use it to to save extra data that you need.
			/// The values of the parameters will be automatically restored via calls to parameterTweak().
			virtual void GetData(void * /*data*/) {}
			/// Called by the host before calling GetData to know the size to allocate for pData before calling GetData()
			virtual int GetDataSize() { return 0; }
		///\}

		/// Called by the host when the user selects the command menu option. Commonly used to show a help box,
		/// but can be used to show a specific editor,a configuration or other similar things.
		virtual void Command() {}

		/// unused vtable slot kept for binary compatibility with old closed-source plugins
		virtual void unused0(int /*track*/) {}
		/// unused vtable slot kept for binary compatibility with old closed-source plugins
		virtual bool unused1(int /*track*/) const { return false; }

		///called by the host to send a midi event (mcm) to the plugin.
		virtual void MidiEvent(int /*channel*/, int /*midievent*/, int /*value*/) {}

		/// unused vtable slot kept for binary compatibility with old closed-source plugins
		virtual void unused2(unsigned int const /*data*/) {}

		/// Called by the host when it requires to show a description of the value of a parameter.
		/// return false to tell the host to show the numerical value. Return true and fill txt with
		/// some text to show that text to the user.
		virtual bool DescribeValue(char * /*txt*/, int const /*param*/, int const /*value*/) { return false; }
		///called by the host to send a control event or ask for information. See HostEvent codes.
		virtual bool HostEvent(int const /*eventNr*/, int const /*val1*/, float const /*val2*/) { return false; }
		/// Called by the host when there is some data to play. Only notes and pattern commands will be informed
		/// this way. Tweaks call ParameterTweak
		virtual void SeqTick(int /*channel*/, int /*note*/, int /*ins*/, int /*cmd*/, int /*val*/) {}

		/// unused vtable slot kept for binary compatibility with old closed-source plugins
		virtual void unused3() {}

	public:
		/// initialize this member in the constructor with the size of parameters.
		int * Vals;

		/// callback.
		/// This member is initialized by the engine right after it calls CreateMachine().
		/// Don't touch it in the constructor.
		CFxCallback mutable * pCB;
};

////////////////////////////////////////////////////////////////////////////
/// From the text below, you just need to know that once you've defined the MachineInteface class
/// and the MachineInfo instance, use PSYCLE__PLUGIN__INSTANTIATOR() to export it.

struct PluginEntry
{
    std::string m_dllName;
    CMachineInfo const* const (*m_getInfo)();
    CMachineInterface* (*m_createMachine)();
};

struct PluginFactory
{
	static bool Register(const char* dllName, CMachineInfo const* const (*getInfo)(), CMachineInterface* (*createMachine)());

	static std::vector<PluginEntry>& Get()
	{
        static std::vector<PluginEntry> nativePlugins;
		return nativePlugins;
	}
};

#define PSYCLE__PLUGIN__INSTANTIATOR(dllname, typename, info) \
		PSYCLE__PLUGIN__DYN_LINK__EXPORT \
		psycle::plugin_interface::CMachineInfo const * const \
		PSYCLE__PLUGIN__CALLING_CONVENTION \
		PSYCLE__PLUGIN__SYMBOL_NAME__GET_INFO() { return &info; } \
		\
		PSYCLE__PLUGIN__DYN_LINK__EXPORT \
		psycle::plugin_interface::CMachineInterface * \
		PSYCLE__PLUGIN__CALLING_CONVENTION \
		PSYCLE__PLUGIN__SYMBOL_NAME__CREATE_MACHINE() { return new typename; } \
		\
		namespace { \
		struct ThePluginEntry : psycle::plugin_interface::PluginEntry { static bool ms_isRegistered; }; \
		bool ThePluginEntry::ms_isRegistered = psycle::plugin_interface::PluginFactory::Register(dllname, PSYCLE__PLUGIN__SYMBOL_NAME__GET_INFO, PSYCLE__PLUGIN__SYMBOL_NAME__CREATE_MACHINE); }
/*
		PSYCLE__PLUGIN__DYN_LINK__EXPORT \
		void \
		PSYCLE__PLUGIN__CALLING_CONVENTION \
		PSYCLE__PLUGIN__SYMBOL_NAME__DELETE_MACHINE(psycle::plugin_interface::CMachineInterface & plugin) { delete &plugin; }
*/

#define PSYCLE__PLUGIN__SYMBOL_NAME__GET_INFO GetInfo
#define PSYCLE__PLUGIN__SYMBOL_NAME__CREATE_MACHINE CreateMachine
#define PSYCLE__PLUGIN__SYMBOL_NAME__DELETE_MACHINE DeleteMachine

/// we don't use universalis/diversalis here because we want no dependency
#if 1//!defined _WIN32 && !defined __CYGWIN__ && !defined __MSYS__ && !defined _UWIN
	#define PSYCLE__PLUGIN__DYN_LINK__EXPORT
	#define PSYCLE__PLUGIN__CALLING_CONVENTION
#elif defined __GNUG__
	#define PSYCLE__PLUGIN__DYN_LINK__EXPORT __attribute__((__dllexport__))
	#define PSYCLE__PLUGIN__CALLING_CONVENTION __attribute__((__cdecl__))
#elif defined _MSC_VER
	#define PSYCLE__PLUGIN__DYN_LINK__EXPORT __declspec(dllexport)
	#define PSYCLE__PLUGIN__CALLING_CONVENTION __cdecl
#else
#error "please add definition for your compiler"
#endif

#define PSYCLE__PLUGIN__DETAIL__STRINGIZED(x) PSYCLE__PLUGIN__DETAIL__STRINGIZED__NO_EXPANSION(x)
#define PSYCLE__PLUGIN__DETAIL__STRINGIZED__NO_EXPANSION(x) #x

namespace symbols {
	const char get_info_function_name[] =
		PSYCLE__PLUGIN__DETAIL__STRINGIZED(PSYCLE__PLUGIN__SYMBOL_NAME__GET_INFO);
	typedef
        psycle::plugin_interface::CMachineInfo *
        (PSYCLE__PLUGIN__CALLING_CONVENTION * get_info_function)
		();
	
	const char create_machine_function_name[] =
		PSYCLE__PLUGIN__DETAIL__STRINGIZED(PSYCLE__PLUGIN__SYMBOL_NAME__CREATE_MACHINE);
	typedef
		psycle::plugin_interface::CMachineInterface *
		(PSYCLE__PLUGIN__CALLING_CONVENTION * create_machine_function)
		();

	const char delete_machine_function_name[] =
		PSYCLE__PLUGIN__DETAIL__STRINGIZED(PSYCLE__PLUGIN__SYMBOL_NAME__DELETE_MACHINE);
	typedef
		void
		(PSYCLE__PLUGIN__CALLING_CONVENTION * delete_machine_function)
		(psycle::plugin_interface::CMachineInterface &);
}

extern std::vector<PluginEntry*> gNativePlugins;

}}
