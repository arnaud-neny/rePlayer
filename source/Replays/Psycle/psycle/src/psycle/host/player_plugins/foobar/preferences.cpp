
#include "preferences.hpp"
#include "FoobarGlobal.hpp"
#include "FoobarConfig.hpp"
#include <psycle/host/machineloader.hpp>
#include <psycle/host/AudioDriver.hpp>
#include <psycle/host/Player.hpp>

static preferences_page_factory_t<psycle_preferences_page_impl> g_preferences_page_myimpl_factory;

extern psycle::host::FoobarGlobal global_;

BOOL CPsyclePreferences::OnInitDialog(CWindow wnd, LPARAM param) {

	//	Sample Rate Combobox
	char valstr[10];
	for (int c=0;c<4;c++)
	{
		sprintf(valstr,"%i",(int)(11025*powf(2.0f,(float)c)));
		SendDlgItemMessage(IDC_SRATE,CB_ADDSTRING,0,(long)valstr);
		sprintf(valstr,"%i",(int)(12000*powf(2.0f,(float)c)));
		SendDlgItemMessage(IDC_SRATE,CB_ADDSTRING,0,(long)valstr);
	}
	switch (global_.conf().audioDriver().GetSamplesPerSec())
	{
	case 11025: SendDlgItemMessage(IDC_SRATE,CB_SETCURSEL,0,0);break;
	case 12000: SendDlgItemMessage(IDC_SRATE,CB_SETCURSEL,1,0);break;
	case 22050: SendDlgItemMessage(IDC_SRATE,CB_SETCURSEL,2,0);break;
	case 24000: SendDlgItemMessage(IDC_SRATE,CB_SETCURSEL,3,0);break;
	case 44100: SendDlgItemMessage(IDC_SRATE,CB_SETCURSEL,4,0);break;
	case 48000: SendDlgItemMessage(IDC_SRATE,CB_SETCURSEL,5,0);break;
	case 88200: SendDlgItemMessage(IDC_SRATE,CB_SETCURSEL,6,0);break;
	case 96000: SendDlgItemMessage(IDC_SRATE,CB_SETCURSEL,7,0);break;
	}
	
	// Autostop
	SendDlgItemMessage(IDC_AUTOSTOP,BM_SETCHECK,global_.conf().UsesAutoStopMachines()?1:0,0);
	
	// Numberthreads
	SetDlgItemInt(IDC_NUMBER_THREADS,global_.conf().GetNumThreads(), TRUE);
	SetDlgItemInt(IDC_RUNNING_THREADS,global_.player().num_threads(), FALSE);
	
	// Directories.
	SetDlgItemText(IDC_NATIVEPATH,global_.conf().GetPluginDir().c_str());
	SetDlgItemText(IDC_VSTPATH,global_.conf().GetVst32Dir().c_str());
	SetDlgItemText(IDC_VSTPATH64,global_.conf().GetVst64Dir().c_str());
	
	// Cache
	char exists[128];
	char notexists[128];
	LoadString(mod.hDllInstance,IDS_CONFIG_CACHE_EXISTS,exists,128);
	LoadString(mod.hDllInstance,IDS_CONFIG_CACHE_MISSING,notexists,128);

	SetDlgItemText(IDC_CACHEVALID,global_.machineload().IsLoaded() ? 
		exists : notexists);

	if (global_.conf().SupportsJBridge()) {
		EnableWindow(GetDlgItem(IDC_CKBRIDGING),TRUE);
		SendDlgItemMessage(IDC_CKBRIDGING,BM_SETCHECK,global_.conf().UsesJBridge()?1:0,0);
		if (global_.conf().UsesJBridge()) {
			EnableWindow(GetDlgItem(IDC_VSTPATH64),TRUE);
			EnableWindow(GetDlgItem(IDC_BWVST64),TRUE);
		}
	}
	return FALSE;
}


