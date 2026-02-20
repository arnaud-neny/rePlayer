//=============================================================================
//		COM Support header
//=============================================================================

#ifndef COMSUPPORT_H
#define COMSUPPORT_H

#include "portability_fmpmd.h"

#ifdef PMD_REPLAYER //_WIN32
	#include <objbase.h>
	
#else
	#include <memory.h>
	
	typedef uint32_t		ULONG;
	typedef int32_t			HRESULT;
	typedef uint32_t 		HINSTANCE;
	typedef void* HANDLE;

	typedef struct _GUID {
		uint32_t	Data1;
		uint16_t	Data2;
		uint16_t	Data3;
		uint8_t		Data4[8];
	} GUID;
	
	typedef GUID IID;
	typedef GUID CLSID;
	
	#define REFGUID				const GUID &
	#define REFIID				const IID &
	#define REFCLSID			const IID &
	
	
	
	const HRESULT	S_OK					= 0x00000000L;
	const HRESULT	E_NOTIMPL				= 0x80004001L;
	const HRESULT	E_NOINTERFACE			= 0x80004002L;
	const HRESULT	E_POINTER				= 0x80004003L;
	const HRESULT	E_FAIL					= 0x80004005L;
	const HRESULT	E_INVALIDARG			= 0x80070057L;
	
	const uint32_t	CLSCTX_INPROC_SERVER	= 0x1;
	const uint32_t	CLSCTX_INPROC_HANDLER	= 0x2;
	const uint32_t	CLSCTX_LOCAL_SERVER		= 0x4;
	
	const uint32_t	CLSCTX_ALL				= CLSCTX_INPROC_SERVER |
											  CLSCTX_INPROC_HANDLER |
											  CLSCTX_LOCAL_SERVER;
	
	
	//	呼出規約の無効化
	
	#define	__stdcall
	#define	__cdecl
	#define	WINAPI
	#define	STDMETHODCALLTYPE WINAPI
	#define	__RPC_FAR
	
	#define	interface struct
	
	// IUnknown
	class IUnknown {
	public:
		virtual HRESULT STDMETHODCALLTYPE QueryInterface(
			REFIID riid,
			void __RPC_FAR *__RPC_FAR *ppvObject) { return S_OK; }
			
		virtual ULONG STDMETHODCALLTYPE AddRef(void) = 0;
		
		virtual ULONG STDMETHODCALLTYPE Release(void) = 0;
	};
	
	
	__inline int IsEqualGUID(REFGUID rg1, REFGUID rg2)
	{
		return !memcmp(&rg1, &rg2, sizeof(GUID));
	}
	
	
	#define	IsEqualIID(riid1, riid2) IsEqualGUID(riid1, riid2)
	#define FAILED(hr) (((HRESULT)(hr)) < 0)
	
	
	typedef IUnknown *LPUNKNOWN;
	typedef void *LPVOID;
	
	// CComModule
	class CComModule {
	public:
		virtual HRESULT Init(void* p, HINSTANCE h)
		{
			return S_OK;
		}
		
		virtual void Term(void)
		{
		}
	};
	
	
	class CComMultiThreadModelNoCS;
	template <class ThreadModel> class CComObjectRootEx {
	};
	

#endif


#endif	// COMSUPPORT_H
