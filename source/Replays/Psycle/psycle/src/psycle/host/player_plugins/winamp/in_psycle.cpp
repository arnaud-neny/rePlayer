/*

  Winamp .psy Player input plugin
  -------------------------------

  This plugin plays Psycle Song files with Winamp 2.x/5.x
  and compatible players.

*/

#include <psycle/host/detail/project.private.hpp>
#include "WinampGlobal.hpp"
#include "WinampConfig.hpp"
#include <psycle/host/Song.hpp>
#include <psycle/host/Player.hpp>
#include <psycle/host/Machine.hpp>
#include <psycle/host/internal_machines.hpp>
#include <psycle/host/machineloader.hpp>
#include "fake_progressDialog.hpp"
#include "resources.hpp"
#include <psycle/helpers/math.hpp>

namespace winamp {
#include <Winamp/in2.h>	// Winamp Input plugin header file
#include <Winamp/wa_ipc.h>
#include <api/service/api_service.h>
extern api_service *serviceManager;
#define WASABI_API_SVC serviceManager

#include <api/service/waServiceFactory.h>
#include <Agave/Language/api_language.h>
}

#define WA_PLUGIN_VERSION "1.10"

#if defined DIVERSALIS__COMPILER__FEATURE__AUTO_LINK
	#pragma comment(lib, "winmm")
#endif

using namespace psycle::helpers::math;
//using namespace psycle::host <-disabled since it breaks namespace std.

//
// Global Variables.
//
// NOTE: when creating a new plugin this must be changed as it will otherwise cause conflicts!
// {131996C0-65E2-4721-A5E7-0C839F7EC472}
static const GUID in_psycle_GUID = 
{ 0x131996c0, 0x65e2, 0x4721, { 0xa5, 0xe7, 0xc, 0x83, 0x9f, 0x7e, 0xc4, 0x72 } };

// Wasabi based services for localisation support
winamp::api_service *WASABI_API_SVC = 0;
winamp::api_language *WASABI_API_LNG = 0;
// these two must be declared as they're used by the language api's
// when the system is comparing/loading the different resources
HINSTANCE WASABI_API_LNG_HINST = 0,
		  WASABI_API_ORIG_HINST = 0;

psycle::host::WinampGlobal global_;

HWND configwnd = 0;
WNDPROC lpWndProcOld = 0;

bool loading=false;
char infofileName[_MAX_PATH];
bool usewasabi=false;
bool uninstallDelete=false;

//Forward Declarations
INT_PTR CALLBACK ConfigProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK InfoProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool BrowseForFolder(HWND m_hWnd, std::string& rpath);
void GenerateSongTitle(in_char *title, const in_char *file, const in_char *author, const in_char *name);

extern In_Module mod;


/////////////////////////////////////////////////////////////
//
// In_Module methods
//

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT ret = CallWindowProc(lpWndProcOld,hwnd,message,wParam,lParam);
	return ret;
}

void config(HWND hwndParent)
{
	if(!IsWindow(configwnd))
	{
		if(usewasabi) {
			// calling the dialog macros from api_language will load the available dialog
			// resource using the standard dialog api calls (dialogbox, createdialogparam, etc)
			WASABI_API_DIALOGBOX(IDD_CONFIGDLG, hwndParent, ConfigProc);
		}
		else
		{
			DialogBox(mod.hDllInstance,(char*)IDD_CONFIGDLG,hwndParent,ConfigProc);
		}
	}
	else
	{
		SetActiveWindow(configwnd);
	}
}
void about(HWND hwndParent)
{
	if(usewasabi) 
	{
		// to display a localised title and message for a messagebox then
		// it requires a call to the lngstring and lngstring_buf macros as
		// otherwise the messagebox will show the same string for the title
		// and the message body due to how the language api work with the
		// lngstring macro (as it uses an internal buffer which will be
		// overwritten by subsequent calls especially at the same time).

		wchar_t title[128];
		wchar_t* about1 = WASABI_API_LNGSTRINGW(IDS_ABOUT1);
		wchar_t about2[256];
		wchar_t about3[256];
		WASABI_API_LNGSTRINGW_BUF(IDS_ABOUT2,about2,256);
		WASABI_API_LNGSTRINGW_BUF(IDS_ABOUT3,about3,256);
		wchar_t about[1024];
		char* versc = PSYCLE__VERSION;
		char* datec = __DATE__;
		wchar_t versw[64];
		wchar_t datew[32];
		MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,versc,-1,versw,32);
		MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,datec,-1,datew,32);
		
		wsprintfW(about,L"%s: %s\n\n%s %s\n\n%s", about1, 
			versw, 
			about2 ,
			datew,
			about3);
		/*linker fails with this
		CStringW about;
		about.FormatMessage(aboutformat, PSYCLE__VERSION, __DATE__);*/
		MessageBoxW(hwndParent, about, 
						WASABI_API_LNGSTRINGW_BUF(IDS_ABOUT_TITLE,title,128),
						MB_OK);
	}
	else {
		char about1[256];
		char about2[256];
		char about3[256];
		char title[256];
		char about[1024];
		LoadString(mod.hDllInstance,IDS_ABOUT1,about1,256);
		LoadString(mod.hDllInstance,IDS_ABOUT2,about2,256);
		LoadString(mod.hDllInstance,IDS_ABOUT3,about3,256);
		LoadString(mod.hDllInstance,IDS_ABOUT_TITLE,title,256);
		sprintf(about,"%s: %s\n\n%s %s\n\n%s", about1, 
			PSYCLE__VERSION, 
			about2 ,
			__DATE__,
			about3);
		MessageBoxA(hwndParent, about, title, MB_OK);
	}
}