void CPsyclePreferences::apply() {
	int c;
	char tmptext[_MAX_PATH];
	if (global_.player()._playing ) stop();
	//SRATE
	c = SendDlgItemMessage(IDC_SRATE,CB_GETCURSEL,0,0);
	if ( (c % 2) == 0) global_.conf().SetSamplesPerSec((int)(11025*powf(2.0f,(float)(c/2))));
	else global_.conf().SetSamplesPerSec((int)(12000*powf(2.0f,(float)(c/2))));

	c = SendDlgItemMessage(IDC_AUTOSTOP,BM_GETCHECK,0,0);
	global_.conf().UseAutoStopMachines(c>0?true:false);

	GetDlgItemText(IDC_NUMBER_THREADS,tmptext,_MAX_PATH);
	global_.conf().SetNumThreads(std::atoi(tmptext));
	GetDlgItemText(IDC_NATIVEPATH,tmptext,_MAX_PATH);
	global_.conf().SetPluginDir(tmptext);
	GetDlgItemText(IDC_VSTPATH,tmptext,_MAX_PATH);
	global_.conf().SetVst32Dir(tmptext);
	GetDlgItemText(IDC_VSTPATH64,tmptext,_MAX_PATH);
	global_.conf().SetVst64Dir(tmptext);
	global_.conf().SaveFoobarSettings();
	global_.conf().RefreshSettings();
	
	OnChanged(); //our dialog content has not changed but the flags have - our currently shown values now match the settings so the apply button can be disabled
}

void CPsyclePreferences::reset() {
	//TODO
	OnChanged();
}

void CPsyclePreferences::OnRegenerate(UINT, int, CWindow)
{
	global_.machineload().ReScan(true);
	char exists[128];
	char notexists[128];
	LoadString(mod.hDllInstance,IDS_CONFIG_CACHE_EXISTS,exists,128);
	LoadString(mod.hDllInstance,IDS_CONFIG_CACHE_MISSING,notexists,128);

	SetDlgItemText(hwndDlg,IDC_CACHEVALID,global_.machineload().IsLoaded() ? 
		exists : notexists);
}
void CPsyclePreferences::OnBrowseNative(UINT, int, CWindow)
{
	char tmptext[_MAX_PATH];
	GetDlgItemText(IDC_NATIVEPATH,tmptext,_MAX_PATH);
	std::string plugin = tmptext;
	if (BrowseForFolder(get_wnd(),plugin))
	{
		SetDlgItemText(IDC_NATIVEPATH,plugin.c_str());
	}
}
void CPsyclePreferences::OnBrowseVst(UINT, int, CWindow)
{
	char tmptext[_MAX_PATH];
	GetDlgItemText(IDC_VSTPATH,tmptext,_MAX_PATH);
	std::string plugin = tmptext;
	if (BrowseForFolder(get_wnd(),plugin))
	{
		SetDlgItemText(IDC_VSTPATH,plugin.c_str());
	}
}
void CPsyclePreferences::OnBrowseVst64(UINT, int, CWindow)
{
	char tmptext[_MAX_PATH];
	GetDlgItemText(IDC_VSTPATH64,tmptext,_MAX_PATH);
	std::string plugin = tmptext;
	if (BrowseForFolder(get_wnd(),plugin))
	{
		SetDlgItemText(IDC_VSTPATH64,plugin.c_str());
	}
}
void CPsyclePreferences::OnCheckBridging(UINT, int, CWindow)
{
	LRESULT result = SendDlgItemMessage(IDC_CKBRIDGING,BM_GETCHECK,0,0);
	::EnableWindow(GetDlgItem(IDC_VSTPATH64),result==BST_CHECKED?TRUE:FALSE);
	::EnableWindow(GetDlgItem(IDC_BWVST64),result==BST_CHECKED?TRUE:FALSE);
}


t_uint32 CPsyclePreferences::get_state() {
	t_uint32 state = preferences_state::resettable;
	if (HasChanged()) state |= preferences_state::changed;
	return state;
}
bool CPsyclePreferences::HasChanged() {
	//TODO:
	/*
	return GetDlgItemInt(IDC_BOGO1, NULL, FALSE) != cfg_bogoSetting1 
		|| GetDlgItemInt(IDC_BOGO2, NULL, FALSE) != cfg_bogoSetting2;
	*/
	return false;
}

void CPsyclePreferences::OnChanged() {
	//tell the host that our state has changed to enable/disable the apply button appropriately.
	m_callback->on_state_changed();
}


bool CPsyclePreferences::BrowseForFolder(HWND hWnd,std::string& rpath)
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
