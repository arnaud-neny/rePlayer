#include "dllloader.h"
#include <filesystem>		//temp_directory_path
#include "MinHook.h"
#define _NTDLL_SELF_		//No need for ntdll.dll linkage
#include "ntddk.h"
#include <atlconv.h>		//CA2W
#include <atlutil.h>		//AtlGetErrorDescription
#include <iostream>         //ifstream
#include <fstream>

using namespace ATL;
using namespace std;
using namespace std::filesystem;

static DllManager* g_dllmanager = nullptr;

//-------------------------------------------------------------------------------------------------------------
using NtOpenFile_pfunc = NTSTATUS(NTAPI*)
	(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, 
	 PIO_STATUS_BLOCK IoStatusBlock, ULONG ShareAccess, ULONG OpenOptions);

NtOpenFile_pfunc NtOpenFile_origfunc;

//
//	Gets accociated file handle for specific filename.
//	If not hooked, then return 0.
//
HANDLE GetAccociatedHandle(POBJECT_ATTRIBUTES ObjectAttributes)
{
	if (g_dllmanager != nullptr && ObjectAttributes != nullptr)
	{
		wstring_view ntFileName(ObjectAttributes->ObjectName->Buffer, ObjectAttributes->ObjectName->Length / sizeof(wchar_t));
#if __cplusplus >= 202002L
		if (ntFileName.starts_with(L"\\??\\"))
#else
		if (ntFileName.rfind(L"\\??\\", 0) == 0)
#endif
		{
			auto fileName = ntFileName.substr(4);
			auto it = g_dllmanager->path2handle.find(fileName);
			if (it != g_dllmanager->path2handle.end())
			{
				return it->second;
			}
		}
	}

	return 0;
}

NTSTATUS WINAPI NtOpenFile_detour(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock, ULONG ShareAccess, ULONG OpenOptions)
{
	HANDLE h = GetAccociatedHandle(ObjectAttributes);
	if( h != 0 )
	{
		*FileHandle = h;
		return STATUS_SUCCESS;
	}
	
	NTSTATUS r = NtOpenFile_origfunc(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, ShareAccess, OpenOptions);
	return r;
}

//-------------------------------------------------------------------------------------------------------------

using NtCreateSection_pfunc = NTSTATUS(NTAPI*)
(PHANDLE SectionHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes,
	PLARGE_INTEGER MaximumSize, ULONG SectionPageProtection, ULONG AllocationAttributes, HANDLE FileHandle);

NtCreateSection_pfunc NtCreateSection_origfunc;

NTSTATUS NTAPI NtCreateSection_detour(PHANDLE SectionHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes,
	PLARGE_INTEGER MaximumSize, ULONG SectionPageProtection, ULONG AllocationAttributes, HANDLE FileHandle)
{
	if (g_dllmanager != nullptr &&
#if __cplusplus >= 202002L
		g_dllmanager->handle2path.contains(FileHandle)
#else
		g_dllmanager->handle2path.find(FileHandle) != g_dllmanager->handle2path.end()
#endif
	)
	{
		return NtCreateSection_origfunc(SectionHandle, DesiredAccess, ObjectAttributes, MaximumSize, PAGE_READONLY, AllocationAttributes, FileHandle);
	}

	return NtCreateSection_origfunc(SectionHandle, DesiredAccess, ObjectAttributes, MaximumSize, SectionPageProtection, AllocationAttributes, FileHandle);
}

//-------------------------------------------------------------------------------------------------------------
using NtQueryAttributesFile_pfunc = NTSTATUS(NTAPI*)(POBJECT_ATTRIBUTES ObjectAttributes, PFILE_BASIC_INFORMATION FileInformation);

NtQueryAttributesFile_pfunc  NtQueryAttributesFile_origfunc;

NTSTATUS NTAPI NtQueryAttributesFile_detour(POBJECT_ATTRIBUTES ObjectAttributes, PFILE_BASIC_INFORMATION FileInformation)
{
	// Since .dll does not physically does not exists on disk - we need to intercept NtQueryAttributesFile
	// so it would report like file does exists.
	HANDLE h = GetAccociatedHandle(ObjectAttributes);
	if (h != 0)
	{
		if(FileInformation)
		{
			*FileInformation = {0};		// Don't care about .dll date/timestamps (probably unused)
			FileInformation->FileAttributes = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE;
		}

		return STATUS_SUCCESS;
	}

	return NtQueryAttributesFile_origfunc(ObjectAttributes, FileInformation);
}

//-------------------------------------------------------------------------------------------------------------
using NtClose_pfunc = NTSTATUS(NTAPI*) (HANDLE Handle);

NtClose_pfunc NtClose_origfunc;

NTSTATUS NTAPI NtClose_detour(HANDLE Handle)
{
	if (g_dllmanager != nullptr &&
#if __cplusplus >= 202002L
		g_dllmanager->handle2path.contains(Handle)
#else
		g_dllmanager->handle2path.find(Handle) != g_dllmanager->handle2path.end()
#endif
		)
	{
		return 0;
	}
		
	auto r = NtClose_origfunc(Handle);
	return r;
}

//-------------------------------------------------------------------------------------------------------------


DllManager::DllManager():
    transactionManager(false, true), hooksEnabled(false)
{
	GetLastError = []() -> wstring{ return L""; };
	g_dllmanager = this;
}

bool DllManager::IsEnabled() const
{
	return hooksEnabled;
}