void init()
{
	global_.conf().LoadWinampSettings(mod);
	global_.conf().RefreshSettings();
	global_.song().fileName[0] = '\0';
	global_.song().New();
	global_.conf().RefreshAudio();
	global_.machineload().LoadPluginInfo(false);

	usewasabi = (SendMessage(mod.hMainWindow, WM_WA_IPC, 0, IPC_GETVERSION) >= 0x5050);
	if (usewasabi)
	{
		// this is our first stage and will attempt to load the base of the wasabi services
		// so we can the get and load other services like the localisation service api
		WASABI_API_SVC = (winamp::api_service*)SendMessage(mod.hMainWindow, WM_WA_IPC, 0, IPC_GET_API_SERVICE);
		if (WASABI_API_SVC == (winamp::api_service*)1) WASABI_API_SVC = NULL;
		if(WASABI_API_SVC != NULL)
		{
			// now that we've got an instance of the base service api, we load the language
			// api service using it's guid to get it as shown
			winamp::waServiceFactory *sf = WASABI_API_SVC->service_getServiceByGuid(winamp::languageApiGUID);
			if (sf) WASABI_API_LNG = reinterpret_cast<winamp::api_language*>(sf->getInterface());

			// now we attempt to load the localisation api to load our lng file by passing the
			// current hinstance of the plugin and it's associated guid
			//
			// note: hence the need for GenExampleLangGUID to be unique to our plugin as that is
			// used to indentify and determine the lng file to be loaded by the loader mechanism
			WASABI_API_START_LANG(mod.hDllInstance,in_psycle_GUID);
			// however if you wanted to access an existing language file then you could pass in
			// the known GUID of the file you want after looking in Agave\Language\lang.h
			// (an example of this will follow in a later example of this example project)

			// doing this will mean that Winamp will be correctly subclassed depending on it
			// being a unicode (5.34+) or a legacy client install so that the taskbar text will
			// show correctly as SetWindowLongPtrA(..) on a 5.34+ client install will cause ???
			// to appear in the taskbar for unicode titled files despite Winamp's unicode support
			//
			if (IsWindowUnicode(mod.hMainWindow))
			{
				lpWndProcOld = (WNDPROC)SetWindowLongPtrW(mod.hMainWindow,GWLP_WNDPROC,(LONG_PTR)WndProc);
			}
			else
			{
				lpWndProcOld = (WNDPROC)SetWindowLongPtrA(mod.hMainWindow,GWLP_WNDPROC,(LONG_PTR)WndProc);
			}
		}
	}
}

void quit()
{
	global_.conf().audioDriver().Reset();
	global_.player().stop_threads();
	if(uninstallDelete) {
		global_.conf().DeleteWinampSettings(mod);
	}
}

