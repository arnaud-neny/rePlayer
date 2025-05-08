#include <psycle/host/detail/project.private.hpp>
#include "JBridgeEnabler.hpp"

// Name of the proxy DLL to load
#define JBRIDGE_PROXY_REGKEY        TEXT("Software\\JBridge")

#ifdef _M_X64
#define JBRIDGE_PROXY_REGVAL        TEXT("Proxy64")  //use this for x64 builds
#else
#define JBRIDGE_PROXY_REGVAL        TEXT("Proxy32")  //use this for x86 builds
#endif


//Check if it’s a plugin_name.xx.dll
bool JBridge::IsBootStrapDll(const char * path)
{
	bool ret = false;

	HMODULE hModule = LoadLibrary(path);
	if( !hModule )
	{
		//some error…
		return ret;
	}
	ret = IsBootStrapDll(hModule);

	FreeLibrary( hModule );

	return ret;
}
bool JBridge::IsBootStrapDll(HMODULE hModule)
{
	//Exported dummy function to identify this as a bootstrap dll.
	return GetProcAddress(hModule, "JBridgeBootstrap") != 0;
}

// Get path to JBridge proxy
void JBridge::getJBridgeLibrary(char szProxyPath[], DWORD pathsize) {
	HKEY hKey;
	if ( RegOpenKeyEx(HKEY_LOCAL_MACHINE, JBRIDGE_PROXY_REGKEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS )
	{
		RegQueryValueEx(hKey, JBRIDGE_PROXY_REGVAL, NULL, NULL, (LPBYTE)szProxyPath, &pathsize);
		RegCloseKey(hKey);
	}
}
// Get bridge's entry point
PFNBRIDGEMAIN JBridge::getBridgeMainEntry(const HMODULE hModuleProxy)
{
	return (PFNBRIDGEMAIN)GetProcAddress(const_cast<HMODULE>(hModuleProxy), "BridgeMain");
}

