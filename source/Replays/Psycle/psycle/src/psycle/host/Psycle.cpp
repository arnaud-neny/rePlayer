///\file
///\brief implementation file for psycle::host::CPsycleApp.
#include <psycle/host/detail/project.private.hpp>
#include "Psycle.hpp"
#include "PsycleConfig.hpp"
#include "SInstance.h"
#include "ConfigDlg.hpp"
#include "MainFrm.hpp"

#include "AudioDriver.hpp"
#include "MidiInput.hpp"
#include "Player.hpp"
#include <psycle/host/machineloader.hpp>
#include "Machine.hpp"

#include <universalis/cpu/exception.hpp>
#include <universalis/os/loggers.hpp>
#include <diversalis/compiler.hpp>

#include <universalis/os/terminal.hpp>

#include "Ui.hpp"

#if defined COMPILE_WITH_VLD
// Visual Leak Detector ( vld.codeplex.com )
#include <vld.h>
#endif

#include <sstream>

#define _WIN32_DCOM
#include <comdef.h>

#include <wbemidl.h>
#if defined DIVERSALIS__COMPILER__FEATURE__AUTO_LINK
	# pragma comment(lib, "wbemuuid")
#endif

namespace psycle { namespace host {
    
	  
/*
		BEGIN_MESSAGE_MAP(CPsycleApp, CWinAppEx)
			ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
		END_MESSAGE_MAP()
*/

		CPsycleApp theApp; /// The one and only CPsycleApp object    

		static void MessageBoxCallback(const char* msg, const char* title)
		{
            MessageBox(NULL, msg, title, MB_OK);
		}

		CPsycleApp::CPsycleApp() : m_uUserMessage(0)
		{			
		   //	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );	
          // _CrtSetBreakAlloc( 129 );
			//_CrtDumpMemoryLeaks( );      
			theApp.global_.pLogCallback = MessageBoxCallback;
		}