DllManager::~DllManager()
{
	g_dllmanager = nullptr;

	DisableDllRedirection();

	for (auto const& [h, path] : handle2path)
		CloseHandle(h);

	handle2path.clear();
	path2handle.clear();

	transactionManager.Rollback();
}

bool DllManager::MhCall(int r)
{
	if (r != MH_OK)
	{
		GetLastError = [r]() -> wstring
		{
			return wstring(L"MinHook error: ") + CA2W(MH_StatusToString((MH_STATUS)r)).operator LPWSTR();
		};
		return false;
	}

	return true;
}

const wchar_t* ntdll_dll = L"ntdll.dll";

bool DllManager::EnableDllRedirection()
{
	HMODULE ntdll = GetModuleHandle(ntdll_dll);
	if(!MhCall(MH_Initialize()))
	{
		return false;
	}

	bool b = MhCall(MH_CreateHookApi(ntdll_dll, "NtOpenFile", &NtOpenFile_detour, (LPVOID*)&NtOpenFile_origfunc));
	b &= MhCall(MH_CreateHookApi(ntdll_dll, "NtClose", &NtClose_detour, (LPVOID*)&NtClose_origfunc));
	b &= MhCall(MH_CreateHookApi(ntdll_dll, "NtCreateSection", &NtCreateSection_detour, (LPVOID*)&NtCreateSection_origfunc));
	b &= MhCall(MH_CreateHookApi(ntdll_dll, "NtQueryAttributesFile", &NtQueryAttributesFile_detour, (LPVOID*)&NtQueryAttributesFile_origfunc));
	
	if(b)
	{
		b &= MhCall(MH_EnableHook((LPVOID)GetProcAddress(ntdll, "NtOpenFile")));
		b &= MhCall(MH_EnableHook((LPVOID)GetProcAddress(ntdll, "NtClose")));
		b &= MhCall(MH_EnableHook((LPVOID)GetProcAddress(ntdll, "NtCreateSection")));
		b &= MhCall(MH_EnableHook((LPVOID)GetProcAddress(ntdll, "NtQueryAttributesFile")));

		hooksEnabled = b;
		if(!b)
		{
			DisableDllRedirection();
		}
	}

	return b;
}

void DllManager::DisableDllRedirection()
{
	HMODULE ntdll = GetModuleHandle(ntdll_dll);

	MH_DisableHook((LPVOID)GetProcAddress(ntdll, "NtOpenFile"));
	MH_DisableHook((LPVOID)GetProcAddress(ntdll, "NtClose"));
	MH_DisableHook((LPVOID)GetProcAddress(ntdll, "NtCreateSection"));

	MH_Uninitialize();
}


bool DllManager::WinApiCall(bool cond)
{
	if(cond)
	{
		return true;
	}
	
	DWORD errorCode = ::GetLastError();
	if (errorCode != 0)
	{
		GetLastError = [errorCode]() -> wstring
		{
			return AtlGetErrorDescription(HRESULT_FROM_WIN32(errorCode)).GetBuffer();
		};
		return false;
	}

	return true;
}

HMODULE DllManager::RamLoadLibrary(const wchar_t* dll_path)
{
	string dllBinary;

	path currentPath(dll_path);
	path newDllPath(temp_directory_path() / currentPath.filename());

	if (!exists(dll_path))
	{
		GetLastError = [dll_path]() -> wstring
		{
			return wstring(L"Dll does not exists: ") + dll_path;
		};
		return 0;
	}

	ifstream is;
	is.exceptions(is.exceptions() | std::ios::failbit);   //throw exception on failure

	try {
		is.open(dll_path, ios::binary);
		is.seekg(0, ios::end);
		int size = is.tellg();
		is.seekg(0, ios::beg);
		dllBinary.resize(size);
		is.read(&dllBinary[0], size);
		is.close();
	}
	catch (ios_base::failure& e)
	{
		wstring what(CA2W(e.what()));
		GetLastError = [what]() -> wstring
		{
			return what;
		};
		return 0;
	}

	if (!SetDllFile(newDllPath.c_str(), &dllBinary[0], dllBinary.size()))
	{
		return 0;
	}

	return LoadLibraryA(newDllPath.c_str());
}


bool DllManager::SetDllFile(const wchar_t* path, const void* dll, size_t size)
{
	HANDLE hFile = transactionManager.CreateFileW(path, GENERIC_WRITE | GENERIC_READ, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if(!WinApiCall(hFile != 0))
	{
		return false;	
	}

	DWORD written = 0;
	bool b = WinApiCall(WriteFile(hFile, dll, size, &written, NULL));
	b &= WinApiCall(FlushFileBuffers(hFile));
	b &= WinApiCall(SetFilePointer(hFile, 0, 0, FILE_BEGIN) != INVALID_SET_FILE_POINTER);
	if(b)
	{
		handle2path[hFile] = path;
		path2handle[path] = hFile;
	}
	else
	{
		CloseHandle(hFile);
		transactionManager.DeleteFileW(path);
	}

	return true;
}

void DllManager::UnsetDllFile(const wchar_t* path)
{
	auto hFile = path2handle[path];
	handle2path.erase(hFile);
	path2handle.erase(path);
	CloseHandle(hFile);
    transactionManager.DeleteFileW(path);
}

HMODULE DllManager::LoadLibraryA(const wchar_t* path)
{
	HMODULE h = ::LoadLibrary(path);
	WinApiCall(h != 0);
	return h;
}


