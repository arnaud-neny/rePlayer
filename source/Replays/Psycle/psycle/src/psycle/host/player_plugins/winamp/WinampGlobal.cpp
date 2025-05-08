///\file
///\brief implementation file for psycle::host::Global.
#include <psycle/host/detail/project.private.hpp>
#include "WinampGlobal.hpp"
#include "WinampConfig.hpp"
#include <psycle/host/Song.hpp>
#include <psycle/host/Player.hpp>
#include <psycle/host/VstHost24.hpp>
#include <psycle/host/plugincatcher.hpp>


namespace psycle
{
	namespace host
	{
		WinampGlobal::WinampGlobal()
			: Global()
		{
			pConfig = new WinampConfig();
			pSong = new Song();
			pVstHost = new vst::Host();
			pPlayer = new Player();
			pMacLoad = new PluginCatcher();
		}

		WinampGlobal::~WinampGlobal()
		{
			delete pSong; pSong = 0;
			//player has to be deleted before vsthost
			delete pPlayer; pPlayer = 0;
			//vst host has to be deleted after song.
			delete pVstHost; pVstHost = 0;
			delete pMacLoad; pMacLoad = 0;
			delete pConfig; pConfig = 0;
		}
	}
}

