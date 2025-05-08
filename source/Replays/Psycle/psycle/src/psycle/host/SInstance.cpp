/*
Module : SINSTANCE.CPP
Purpose: Defines the implementation for an MFC wrapper classe
         to do instance checking
Created: PJN / 29-07-1998
History: PJN / 27-03-2000 1. Fixed a potential handle leak where the file handle m_hPrevInstance was not being
                          closed under certain circumstances.
                          Neville Franks made the following changes. Contact nevf@getsoft.com, www.getsoft.com
                          2. Split PreviousInstanceRunning() up into separate functions so we
                          can call it without needing the MainFrame window.
                          3. Changed ActivatePreviousInstance() to return hWnd.


Copyright (c) 1998 - 2000 by PJ Naughter.  
All rights reserved.

*/


/////////////////////////////////  Includes  //////////////////////////////////

#include <psycle/host/detail/project.private.hpp>
#include "SInstance.h"


/* Typical use instructions:

    CMyApp::InitInstance()
    {
      CInstanceChecker instanceChecker;

      if (instanceChecker.PreviousInstanceRunning())
      {
	      AfxMessageBox(_T("Previous version detected, will now restore it"), MB_OK);
        instanceChecker.ActivatePreviousInstance();
	      return FALSE;
      }

      ....

	    // create main MDI Frame window
      CMainFrame* pMainFrame = new CMainFrame;
      m_pMainWnd = pMainFrame;

      if (!pMainFrame->LoadFrame(IDR_MAINFRAME))
	      return FALSE;

      // If this is the first instance of our App then track it so any other instances can find it.
      if (!instanceChecker.PreviousInstanceRunning())
        instanceChecker.TrackFirstInstanceRunning();

      .....
    }
*/




///////////////////////////////// Implementation //////////////////////////////

//struct which is put into shared memory
struct CWindowInstance
{
  HWND hMainWnd;
};

//Class which be used as a static to ensure that we
//only close the file mapping at the very last chance
class _INSTANCE_DATA
{
public:
  _INSTANCE_DATA();
  ~_INSTANCE_DATA();

protected:
  HANDLE hInstanceData;
  friend class CInstanceChecker;
};

_INSTANCE_DATA::_INSTANCE_DATA()
{
  hInstanceData = NULL;
}

_INSTANCE_DATA::~_INSTANCE_DATA()
{
  if (hInstanceData != NULL)
  {
    ::CloseHandle(hInstanceData);
    hInstanceData = NULL;
  }
}

static _INSTANCE_DATA instanceData;


CInstanceChecker::CInstanceChecker()
{
  // Only one object of type CInstanceChecker should be created
  VERIFY(instanceData.hInstanceData == NULL);

  m_hPrevInstance = NULL;
}

CInstanceChecker::~CInstanceChecker()
{
  //Close the MMF if need be
  if (m_hPrevInstance)
  {
    ::CloseHandle(m_hPrevInstance);
    m_hPrevInstance = NULL;
  }
}

/** Track the first instance of our App.
 *  Call this after LoadFrame() in InitInstance(). Call PreviousInstanceRunning() first
 *  and only call this if it returns false.
 *
 *  @return TRUE on success, else FALSE - another instance is already running.
 */
BOOL CInstanceChecker::TrackFirstInstanceRunning()
{
//  VERIFY(PreviousInstanceRunning() == NULL);

  //If this is the first instance then copy in our info into the shared memory
  if (m_hPrevInstance == NULL)
  {
    //Create the MMF
    int nMMFSize = sizeof(CWindowInstance);
    instanceData.hInstanceData = ::CreateFileMapping((HANDLE)-1, NULL, PAGE_READWRITE, 0, nMMFSize, MakeMMFFilename());
    VERIFY(instanceData.hInstanceData != NULL); //Creating the MMF should work

    //Open the MMF
    CWindowInstance* pInstanceData = (CWindowInstance*) ::MapViewOfFile(instanceData.hInstanceData, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, nMMFSize);
    VERIFY(pInstanceData != NULL);   //Opening the MMF should work

    // Lock the data prior to updating it
    CSingleLock dataLock(&m_instanceDataMutex, TRUE);

    ASSERT(AfxGetMainWnd() != NULL); //Did you forget to set up the mainfrm in InitInstance ?
    ASSERT(AfxGetMainWnd()->GetSafeHwnd() != NULL);
    pInstanceData->hMainWnd = AfxGetMainWnd()->GetSafeHwnd();

    VERIFY(::UnmapViewOfFile(pInstanceData));
  }

  return (m_hPrevInstance == NULL);
}

/** Returns true if a previous instance of the App is running.
 *  Call this at the very start of InitInstance().
 *  @see ActivatePreviousInstance(), TrackFirstInstanceRunning().
 */
BOOL CInstanceChecker::PreviousInstanceRunning()
{
  ASSERT(m_hPrevInstance == NULL); //Trying to call PreviousInstanceRunning twice

  //Try to open the MMF first to see if we are the second instance
  m_hPrevInstance = ::OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, MakeMMFFilename());

  return (m_hPrevInstance != NULL);
}

CString CInstanceChecker::MakeMMFFilename()
{
  //MMF name is taken from MFC's AfxGetAppName
  LPCTSTR pszAppName = AfxGetAppName(); 
  ASSERT(pszAppName);
  ASSERT(_tcslen(pszAppName)); //Missing the Application Title ?
  CString sMMF(_T("CInstanceChecker_MMF_"));
  sMMF += pszAppName;
  return sMMF;
}

/** Activate the Previous Instance of our Application.
 *  @note Call PreviousInstanceRunning() before calling this function.
 *  @return hWnd of the previous instance's MainFrame if successful, else NULL.
 */
HWND CInstanceChecker::ActivatePreviousInstance()
{
  if (m_hPrevInstance != NULL) //Whats happened to my handle !
  {
    // Open up the MMF
    int nMMFSize = sizeof(CWindowInstance);
    CWindowInstance* pInstanceData = (CWindowInstance*) ::MapViewOfFile(m_hPrevInstance, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, nMMFSize);
    if (pInstanceData != NULL) //Opening the MMF should work
    {
      // Lock the data prior to reading from it
      CSingleLock dataLock(&m_instanceDataMutex, TRUE);

      //activate the old window
      HWND hWindow = pInstanceData->hMainWnd;
      CWnd wndPrev;
      wndPrev.Attach(hWindow);
      CWnd* pWndChild = wndPrev.GetLastActivePopup();

      if (wndPrev.IsIconic())
        wndPrev.ShowWindow(SW_RESTORE);

      pWndChild->SetForegroundWindow();

      //Detach the CWnd we were using
      wndPrev.Detach();

      //Unmap the MMF we were using
      VERIFY(::UnmapViewOfFile(pInstanceData));

      //Close the file handle now that we 
      ::CloseHandle(m_hPrevInstance);
      m_hPrevInstance = NULL;

      return hWindow;
    }
    else
      VERIFY(FALSE);  //Somehow MapViewOfFile failed.

    //Close the file handle now that we 
    ::CloseHandle(m_hPrevInstance);
    m_hPrevInstance = NULL;
  }
  else
    VERIFY(FALSE); //You should only call this function if PreviousInstanceRunning returned TRUE

  return NULL;
}