void getfileinfo(const in_char *file, in_char *title, int *length_in_ms)
{
	if (!file || !*file) // Current Playing
	{
		if (global_.player()._playing)
		{
			if (title) { 
				GenerateSongTitle(title, file, global_.song().author.c_str(), global_.song().name.c_str());
			}
			
			if (length_in_ms) { 
				int inoutseqPos, inoutpatLine, inoutseektime_ms, inoutlinecount = -1;
				global_.player().CalcPosition(global_.song(), inoutseqPos, inoutpatLine, inoutseektime_ms, inoutlinecount);
				*length_in_ms = inoutseektime_ms;
			}
		}
	}
	else
	{
		psycle::host::OldPsyFile songfile;
		char Header[9];

		if (songfile.Open(file))
		{
	
			songfile.Read(&Header, 8);
			Header[8]=0;
			
			if (strcmp(Header,"PSY3SONG")==0)
			{
				psycle::host::CProgressDialog dlg;
				psycle::host::Song *pSong;
				pSong=new psycle::host::Song();
				pSong->New();
				songfile.Seek(0);
				pSong->Load(&songfile,dlg,false);

				if (title) { GenerateSongTitle(title, file, pSong->author.c_str(), pSong->name.c_str()); }
				if (length_in_ms)
				{
					int inoutseqPos, inoutpatLine, inoutseektime_ms, inoutlinecount = -1;
					global_.player().CalcPosition(*pSong, inoutseqPos, inoutpatLine, inoutseektime_ms, inoutlinecount);
					*length_in_ms = inoutseektime_ms;
				}
				songfile.Close();
				delete pSong;
				return;
			}
			else if (strcmp(Header,"PSY2SONG")==0)
			{
				char Name[33], Author[33];
				int bpm, tpb, spt, num, playLength, patternLines[MAX_PATTERNS];
				unsigned char playOrder[128];
				
				songfile.Read(Name, 32); Name[32]='\0';
				songfile.Read(Author, 32); Author[32]='\0';
				if (title) { GenerateSongTitle(title, file, Author, Name); }

				if (length_in_ms) { 
					songfile.Skip(128); // Comment;
					songfile.Read(&bpm, sizeof(bpm));
					songfile.Read(&spt, sizeof(spt));
					if ( spt <= 0 )  // Shouldn't happen, but has happened. (bug of 1.1b1)
					{	tpb= 4; spt = 4315;
					}
					else tpb = 44100*15*4/(spt*bpm);

					songfile.Skip(sizeof(unsigned char)); // currentOctave
					songfile.Skip(sizeof(unsigned char)*64); // BusMachine

					songfile.Read(&playOrder, 128);
					songfile.Read(&playLength, sizeof(playLength));
					songfile.Skip(sizeof(int));	//SONG_TRACKS

					// Patterns
					//
					songfile.Read(&num, sizeof(num));
					for (int i=0; i<num; i++)
					{
						songfile.Read(&patternLines[i], sizeof(patternLines[0]));
						songfile.Skip(sizeof(char)*32);	// Pattern Name
						songfile.Skip(patternLines[i]*OLD_MAX_TRACKS*sizeof(psycle::host::PatternEntry)); // Pattern Data
					}
					
					*length_in_ms = 0;
					for (int i=0; i <playLength; i++)
					{
						*length_in_ms += (patternLines[playOrder[i]] * 60000/(bpm * tpb));
					}
					
				}
				songfile.Close();
				return;
			}
			songfile.Close();
		}
		if (title) { GenerateSongTitle(title, file, NULL, NULL); }
		if (length_in_ms ) *length_in_ms = -1000;
	}
}

int infoBox(const in_char *file, HWND hwndParent)
{
	if ( strcmp(file,global_.song().fileName.c_str()) ) // if not the current one
	{
		strcpy(infofileName,file);
	}
	else infofileName[0]='\0';

	if(usewasabi) {
		// calling the dialog macros from api_language will load the available dialog
		// resource using the standard dialog api calls (dialogbox, createdialogparam, etc)
		WASABI_API_DIALOGBOX(IDD_INFODLG, hwndParent, InfoProc);
	}
	else
	{
		DialogBox(mod.hDllInstance,(char*)IDD_INFODLG,hwndParent,InfoProc);
	}
	
	return 0;
}

int isourfile(const in_char *fn)
{
	psycle::host::OldPsyFile file;
	char Header[9];
	
	if (file.Open(fn))
	{
		file.Read(&Header, 8);
		Header[8]=0;
		
		if (strcmp(Header,"PSY3SONG")==0)
		{
			file.Close();
			return 1;
		}
		else if (strcmp(Header,"PSY2SONG")==0)
		{
			file.Close();
			return 1;
		}
		file.Close();
	}
	return 0;
}

