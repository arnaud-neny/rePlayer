///\file
///\brief implementation file for psycle::host::CNewMachine.
#include <psycle/host/detail/project.private.hpp>
#include "NewMachine.hpp"
#include "Configuration.hpp"
#include "ProgressDialog.hpp"

#include "Plugin.hpp"
#include "VstHost24.hpp"

#include <string>
#include <sstream>
#include <fstream>
#include <algorithm> // for std::transform
#include <cctype> // for std::tolower

#include <direct.h>

namespace psycle { namespace host {

		int CNewMachine::pluginOrder = 1;
		bool CNewMachine::pluginName = 1;
		int CNewMachine::selectedClass=internal;
		int CNewMachine::selectedMode=modegen;


		CNewMachine::CNewMachine(CWnd* pParent)
			: CDialog(CNewMachine::IDD, pParent)
		{
			m_orderby = pluginOrder;
			m_showdllName = pluginName;
			shellIdx = 0;
			//pluginOrder = 0; Do NOT uncomment. It would cause the variable to be reseted each time.
				// It is initialized above, where it is declared.
		}

		CNewMachine::~CNewMachine()
		{
		}

		void CNewMachine::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_CHECK_ALLOW, m_Allow);
			DDX_Control(pDX, IDC_NAMELABEL, m_nameLabel);
			DDX_Control(pDX, IDC_BROWSER, m_browser);
			DDX_Control(pDX, IDC_VERSIONLABEL, m_versionLabel);
			DDX_Control(pDX, IDC_DESCRLABEL, m_descLabel);
			DDX_Radio(pDX, IDC_BYTYPE, m_orderby);
			DDX_Control(pDX, IDC_DLLNAMELABEL, m_dllnameLabel);
			DDX_Radio(pDX, IDC_SHOWDLLNAME, m_showdllName);
			DDX_Control(pDX, IDC_LOAD_NEW_BLITZ, m_LoadNewBlitz);
			DDX_Control(pDX, IDC_APIVERSIONLABEL, m_APIversionLabel);
		}