		CPsycleApp::~CPsycleApp()
		{
		}

              
		BOOL CPsycleApp::InitInstance()			
		{           
			// InitCommonControlsEx() is required on Windows XP if an application
			// manifest specifies use of ComCtl32.dll version 6 or later to enable
			// visual styles.  Otherwise, any window creation will fail.
// 			INITCOMMONCONTROLSEX InitCtrls;
// 			InitCtrls.dwSize = sizeof(InitCtrls);
// 			// Set this to include all the common control classes you want to use
// 			// in your application.
// 			InitCtrls.dwICC = ICC_WIN95_CLASSES;
// 			InitCommonControlsEx(&InitCtrls);

// 			CWinAppEx::InitInstance();
                  
			// Allow only one instance of the program
// 			m_uUserMessage=RegisterWindowMessage("Psycle.exe_CommandLine");
			CInstanceChecker instanceChecker;

			//For the Bar states. Everything else is maintained in PsycleConf
			///\todo is it this line that causes the \HKCU\Software\psycle\Psycle\ path? (note the doubled "psycle" in the path)
// 			SetRegistryKey(_T(PSYCLE__NAME)); // Change the registry key under which our settings are stored.

			//Psycle maintains its own recent
			//LoadStdProfileSettings();  // Load standard INI file options (including MRU)

// 			loggers::information()("Build identifier: \n" PSYCLE__BUILD__IDENTIFIER("\n"));

			bool loaded = global_.conf().LoadPsycleSettings();
			if (loaded &&
				instanceChecker.PreviousInstanceRunning() &&
				!global_.conf()._allowMultipleInstances)
			{
// 				HWND prevWnd = instanceChecker.ActivatePreviousInstance();
// 				if(*(m_lpCmdLine) != 0)
// 				{
// 					::PostMessage(prevWnd,m_uUserMessage,reinterpret_cast<WPARAM>(m_lpCmdLine),0);
// 				}
				return FALSE;
			}     

			std::string sci_path = global_.conf().appPath();
			sci_path += "\\SciLexer.dll";
			m_hDll = LoadLibrary (_T (sci_path.c_str()));
			if (m_hDll == NULL) {
				ui::alert("LoadLibrary SciLexer.dll failure ...");
			}
      
			// To create the main window, this code creates a new frame window
			// object and then sets it as the application's main window object.
			CMainFrame* pFrame = new CMainFrame();
			if (!pFrame)
				return FALSE;
			m_pMainWnd = pFrame;
      
			if(!loaded) // problem reading registry info. missing or damaged
			{
				CConfigDlg dlg("Psycle Settings");
				if (dlg.DoModal() == IDOK)
				{
					pFrame->m_wndView._outputActive = true;
				}
			}
			else
			{
				pFrame->m_wndView._outputActive = true;
			}
			global_.conf().RefreshSettings();

			// create and load the frame with its resources
// 			pFrame->LoadFrame(IDR_MAINFRAME, WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, 0, 0);

			instanceChecker.TrackFirstInstanceRunning();

			// The one and only window has been initialized, so show and update it.
			pFrame->ShowWindow(SW_MAXIMIZE);
			
			pFrame->m_wndView.RestoreRecent();
			global_.conf().RefreshAudio();
			// center master machine. Cannot be done until here because it's when we have the window size.
			pFrame->m_wndView._pSong._pMachine[MASTER_INDEX]->_x=(pFrame->m_wndView.CW-pFrame->m_wndView.MachineCoords->sMaster.width)/2;
			pFrame->m_wndView._pSong._pMachine[MASTER_INDEX]->_y=(pFrame->m_wndView.CH-pFrame->m_wndView.MachineCoords->sMaster.width)/2;

			//Psycle maintains its own recent
			//LoadRecent(pFrame); // Import recent files from registry.
			pFrame->m_wndView.InitWindowMenu();
			if (*m_lpCmdLine)
				ProcessCmdLine(m_lpCmdLine); // Process Command Line
// 			else
// 			{
// 				global_.machineload().LoadPluginInfo(false);
// 				ui::Systems::instance().InitInstance(PsycleGlobal::configuration().GetAbsoluteLuaDir() + "\\psycle\\ui\\uiconfiguration.lua");        
// 				pFrame->m_wndView.LoadHostExtensions();
// 				// Show splash screen
// 				if (global_.conf()._showAboutAtStart)
// 				{
// 					OnAppAbout();
// 				}
// 				pFrame->CheckForAutosave();        
// 			}      
			          
			return TRUE;
		}

		int CPsycleApp::ExitInstance() 
		{      
			if (m_hDll != NULL) {
				FreeLibrary (m_hDll);
			}
			if(global_.conf()._pOutputDriver != NULL) {
				global_.conf()._pOutputDriver->Enable(false);
				global_.midi().Close();
				global_.player().stop_threads();
				global_.conf().SavePsycleSettings();
			}

			return CWinAppEx::ExitInstance();
		}

		BOOL CPsycleApp::PreTranslateMessage(MSG* pMsg)
		{
			if( pMsg->message == m_uUserMessage )
			{
				ProcessCmdLine(reinterpret_cast<LPSTR>(pMsg->wParam));
			}
			return CWinAppEx::PreTranslateMessage(pMsg);
		}

		/////////////////////////////////////////////////////////////////////////////
		// CPsycleApp message handlers


		// Returning false on WM_TIMER prevents the statusbar from being updated. So it's disabled for now.
		BOOL CPsycleApp::IsIdleMessage( MSG* pMsg )
		{
			if (!CWinAppEx::IsIdleMessage( pMsg ) || 
				pMsg->message == WM_TIMER) 
			{
//				return FALSE;
			}
//			else
				return TRUE;
		}

		BOOL CPsycleApp::OnIdle(LONG lCount)
		{
			BOOL bMore = CWinAppEx::OnIdle(lCount);            
			///\todo: 
			return bMore;
		}

