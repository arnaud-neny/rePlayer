#pragma once
#include <vst2.x/AEffectx.h>               /* VST header files                  */

// Typedef for BridgeMain proc
typedef AEffect * (*PFNBRIDGEMAIN)( audioMasterCallback audiomaster, char * pszPluginPath );

//*******************************
class JBridge {
public:
	static bool IsBootStrapDll(const char * path);
	static bool IsBootStrapDll(HMODULE hModule);
	static void getJBridgeLibrary(char szProxyPath[], DWORD pathsize);
	static PFNBRIDGEMAIN getBridgeMainEntry(const HMODULE hModuleProxy);
};