/*
		BEGIN_MESSAGE_MAP(CNewMachine, CDialog)
			ON_NOTIFY(TVN_SELCHANGED, IDC_BROWSER, OnSelchangedBrowser)
			ON_BN_CLICKED(IDC_REFRESH, OnRefresh)
			ON_BN_CLICKED(IDC_BYCLASS, OnByclass)
			ON_BN_CLICKED(IDC_BYTYPE, OnBytype)
			ON_NOTIFY(NM_DBLCLK, IDC_BROWSER, OnDblclkBrowser)
			ON_WM_DESTROY()
			ON_BN_CLICKED(IDC_SHOWDLLNAME, OnShowdllname)
			ON_BN_CLICKED(IDC_LOAD_NEW_BLITZ, OnLoadNewBlitz)
			ON_BN_CLICKED(IDC_SHOWEFFNAME, OnShoweffname)
			ON_BN_CLICKED(IDC_CHECK_ALLOW, OnCheckAllow)
			ON_BN_CLICKED(IDC_SCAN_NEW, OnScanNew)
		END_MESSAGE_MAP()
*/

		BOOL CNewMachine::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			if(imgList.GetSafeHandle()) imgList.DeleteImageList();
			imgList.Create(IDB_MACHINETYPE,16,2,1);
			m_browser.SetImageList(&imgList,TVSIL_NORMAL);
			bAllowChanged = false;
			if (!Global::machineload().IsLoaded()) Global::machineload().LoadPluginInfo();
			UpdateList();
			m_LoadNewBlitz.SetCheck(Global::configuration().LoadsNewBlitz());
			return TRUE;
		}

		void CNewMachine::OnDestroy() 
		{
			CDialog::OnDestroy();
			if(imgList.GetSafeHandle()) imgList.DeleteImageList();
		}

		void CNewMachine::UpdateList(bool bInit)
		{
			//int nodeindex;
			m_browser.DeleteAllItems();
			HTREEITEM intFxNode;

			const int  _numPlugins = static_cast<PluginCatcher*>(&Global::machineload())->_numPlugins;
			PluginInfo** const _pPlugsInfo = static_cast<PluginCatcher*>(&Global::machineload())->_pPlugsInfo;
			
			if(!pluginOrder)
			{
				hNodes[0] = m_browser.InsertItem("Internal machines",0,0 , TVI_ROOT, TVI_LAST);
				hNodes[1] = m_browser.InsertItem("Psycle Host machines",2,2,TVI_ROOT,TVI_LAST);
				hNodes[2] = m_browser.InsertItem("VST 2.x Host machines",4,4,TVI_ROOT,TVI_LAST);
				hNodes[3] = m_browser.InsertItem("Lua script machines",6,6,TVI_ROOT,TVI_LAST);
				hNodes[4] = m_browser.InsertItem("LADSPA Host machines",8,8,TVI_ROOT,TVI_LAST);
				hNodes[5] = m_browser.InsertItem("Non-working or not a machine",10,10,TVI_ROOT,TVI_LAST);
				intFxNode = hNodes[0];
				//nodeindex = 4;
				//The following is unfinished. It is for nested branches.
				/*
				int i=_numPlugins;	// I Search from the end because when creating the array, the deepest dir is saved first.
				HTREEITEM curnode;
				int currdir = numDirs;
				while (i>=0)
				{
					if ( strcpy(_pPlugsInfo[i]->_pPlugsInfo[i]->dllname,dirArray[currdir]) != 0 )
					{
						currdir--:
						// check if you need to create a new node or what.
						// use m_browser.GetNextItem() to check where you are.
					}
					// do what it is under here, but with the correct "hNodes";

				}
				*/
				for(int i(_numPlugins - 1) ; i >= 0 ; --i)
				{
					int imgindex;
					HTREEITEM hitem;
					PluginInfo* pInfo = _pPlugsInfo[i];
					if ( pInfo->error.empty())
					{
						if( pInfo->mode == MACHMODE_HOST) {
							continue;
						}
						else if( pInfo->mode == MACHMODE_GENERATOR)
						{
							if( pInfo->type == MACH_PLUGIN) 
							{ 
								imgindex = 2; 
								hitem= hNodes[1]; 
							}
							else if ( pInfo->type == MACH_LUA) 
							{ 
								imgindex = 6; 
								hitem=hNodes[3]; 
							} else if ( pInfo->type == MACH_LADSPA) 
							{ 
								imgindex = 8; 
								hitem=hNodes[4]; 
							}
							else {  // VST
								imgindex = 4; 
								hitem=hNodes[2]; 
							}
						}
						else
						{
							if( pInfo->type == MACH_PLUGIN) 
							{ 
								imgindex = 3; 
								hitem= hNodes[1];
							}
							else if ( pInfo->type == MACH_LUA) 
							{ 
								imgindex = 7; 
								hitem=hNodes[3]; 
							} else if ( pInfo->type == MACH_LADSPA) 
							{ 
								imgindex = 9; 
								hitem=hNodes[4]; 
							}
							else 
							{   // VST
								imgindex = 5; 
								hitem=hNodes[2];
							}
						}
					}
					else
					{
						imgindex = 10;
						hitem=hNodes[5];
					}
					if( pInfo->mode != MACHMODE_HOST) {
						if(pluginName && pInfo->error.empty()) {
							hPlug[i] = m_browser.InsertItem(pInfo->name.c_str(), imgindex, imgindex, hitem, TVI_SORT);
						}
						else {
							boost::filesystem::path path(pInfo->dllname);
							if (path.filename() != "blwtbl.dll"
									&& path.filename() != "fluidsynth.dll"
									&& path.filename() != "asio.dll"
									&& path.filename() != "msvcp71.dll"
									&& path.filename() != "msvcr71.dll"
									&& path.filename() != "msvcp80.dll"
									&& path.filename() != "msvcr80.dll") {
								hPlug[i] = m_browser.InsertItem(pInfo->dllname.c_str(), imgindex, imgindex, hitem, TVI_SORT);
							}
						}
					}
				}
				hInt[0] = m_browser.InsertItem("Sampler",0, 0, hNodes[0], TVI_SORT);
				hInt[1] = m_browser.InsertItem("Dummy plug",1,1,intFxNode,TVI_SORT);
				hInt[2] = m_browser.InsertItem("Sampulse",0, 0, hNodes[0], TVI_SORT);
				hInt[3] = m_browser.InsertItem("Note Duplicator",0, 0, hNodes[0], TVI_SORT);
				hInt[4] = m_browser.InsertItem("Send-Return Mixer",1, 1, intFxNode, TVI_SORT);
				hInt[5] = m_browser.InsertItem("Wave In Recorder",0, 0, hNodes[0], TVI_SORT);
				hInt[6] = m_browser.InsertItem("Note Duplicator2",0, 0, hNodes[0], TVI_SORT);
				m_browser.Select(hNodes[selectedClass],TVGN_CARET);
			}
			else
			{
				hNodes[0] = m_browser.InsertItem("Sound generators (Instruments)",0,0 , TVI_ROOT, TVI_LAST);
				hNodes[1] = m_browser.InsertItem("Sound Effects (DSP)",1,1,TVI_ROOT,TVI_LAST);
				hNodes[2] = m_browser.InsertItem("Non-working or not a machine",10,10,TVI_ROOT,TVI_LAST);
				intFxNode = hNodes[1];
				//nodeindex=2;
				for(int i(_numPlugins - 1) ; i >= 0 ; --i) // I Search from the end because when creating the array, the deepest dir comes first.
				{
					int imgindex;
					HTREEITEM hitem;
					PluginInfo* pInfo = _pPlugsInfo[i];
					if(pInfo->error.empty())
					{
						if(pInfo->mode == MACHMODE_GENERATOR)
						{
							if(pInfo->type == MACH_PLUGIN) 
							{ 
								imgindex = 2; 
								hitem= hNodes[0]; 
							}
							else if ( pInfo->type == MACH_LUA) 
							{ 
								  imgindex = 6; 
								  hitem=hNodes[0]; 
							} else if ( pInfo->type == MACH_LADSPA) 
							{ 
								imgindex = 8; 
								hitem=hNodes[0]; 
							}
							else 
							{  // VST
								imgindex = 4; 
								hitem=hNodes[0]; 
							}
						}
						else
						{
							if(pInfo->type == MACH_PLUGIN) 
							{ 
								imgindex = 3; 
								hitem= hNodes[1]; 
							}
							else if ( pInfo->type == MACH_LUA) 
							{ 
								  imgindex = 7; 
								  hitem=hNodes[1]; 
							} else if ( pInfo->type == MACH_LADSPA) 
							{ 
								imgindex = 9; 
								hitem=hNodes[1]; 
							}
							else 
							{   // VST
								imgindex = 5; 
								hitem=hNodes[1]; 
							}
						}
					}
					else
					{
						imgindex = 10;
						hitem=hNodes[2];
					}
					if( pInfo->mode != MACHMODE_HOST) {
						if(pluginName && pInfo->error.empty()) {
							hPlug[i] = m_browser.InsertItem(pInfo->name.c_str(), imgindex, imgindex, hitem, TVI_SORT);
						}
						else {
							boost::filesystem::path path(pInfo->dllname);
							if (path.filename() != "blwtbl.dll"
									&& path.filename() != "fluidsynth.dll"
									&& path.filename() != "asio.dll"
									&& path.filename() != "msvcp71.dll"
									&& path.filename() != "msvcr71.dll"
									&& path.filename() != "msvcp80.dll"
									&& path.filename() != "msvcr80.dll") {
								hPlug[i] = m_browser.InsertItem(pInfo->dllname.c_str(), imgindex, imgindex, hitem, TVI_SORT);
							}
						}
					}
				}
				hInt[0] = m_browser.InsertItem("Sampler",0, 0, hNodes[0], TVI_SORT);
				hInt[1] = m_browser.InsertItem("Dummy plug",1,1,intFxNode,TVI_SORT);
				hInt[2] = m_browser.InsertItem("Sampulse",0, 0, hNodes[0], TVI_SORT);
				hInt[3] = m_browser.InsertItem("Note Duplicator",0, 0, hNodes[0], TVI_SORT);
				hInt[4] = m_browser.InsertItem("Send-Return Mixer",1, 1, intFxNode, TVI_SORT);
				hInt[5] = m_browser.InsertItem("Wave In Recorder",0, 0, hNodes[0], TVI_SORT);
				hInt[6] = m_browser.InsertItem("Note Duplicator2",0, 0, hNodes[0], TVI_SORT);
				m_browser.Select(hNodes[selectedMode],TVGN_CARET);
			}
			Outputmachine = -1;
		}

		void CNewMachine::OnSelchangedBrowser(NMHDR* pNMHDR, LRESULT* pResult) 
		{
			const int  _numPlugins = static_cast<PluginCatcher*>(&Global::machineload())->_numPlugins;
			PluginInfo** const _pPlugsInfo = static_cast<PluginCatcher*>(&Global::machineload())->_pPlugsInfo;
			NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR; pNMTreeView; // not used
			tHand = m_browser.GetSelectedItem();
			Outputmachine = -1;
			if (tHand == hInt[0])
			{
				m_nameLabel.SetWindowText("Sampler");
				m_descLabel.SetWindowText("Stereo Sampler Unit. Inserts new sampler.");
				m_dllnameLabel.SetWindowText("Internal Machine");
				m_versionLabel.SetWindowText("V1.0");
				m_APIversionLabel.SetWindowText("Internal");
				Outputmachine = MACH_SAMPLER;
				selectedClass = internal;
				selectedMode = modegen;
				m_Allow.SetCheck(FALSE);
				m_Allow.EnableWindow(FALSE);
			}
			else if (tHand == hInt[1])
			{
				m_nameLabel.SetWindowText("Dummy");
				m_descLabel.SetWindowText("Replaces non-existant plugins");
				m_dllnameLabel.SetWindowText("Internal Machine");
				m_versionLabel.SetWindowText("V1.0");
				m_APIversionLabel.SetWindowText("Internal");
				Outputmachine = MACH_DUMMY;
				selectedClass = internal;
				selectedMode = modefx;
				m_Allow.SetCheck(FALSE);
				m_Allow.EnableWindow(FALSE);
			}
			else if (tHand == hInt[2])
				{
				m_nameLabel.SetWindowText("Sampulse Sampler V2");
				m_descLabel.SetWindowText("Sampler with the essence of FastTracker II and Impulse Tracker 2");
				m_dllnameLabel.SetWindowText("Internal Machine");
				m_versionLabel.SetWindowText("V1.1");
				m_APIversionLabel.SetWindowText("Internal");
				Outputmachine = MACH_XMSAMPLER;
				selectedClass = internal;
				selectedMode = modegen;
				m_Allow.SetCheck(FALSE);
				m_Allow.EnableWindow(FALSE);
				}
			else if (tHand == hInt[3])
			{
				m_nameLabel.SetWindowText("Note Duplicator");
				m_descLabel.SetWindowText("Repeats the Events received to the selected machines");
				m_dllnameLabel.SetWindowText("Internal Machine");
				m_versionLabel.SetWindowText("V1.0");
				m_APIversionLabel.SetWindowText("Internal");
				Outputmachine = MACH_DUPLICATOR;
				selectedClass = internal;
				selectedMode = modegen;
				m_Allow.SetCheck(FALSE);
				m_Allow.EnableWindow(FALSE);
			}
			else if (tHand == hInt[4])
			{
				m_nameLabel.SetWindowText("Send-Return Mixer");
				m_descLabel.SetWindowText("Allows to mix the audio with a typical mixer table, with send/return effects");
				m_dllnameLabel.SetWindowText("Internal Machine");
				m_versionLabel.SetWindowText("V1.0");
				m_APIversionLabel.SetWindowText("Internal");
				Outputmachine = MACH_MIXER;
				selectedClass = internal;
				selectedMode = modefx;
				m_Allow.SetCheck(FALSE);
				m_Allow.EnableWindow(FALSE);
			}
			else if (tHand == hInt[5])
			{
				m_nameLabel.SetWindowText("Wave In Recorder");
				m_descLabel.SetWindowText("Allows Psycle to get audio from an external source");
				m_dllnameLabel.SetWindowText("Internal Machine");
				m_versionLabel.SetWindowText("V1.1");
				m_APIversionLabel.SetWindowText("Internal");
				Outputmachine = MACH_RECORDER;
				selectedClass = internal;
				selectedMode = modegen;
				m_Allow.SetCheck(FALSE);
				m_Allow.EnableWindow(FALSE);
			} else if (tHand == hInt[6]) 
			{
				m_nameLabel.SetWindowText("Note Duplicator 2");
				m_descLabel.SetWindowText("Repeats and/or Splits the Events received to the selected machines");
				m_dllnameLabel.SetWindowText("Internal Machine");
				m_versionLabel.SetWindowText("V1.0");
				m_APIversionLabel.SetWindowText("Internal");
				Outputmachine = MACH_DUPLICATOR2;
				selectedClass = internal;
				selectedMode = modegen;
				m_Allow.SetCheck(FALSE);
				m_Allow.EnableWindow(FALSE);
			}
			else for (int i=0; i<_numPlugins; i++)
			{
				PluginInfo* pInfo = _pPlugsInfo[i];
				if (tHand == hPlug[i])
				{
					{ //  Strip the directory and put just the dll name.
						std::string str = pInfo->dllname;
						std::string::size_type pos = str.rfind('\\');
						if(pos != std::string::npos)
							str=str.substr(pos+1);
						m_dllnameLabel.SetWindowText(str.c_str());
					}
					m_nameLabel.SetWindowText(pInfo->name.c_str());
					if ( pInfo->error.empty())
						m_descLabel.SetWindowText(pInfo->desc.c_str());
					else
					{	// Strip the function, and show only the error.
						std::string str = pInfo->error;
						std::ostringstream s; s << std::endl;
						std::string::size_type pos = str.find(s.str());
						if(pos != std::string::npos)
							str=str.substr(pos+1);

						m_descLabel.SetWindowText(str.c_str());
					}
					if(pInfo->type == MACH_PLUGIN)
					{
						m_versionLabel.SetWindowText(pInfo->version.c_str());
					} 
					else if(pInfo->type == MACH_LUA)
					{
						m_versionLabel.SetWindowText(pInfo->version.c_str());
					} 
					else if(pInfo->type == MACH_LADSPA)					
					{
						m_versionLabel.SetWindowText(pInfo->version.c_str());
					} 
					else
					{	// convert integer to string.
						std::ostringstream s;
						std::istringstream s2(pInfo->version);
						int iver = 0;
						s2 >> iver;
						s << pInfo->version << " or " << std::hex << iver;
						m_versionLabel.SetWindowText(s.str().c_str());
					}
					{	// convert integer to string.
						std::ostringstream s;
						if(pInfo->type == MACH_PLUGIN  && pInfo->APIversion > 0x10) {
							s << std::hex << (pInfo->APIversion&0x7FFF);
						}
						else {
							 s << pInfo->APIversion;
						}
						m_APIversionLabel.SetWindowText(s.str().c_str());
					}
					Outputmachine = pInfo->type;
					selectedMode = ( pInfo->mode == MACHMODE_GENERATOR) ? modegen : modefx;

					if ( pInfo->type == MACH_PLUGIN )
					{
						selectedClass = native;
					}
					else if ( pInfo->type == MACH_LUA )
					{
						selectedClass = luascript;
					}
					else if ( pInfo->type == MACH_LADSPA )
					{
						selectedClass = ladspa;
					}
					else
					{
						selectedClass = vstmac;
						Outputmachine = ( pInfo->mode == MACHMODE_GENERATOR) ? MACH_VST : MACH_VSTFX;
					}

					shellIdx = pInfo->identifier;
					psOutputDll = pInfo->dllname;

					m_Allow.SetCheck(!pInfo->allow);
					m_Allow.EnableWindow(TRUE);
				}
			}
			*pResult = 0;
		}

		void CNewMachine::OnDblclkBrowser(NMHDR* pNMHDR, LRESULT* pResult) 
		{
			OnOK();	
			*pResult = 0;
		}

		void CNewMachine::OnRefresh() 
		{
			::AfxGetApp()->DoWaitCursor(1); 
			Global::machineload().ReScan(true);
			::AfxGetApp()->DoWaitCursor(-1); 
			UpdateList();
			m_browser.Invalidate();
			m_browser.SetFocus();
		}
		void CNewMachine::OnScanNew()
		{
			::AfxGetApp()->DoWaitCursor(1); 
			Global::machineload().ReScan(false);
			::AfxGetApp()->DoWaitCursor(-1); 
			UpdateList();
			m_browser.Invalidate();
			m_browser.SetFocus();
		}

		void CNewMachine::OnBytype() 
		{
			pluginOrder=0;
			UpdateList();
			m_browser.Invalidate();
		}
		void CNewMachine::OnByclass() 
		{
			pluginOrder=1;
			UpdateList();
			m_browser.Invalidate();
		}

		void CNewMachine::OnShowdllname() 
		{
			pluginName=false;	
			UpdateList();
			m_browser.Invalidate();
		}

		void CNewMachine::OnShoweffname() 
		{
			pluginName = true;
			UpdateList();
			m_browser.Invalidate();
		}
		void CNewMachine::OnLoadNewBlitz()
		{
			Global::configuration().LoadNewBlitz(m_LoadNewBlitz.GetCheck());
		}

		void CNewMachine::OnOK() 
		{
			if (Outputmachine > -1) // Necessary so that you cannot doubleclick a Node
			{
				if (bAllowChanged)
				{
					static_cast<PluginCatcher*>(&Global::machineload())->SaveCacheFile();
				}
				CDialog::OnOK();
			}
		}

		void CNewMachine::OnCheckAllow() 
		{
			const int  _numPlugins = static_cast<PluginCatcher*>(&Global::machineload())->_numPlugins;
			PluginInfo** const _pPlugsInfo = static_cast<PluginCatcher*>(&Global::machineload())->_pPlugsInfo;
			for (int i=0; i<_numPlugins; i++)
			{
				if (tHand == hPlug[i])
				{
					_pPlugsInfo[i]->allow = !m_Allow.GetCheck();
					bAllowChanged = TRUE;
				}
			}
		}
	}   // namespace
}   // namespace