		void CPsycleApp::ProcessCmdLine(LPSTR cmdline)
		{
			if (*(cmdline) != 0)
			{
				if(strcmp(cmdline,"/skipscan") != 0) {
					global_.machineload().LoadPluginInfo(false);
			    ((CMainFrame*)m_pMainWnd)->m_wndView.LoadHostExtensions();
					char tmpName [MAX_PATH];
					std::strncpy(tmpName, m_lpCmdLine+1, MAX_PATH-1 );
					tmpName[std::strlen(m_lpCmdLine+1) -1 ] = 0;
					static_cast<CMainFrame*>(m_pMainWnd)->m_wndView.FileLoadsongNamed(tmpName);
				}
			}
		}

		void CPsycleApp::OnAppAbout()
		{
			CAboutDlg dlg;
			dlg.DoModal();
		}

		bool CPsycleApp::BrowseForFolder(HWND hWnd_, char* title_, std::string& rpath)
		{

			///\todo: alternate browser window for Vista/7: http://msdn.microsoft.com/en-us/library/bb775966%28v=VS.85%29.aspx
			// SHCreateItemFromParsingName(
			bool val=false;
			
			LPMALLOC pMalloc;
			// Gets the Shell's default allocator
			//
/*
			if (::SHGetMalloc(&pMalloc) == NOERROR)
			{
				char pszBuffer[MAX_PATH];
				pszBuffer[0]='\0';
				BROWSEINFO bi;
				LPITEMIDLIST pidl;
				// Get help on BROWSEINFO struct - it's got all the bit settings.
				//
				bi.hwndOwner = hWnd_;
				bi.pidlRoot = NULL;
				bi.pszDisplayName = pszBuffer;
				bi.lpszTitle = title_;
				bi.ulFlags = BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
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
*/
			return val;
		}

		//////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////
		
		CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
		{
			//{{AFX_DATA_INIT(CAboutDlg)
			//}}AFX_DATA_INIT
		}

		void CAboutDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			//{{AFX_DATA_MAP(CAboutDlg)
			DDX_Control(pDX, IDC_ASIO, m_asio);
			DDX_Control(pDX, IDC_EDIT5, m_sourceforge);
			DDX_Control(pDX, IDC_EDIT2, m_psycledelics);
			DDX_Control(pDX, IDC_STEINBERGCOPY, m_steincopyright);
			DDX_Control(pDX, IDC_SHOWATSTARTUP, m_showabout);
			DDX_Control(pDX, IDC_HEADER, m_headercontrib);
			DDX_Control(pDX, IDC_ABOUTBMP, m_aboutbmp);
			DDX_Control(pDX, IDC_EDIT1, m_contrib);
			DDX_Control(pDX, IDC_VERSION_INFO_MULTI_LINE, m_versioninfo);
			//}}AFX_DATA_MAP
		}

/*
		BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
			//{{AFX_MSG_MAP(CAboutDlg)
			ON_BN_CLICKED(IDC_CONTRIBUTORS, OnContributors)
			ON_BN_CLICKED(IDC_SHOWATSTARTUP, OnShowatstartup)
			//}}AFX_MSG_MAP
		END_MESSAGE_MAP()
*/



		void CAboutDlg::OnContributors() 
		{
			if ( m_aboutbmp.IsWindowVisible() )
			{
				m_aboutbmp.ShowWindow(SW_HIDE);
				m_contrib.ShowWindow(SW_SHOW);
				m_headercontrib.ShowWindow(SW_SHOW);
				m_psycledelics.ShowWindow(SW_SHOW);
				m_sourceforge.ShowWindow(SW_SHOW);
				m_asio.ShowWindow(SW_SHOW);
				m_steincopyright.ShowWindow(SW_SHOW);
			}
			else 
			{
				m_aboutbmp.ShowWindow(SW_SHOW);
				m_contrib.ShowWindow(SW_HIDE);
				m_headercontrib.ShowWindow(SW_HIDE);
				m_psycledelics.ShowWindow(SW_HIDE);
				m_sourceforge.ShowWindow(SW_HIDE);
				m_asio.ShowWindow(SW_HIDE);
				m_steincopyright.ShowWindow(SW_HIDE);
			}
		}

		BOOL CAboutDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();

