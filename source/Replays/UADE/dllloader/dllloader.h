#pragma once
#include "atlcore.h"                    //AtlLoadSystemLibraryUsingFullPath
#include "atltransactionmanager.h"
#include <string>
#include <functional>
#include <map>
#include <string>


/*
#ifdef DLLLOADER_EXPORT
    #define DLLLOADER_ENTRY __declspec(dllexport)
#else
    #define DLLLOADER_ENTRY __declspec(dllimport)
#endif
*/
#define DLLLOADER_ENTRY

class DLLLOADER_ENTRY DllManager
{
public:
    DllManager();
    ~DllManager();

    bool IsEnabled() const;

    bool EnableDllRedirection();
    void DisableDllRedirection();

    std::function<std::wstring()> GetLastError;

    // Same as LoadLibrary, except loads .dll into ram avoiding lock to file.
    // returns 0 if failure, DllManager::GetLastError() can be used to retrieve error message.
    HMODULE RamLoadLibrary(const wchar_t* path);

    /// Sets dll contents for specific path
    bool SetDllFile(const wchar_t* path, const void* dll, size_t size);
    void UnsetDllFile(const wchar_t* path);
    
    HMODULE LoadLibrary(const wchar_t* path);

    ATL::CAtlTransactionManager transactionManager;
    std::map<std::wstring, HANDLE, std::less<> > path2handle;
    std::map<HANDLE, std::wstring> handle2path;

    bool WinApiCall(bool cond);

private:
    bool MhCall(int r);
    bool hooksEnabled;
};