int play(const in_char *fn)
{
	global_.player().Stop();//	stop();

	psycle::host::OldPsyFile file;
	if (!file.Open(fn))
	{
		return -1;
	}
	psycle::host::CProgressDialog dlg;
	dlg.ShowWindow(SW_SHOW);
	global_.song().filesize=file.FileSize();
	loading = true;
//	global_.song().New();
	global_.song().Load(&file,dlg);
	file.Close();
	global_.song().fileName = fn;
	global_.player()._loopSong=false;
	global_.player().Start(0,0);

	global_.conf()._pOutputDriver->Enable(true);
	mod.SetInfo(global_.song().BeatsPerMin(),global_.conf().audioDriver().GetSamplesPerSec()/1000,
		global_.conf().audioDriver().settings().numChannels(),1);
	mod.SAVSAInit(global_.conf().audioDriver().GetOutputLatencyMs(),
		global_.conf().audioDriver().GetSamplesPerSec());
	mod.VSASetInfo(global_.conf().audioDriver().GetSamplesPerSec(),
		global_.conf().audioDriver().settings().numChannels());

	loading = false;
	return 0;
}

void pause() { ((psycle::host::WinampDriver*)global_.conf()._pOutputDriver)->Pause(true); }
void unpause() { ((psycle::host::WinampDriver*)global_.conf()._pOutputDriver)->Pause(false); }
int ispaused() { return ((psycle::host::WinampDriver*)global_.conf()._pOutputDriver)->Paused(); }
void stop()
{ 
	while (loading) Sleep(10);
	global_.conf().audioDriver().Enable(false);
	global_.song().New();
	mod.SAVSADeInit();
}


int getlength() { 
	int inoutseqPos, inoutpatLine, inoutseektime_ms, inoutlinecount = -1;
	global_.player().CalcPosition(global_.song(), inoutseqPos, inoutpatLine, inoutseektime_ms, inoutlinecount);
	return inoutseektime_ms;
}
int getoutputtime() { return mod.outMod->GetOutputTime(); }
void setoutputtime(int time_in_ms)
{
	global_.player().SeekToPosition(global_.song(),time_in_ms);
	mod.outMod->Flush(time_in_ms);
}

void setvolume(int volume) { mod.outMod->SetVolume(volume); }
void setpan(int pan) { mod.outMod->SetPan(pan); }

void eq_set(int on, char data[10], int preamp) { }

In_Module mod = 
{
	IN_VER,
	"Psycle Winamp Plugin v" WA_PLUGIN_VERSION ,
	NULL,  // hMainWindow (filled in by winamp)
	NULL,  // hDllInstance (filled in by winamp)
	"psy\0Psycle Song (*.psy)\0",
	1,
	IN_MODULE_FLAG_USES_OUTPUT_PLUGIN,
	config,
	about,
	init,
	quit,
	getfileinfo,
	infoBox,
	isourfile,
	play,
	pause,
	unpause,
	ispaused,
	stop,
	
	getlength,
	getoutputtime,
	setoutputtime,

	setvolume,
	setpan,

	0,0,0,0,0,0,0,0,0,  // visualization calls filled in by winamp

	0,0,  // dsp calls filled in by winamp

	eq_set,

	NULL, // setinfo call filled in by winamp

	NULL // out_mod filled in by winamp

};

//
// Exported Symbols
//

#ifdef __cplusplus
extern "C" {
#endif
	__declspec( dllexport ) In_Module * winampGetInModule2()
	{
		return &mod;
	}

	__declspec(dllexport) int winampUninstallPlugin(HINSTANCE hDllInst, HWND hwndDlg, int param)
	{
		if (usewasabi) {
			// we prompt the user here if they want us to remove our settings with default as no
			//(just incase) when the plugin is being uninstalled from within Winamp's preferences
			if(MessageBoxA(hwndDlg,WASABI_API_LNGSTRING(IDS_UNINSTALL_CONFIRM_REMOVE_SETTINGS),
						   mod.description,MB_YESNO|MB_DEFBUTTON2) == IDYES)
			{
				uninstallDelete = true;
			}

			// as we're doing too much in subclasses, etc we cannot allow for on-the-fly removal so need to do a normal reboot
			return IN_PLUGIN_UNINSTALL_REBOOT;
		}
		else {
			char confirmuninstall[1024];
			LoadString(mod.hDllInstance,IDS_UNINSTALL_CONFIRM_REMOVE_SETTINGS,confirmuninstall,1024);

			if(MessageBoxA(hwndDlg,confirmuninstall,
						   mod.description,MB_YESNO|MB_DEFBUTTON2) == IDYES)
			{
				uninstallDelete = true;
			}
			return IN_PLUGIN_UNINSTALL_NOW;
		}
	}
#ifdef __cplusplus
}
#endif

//
// Internal methods
//
void GenerateSongTitle(in_char *title, const in_char *file, const in_char *author, const in_char *name)
{
	if( author != NULL && name != NULL
			&& (strcmp(author,"Unnamed") || strlen(author) == 0)
			&& (strcmp(author,"Untitled") || strlen(name) == 0)) {
		sprintf(title,"%s - %s",author,name);
	}
	else {
		const char *p=file+strlen(file);
		while (*p != '\\' && p >= file) p--;
		strcpy(title,++p);
	}
}