			m_contrib.SetWindowText
			(
				"Josep Mª Antolín. [JAZ]/JosepMa\tDeveloper since release 1.5" "\r\n"
				"Johan Boulé [bohan]\t\tDeveloper since release 1.7.3" "\r\n"
				"Stefan Nattkemper\t\t\tDeveloper of the LUA host in release 1.12" "\r\n"
				"James Redfern [alk]\t\tDeveloper and plugin coder" "\r\n"
				"Magnus Jonsson [zealmange]\t\tDeveloper during 1.7.x and 1.9alpha" "\r\n"
				"Jeremy Evers [pooplog]\t\tDeveloper in releases 1.7.x" "\r\n"
				"Daniel Arena\t\t\tDeveloper in release 1.5 & 1.6" "\r\n"
				"Marcin Kowalski [FideLoop]\t\tDeveloper in release 1.6" "\r\n"
				"Mark McCormack\t\t\tMIDI (in) Support in release 1.6" "\r\n"
				"Mats Höjlund\t\t\tMain Developer until release 1.5" "\r\n" // (Internal Recoding) .. doesn't fit in the small box :/
				"Juan Antonio Arguelles [Arguru] (RIP)\tOriginal Developer and maintainer until 1.0" "\r\n"
				"Satoshi Fujiwara\t\t\tBase code for the Sampulse machine\r\n"
				"Hermann Seib\t\t\tBase code for the new VST Host in 1.8.5\r\n"
				"Martin Etnestad Johansen [lobywang]\tCoding Help" "\r\n"
				"Patrick Haworth [TranceMyriad]\tAuthor of the Help File" "\r\n"
				"Budislav Stepanov\t\t\tNew artwork for version 1.11\r\n"
				"Angelus\t\t\t\tNew Skin for 1.10, example songs, beta tester" "\r\n"
				"ikkkle\t\t\t\tNew toolbar graphics for 1.10.0" "\r\n"
				"Hamarr Heylen\t\t\tInitial Graphics" "\r\n"
				"David Buist\t\t\tAdditional Graphics" "\r\n"
				"frown\t\t\t\tAdditional Graphics" "\r\n"
				"/\\/\\ark\t\t\t\tAdditional Graphics" "\r\n"
				"Michael Haralabos\t\t\tInstaller and Debugging help" "\r\n\r\n"
				"This release of Psycle also contains VST plugins from:" "\r\n"
				"Digital Fish Phones\t( http://www.digitalfishphones.com/ )" "\r\n"
				"DiscoDSP\t\t( http://www.discodsp.com/ )" "\r\n"
				"SimulAnalog\t( http://www.simulanalog.org/ )" "\r\n"
				"Jeroen Breebaart\t( http://www.jeroenbreebaart.com/ )" "\r\n"
				"George Yohng\t( http://www.yohng.com/ )" "\r\n"
				"Christian Budde\t( http://www.savioursofsoul.de/Christian/ )" "\r\n"
				"DDMF\t\t( http://www.ddmf.eu/ )" "\r\n"
				"Loser\t\t( http://loser.asseca.com/ )" "\r\n"
				"E-phonic\t\t( http://www.e-phonic.com/ )" "\r\n"
				"Argu\t\t( http://www.aodix.com/ )" "\r\n"
				"Oatmeal by Fuzzpilz\t( http://bicycle-for-slugs.org/ )"
			);
			m_showabout.SetCheck(PsycleGlobal::conf()._showAboutAtStart);

			m_psycledelics.SetWindowText("http://psycle.pastnotecut.org");
			m_sourceforge.SetWindowText("http://psycle.sourceforge.net");
			m_versioninfo.SetWindowText(PSYCLE__BUILD__IDENTIFIER("\r\n"));

			// return TRUE unless you set the focus to a control
			// EXCEPTION: OCX Property Pages should return FALSE
			return TRUE;
		}

		void CAboutDlg::OnShowatstartup() 
		{
			if ( m_showabout.GetCheck() )  PsycleGlobal::conf()._showAboutAtStart = true;
			else PsycleGlobal::conf()._showAboutAtStart=false;
		}

}}