void SaveDialogSettings(HWND hwndDlg)
{
	int c;
	char tmptext[_MAX_PATH];
	if (global_.player()._playing ) stop();
	//SRATE
	c = SendDlgItemMessage(hwndDlg,IDC_SRATE,CB_GETCURSEL,0,0);
	if ( (c % 2) == 0) global_.conf().SetSamplesPerSec((int)(11025*powf(2.0f,(float)(c/2))));
	else global_.conf().SetSamplesPerSec((int)(12000*powf(2.0f,(float)(c/2))));
	//BITDEPTH
	c = SendDlgItemMessage(hwndDlg,IDC_BITDEPTH,CB_GETCURSEL,0,0);
	c = 16 + (8*c);
	global_.conf().audioDriver().settings().setValidBitDepth(c);
	if (c==16) {
		global_.conf().audioDriver().settings().setDither(true);
	}
	else {
		global_.conf().audioDriver().settings().setDither(false);
	}

	c = SendDlgItemMessage(hwndDlg,IDC_AUTOSTOP,BM_GETCHECK,0,0);
	global_.conf().UseAutoStopMachines(c>0?true:false);

	GetDlgItemText(hwndDlg,IDC_NUMBER_THREADS,tmptext,_MAX_PATH);
	global_.conf().SetNumThreads(std::atoi(tmptext));
	GetDlgItemText(hwndDlg,IDC_NATIVEPATH,tmptext,_MAX_PATH);
	global_.conf().SetPluginDir(tmptext);
	GetDlgItemText(hwndDlg,IDC_VSTPATH,tmptext,_MAX_PATH);
	global_.conf().SetVst32Dir(tmptext);
	GetDlgItemText(hwndDlg,IDC_VSTPATH64,tmptext,_MAX_PATH);
	global_.conf().SetVst64Dir(tmptext);
	global_.conf().UseJBridge(SendDlgItemMessage(hwndDlg,IDC_CKBRIDGING,BM_GETCHECK,0,0) == BST_CHECKED);
	global_.conf().SaveWinampSettings(mod);
	global_.conf().RefreshSettings();
}

INT_PTR CALLBACK ConfigProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char tmptext[_MAX_PATH];
	
	switch(uMsg)
	{
	case WM_INITDIALOG:
	{
		configwnd = hwndDlg;

		//	Sample Rate Combobox
		char valstr[10];
		for (int c=0;c<4;c++)
		{
			sprintf(valstr,"%i",(int)(11025*powf(2.0f,(float)c)));
			SendDlgItemMessage(hwndDlg,IDC_SRATE,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(valstr));
			sprintf(valstr,"%i",(int)(12000*powf(2.0f,(float)c)));
			SendDlgItemMessage(hwndDlg,IDC_SRATE,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(valstr));
		}
		switch (global_.conf().audioDriver().GetSamplesPerSec())
		{
		case 11025: SendDlgItemMessage(hwndDlg,IDC_SRATE,CB_SETCURSEL,0,0);break;
		case 12000: SendDlgItemMessage(hwndDlg,IDC_SRATE,CB_SETCURSEL,1,0);break;
		case 22050: SendDlgItemMessage(hwndDlg,IDC_SRATE,CB_SETCURSEL,2,0);break;
		case 24000: SendDlgItemMessage(hwndDlg,IDC_SRATE,CB_SETCURSEL,3,0);break;
		case 44100: SendDlgItemMessage(hwndDlg,IDC_SRATE,CB_SETCURSEL,4,0);break;
		case 48000: SendDlgItemMessage(hwndDlg,IDC_SRATE,CB_SETCURSEL,5,0);break;
		case 88200: SendDlgItemMessage(hwndDlg,IDC_SRATE,CB_SETCURSEL,6,0);break;
		case 96000: SendDlgItemMessage(hwndDlg,IDC_SRATE,CB_SETCURSEL,7,0);break;
		}
		
		//	Sample Rate Combobox
		for (int c=16;c<33;c+=8)
		{
			sprintf(valstr,"%i", c);
			SendDlgItemMessage(hwndDlg,IDC_BITDEPTH,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(valstr));
		}
		switch (global_.conf().audioDriver().GetSampleValidBits())
		{
		case 16: SendDlgItemMessage(hwndDlg,IDC_BITDEPTH,CB_SETCURSEL,0,0);break;
		case 24: SendDlgItemMessage(hwndDlg,IDC_BITDEPTH,CB_SETCURSEL,1,0);break;
		case 32: SendDlgItemMessage(hwndDlg,IDC_BITDEPTH,CB_SETCURSEL,2,0);break;
		}


		// Autostop
		SendDlgItemMessage(hwndDlg,IDC_AUTOSTOP,BM_SETCHECK,global_.conf().UsesAutoStopMachines()?1:0,0);
		
		// Numberthreads
		{
			std::ostringstream s;
			s <<global_.conf().GetNumThreads();
			SetDlgItemText(hwndDlg,IDC_WA_NUMBER_THREADS,s.str().c_str());
		}
		{
			std::ostringstream s;
			s <<global_.player().num_threads();
			SetDlgItemText(hwndDlg,IDC_WA_RUNNING_THREADS,s.str().c_str());
		}
		
		// Directories.
		SetDlgItemText(hwndDlg,IDC_NATIVEPATH,global_.conf().GetPluginDir().c_str());
		SetDlgItemText(hwndDlg,IDC_VSTPATH,global_.conf().GetVst32Dir().c_str());
		SetDlgItemText(hwndDlg,IDC_VSTPATH64,global_.conf().GetVst64Dir().c_str());
		
		// Cache
		if(usewasabi) 
		{
			wchar_t* exists = WASABI_API_LNGSTRINGW(IDS_CONFIG_CACHE_EXISTS);
			wchar_t notexists[256];
			WASABI_API_LNGSTRINGW_BUF(IDS_CONFIG_CACHE_MISSING,notexists,256);

			SetDlgItemTextW(hwndDlg,IDC_CACHEVALID,global_.machineload().IsLoaded() ? 
				exists : notexists);
		}
		else {
			char exists[128];
			char notexists[128];
			LoadString(mod.hDllInstance,IDS_CONFIG_CACHE_EXISTS,exists,128);
			LoadString(mod.hDllInstance,IDS_CONFIG_CACHE_MISSING,notexists,128);

			SetDlgItemText(hwndDlg,IDC_CACHEVALID,global_.machineload().IsLoaded() ? 
				exists : notexists);
		}
		if (global_.conf().SupportsJBridge()) {
			EnableWindow(GetDlgItem(hwndDlg,IDC_CKBRIDGING),TRUE);
			SendDlgItemMessage(hwndDlg,IDC_CKBRIDGING,BM_SETCHECK,global_.conf().UsesJBridge()?1:0,0);
			if (global_.conf().UsesJBridge()) {
				EnableWindow(GetDlgItem(hwndDlg,IDC_VSTPATH64),TRUE);
				EnableWindow(GetDlgItem(hwndDlg,IDC_BWVST64),TRUE);
			}
		}

		return 1;
		break;
	}
	case WM_COMMAND:
	{
		switch(wParam)
		{
		case IDOK:
			SaveDialogSettings(hwndDlg);
			EndDialog(hwndDlg,1);
			break;
		case IDCANCEL:
			EndDialog(hwndDlg,0);
			break;
		case IDC_REGENERATE:
			SaveDialogSettings(hwndDlg);
			global_.machineload().ReScan(true);
			if(usewasabi)
			{
				wchar_t* exists = WASABI_API_LNGSTRINGW(IDS_CONFIG_CACHE_EXISTS);
				wchar_t notexists[256];
				WASABI_API_LNGSTRINGW_BUF(IDS_CONFIG_CACHE_MISSING,notexists,256);

				SetDlgItemTextW(hwndDlg,IDC_CACHEVALID,global_.machineload().IsLoaded() ? 
					exists : notexists);
			}
			else 
			{
				char exists[128];
				char notexists[128];
				LoadString(mod.hDllInstance,IDS_CONFIG_CACHE_EXISTS,exists,128);
				LoadString(mod.hDllInstance,IDS_CONFIG_CACHE_MISSING,notexists,128);

				SetDlgItemText(hwndDlg,IDC_CACHEVALID,global_.machineload().IsLoaded() ? 
					exists : notexists);
			}
			break;
		case IDC_BWNATIVE:
			{
				GetDlgItemText(hwndDlg,IDC_NATIVEPATH,tmptext,_MAX_PATH);
				std::string plugin = tmptext;
				if (BrowseForFolder(hwndDlg,plugin))
				{
					SetDlgItemText(hwndDlg,IDC_NATIVEPATH,plugin.c_str());
				}
			}
			break;
		case IDC_BWVST:
			{
				GetDlgItemText(hwndDlg,IDC_VSTPATH,tmptext,_MAX_PATH);
				std::string plugin = tmptext;
				if (BrowseForFolder(hwndDlg,plugin))
				{
					SetDlgItemText(hwndDlg,IDC_VSTPATH,plugin.c_str());
				}
			}
			break;
		case IDC_BWVST64:
			{
				GetDlgItemText(hwndDlg,IDC_VSTPATH64,tmptext,_MAX_PATH);
				std::string plugin = tmptext;
				if (BrowseForFolder(hwndDlg,plugin))
				{
					SetDlgItemText(hwndDlg,IDC_VSTPATH64,plugin.c_str());
				}
			}
			break;
		case IDC_CKBRIDGING:
			{
				LRESULT result = SendDlgItemMessage(hwndDlg,IDC_CKBRIDGING,BM_GETCHECK,0,0);
				EnableWindow(GetDlgItem(hwndDlg,IDC_VSTPATH64),result==BST_CHECKED?TRUE:FALSE);
				EnableWindow(GetDlgItem(hwndDlg,IDC_BWVST64),result==BST_CHECKED?TRUE:FALSE);
			}
			break;
		}
		break;
	}
	}
	return 0;
}
INT_PTR CALLBACK InfoProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int i,j=0;
	psycle::host::Song* pSong;
	
	switch(uMsg)
	{
	case WM_INITDIALOG:
		if ( infofileName[0]!='\0' )
		{
			psycle::host::CProgressDialog dlg;
			pSong=new psycle::host::Song();
			psycle::host::OldPsyFile file;
			pSong->New(); // this is NOT done in Load for the winamp plugin.
			if (file.Open(infofileName))
			{
				pSong->filesize=file.FileSize();
				pSong->Load(&file,dlg,false);
				pSong->fileName = infofileName;
				file.Close();
			}
		}
		else pSong= &global_.song();
		
		//SetWindowText(hwndDlg,"Psycle Song Information");
		char tmp2[20];
		SetDlgItemText(hwndDlg,IDC_DLG_FILENAME,pSong->fileName.c_str());
		SetDlgItemText(hwndDlg,IDC_ARTIST,pSong->author.c_str());
		SetDlgItemText(hwndDlg,IDC_TITLE,pSong->name.c_str());
		SetDlgItemText(hwndDlg,IDC_COMMENTS,pSong->comments.c_str());

		for( i=0;i<MAX_MACHINES;i++)
		{
			if(pSong->_pMachine[i])
			{
				char valstr[255];
				switch( pSong->_pMachine[i]->_type )
				{
					case psycle::host::MACH_VST: //fallthrough
					case psycle::host::MACH_VSTFX: strcpy(tmp2,"V");break;
					case psycle::host::MACH_PLUGIN: strcpy(tmp2,"N");break;
					case psycle::host::MACH_MASTER: strcpy(tmp2,"M");break;
					default: strcpy(tmp2,"I"); break;
				}
				
				if ( pSong->_pMachine[i]->_type == psycle::host::MACH_DUMMY && 
					strcmp("", pSong->_pMachine[i]->GetDllName())!=0 )
				{
					sprintf(valstr,"%.02x:[!]  %s",i,pSong->_pMachine[i]->_editName);
				}
				else sprintf(valstr,"%.02x:[%s]  %s",i,tmp2,pSong->_pMachine[i]->_editName);
				
				SendDlgItemMessage(hwndDlg,IDC_MACHINES,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(valstr));
				j++;
			}
		}
		{
			int inoutseqPos, inoutpatLine, inoutseektime_ms, inoutlinecount = -1;
			global_.player().CalcPosition(*pSong, inoutseqPos, inoutpatLine, inoutseektime_ms, inoutlinecount);
			i = inoutseektime_ms/1000;
		}

		if(usewasabi) 
		{
			wchar_t valstrw[1024];
			wchar_t* strfz = WASABI_API_LNGSTRINGW(IDS_INFO_FILESIZE);
			wchar_t strtra[256];
			wchar_t strbpm[256];
			wchar_t strlpb[256];
			wchar_t stror[256];
			wchar_t strsl[256];
			wchar_t strpu[256];
			wchar_t strmu[256];
			wchar_t striu[256];
			WASABI_API_LNGSTRINGW_BUF(IDS_INFO_TRACKS,strtra,256);
			WASABI_API_LNGSTRINGW_BUF(IDS_INFO_BPM,strbpm,256);
			WASABI_API_LNGSTRINGW_BUF(IDS_INFO_LPB,strlpb,256);
			WASABI_API_LNGSTRINGW_BUF(IDS_INFO_ORDERS,stror,256);
			WASABI_API_LNGSTRINGW_BUF(IDS_INFO_SONG_LENGTH,strsl,256);
			WASABI_API_LNGSTRINGW_BUF(IDS_INFO_PATTERNS_USED,strpu,256);
			WASABI_API_LNGSTRINGW_BUF(IDS_INFO_MACHINES_USED,strmu,256);
			WASABI_API_LNGSTRINGW_BUF(IDS_INFO_INSTRUMENTS_USED,striu,256);
			wsprintfW(valstrw,L"%s %i\n%s %i\n%s %i\n%s %i\n%s %i\n%s %02i:%02i\n%s %i\n%s %i\n%s %i",
				strfz,
				pSong->filesize,
				strtra,
				pSong->SongTracks(),
				strbpm,
				pSong->BeatsPerMin(),
				strlpb,
				pSong->LinesPerBeat(),
				stror,
				pSong->playLength,
				strsl,
				i / 60, i % 60,
				strpu,
				pSong->GetNumPatterns(),
				strmu,
				j,
				striu,
				pSong->GetNumInstruments());
			SetDlgItemTextW(hwndDlg,IDC_INFO,valstrw);
		}
		else
		{
			char valstr[1024];
			char strfz[128];
			char strtra[128];
			char strbpm[128];
			char strlpb[128];
			char stror[128];
			char strsl[128];
			char strpu[128];
			char strmu[128];
			char striu[128];
			LoadString(mod.hDllInstance,IDS_INFO_FILESIZE,strfz,128);
			LoadString(mod.hDllInstance,IDS_INFO_TRACKS,strtra,128);
			LoadString(mod.hDllInstance,IDS_INFO_BPM,strbpm,128);
			LoadString(mod.hDllInstance,IDS_INFO_LPB,strlpb,128);
			LoadString(mod.hDllInstance,IDS_INFO_ORDERS,stror,128);
			LoadString(mod.hDllInstance,IDS_INFO_SONG_LENGTH,strsl,128);
			LoadString(mod.hDllInstance,IDS_INFO_PATTERNS_USED,strpu,128);
			LoadString(mod.hDllInstance,IDS_INFO_MACHINES_USED,strmu,128);
			LoadString(mod.hDllInstance,IDS_INFO_INSTRUMENTS_USED,striu,128);

			sprintf(valstr,"%s %i\n%s %i\n%s %i\n%s %i\n%s %i\n%s %02i:%02i\n%s %i\n%s %i\n%s %i",
				strfz,
				pSong->filesize,
				strtra,
				pSong->SongTracks(),
				strbpm,
				pSong->BeatsPerMin(),
				strlpb,
				pSong->LinesPerBeat(),
				stror,
				pSong->playLength,
				strsl,
				i / 60, i % 60,
				strpu,
				pSong->GetNumPatterns(),
				strmu,
				j,
				striu,
				pSong->GetNumInstruments());
			SetDlgItemText(hwndDlg,IDC_INFO,valstr);
		}		
		
		if ( infofileName[0]!='\0' ) delete pSong;
			
		break;
	case WM_COMMAND:
		switch(wParam)
		{
		case IDCANCEL:
			EndDialog(hwndDlg,0);
			break;
		}
		break;
	}
	return 0;
}

bool BrowseForFolder(HWND hWnd,std::string& rpath)
{
	bool val=false;

	LPMALLOC pMalloc;
	// Gets the Shell's default allocator
	//
	if (::SHGetMalloc(&pMalloc) == NOERROR)
	{
		BROWSEINFO bi;
		char strselect[128];
		LoadString(mod.hDllInstance,IDS_BROWSE_SELECTDIR,strselect,128);
		char pszBuffer[MAX_PATH];
		LPITEMIDLIST pidl;
		// Get help on BROWSEINFO struct - it's got all the bit settings.
		//
		bi.hwndOwner = hWnd;
		bi.pidlRoot = NULL;
		bi.pszDisplayName = pszBuffer;
		bi.lpszTitle = strselect;
		bi.ulFlags = BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS;
		bi.lpfn = NULL;
		bi.lParam = 0;
		// This next call issues the dialog box.
		//
		if ((pidl = ::SHBrowseForFolder(&bi)) != NULL)
		{
			if (::SHGetPathFromIDList(pidl, pszBuffer))
			{
				// At this point pszBuffer contains the selected path
				//
				val = true;
				rpath =pszBuffer;
			}
			// Free the PIDL allocated by SHBrowseForFolder.
			//
			pMalloc->Free(pidl);
		}
		// Release the shell's allocator.
		//
		pMalloc->Release();
	}
	return val;
}
