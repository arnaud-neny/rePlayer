///\file
///\brief implementation file for psycle::host::Global.
#include <psycle/host/detail/project.private.hpp>
#include "FoobarGlobal.hpp"
#include "FoobarConfig.hpp"
#include <psycle/host/Song.hpp>
#include <psycle/host/Player.hpp>
#include <psycle/host/VstHost24.hpp>
#include <psycle/host/plugincatcher.hpp>
#include <psycle/helpers/dsp.hpp>


namespace psycle
{
	namespace host
	{
		FoobarGlobal::FoobarGlobal()
			: Global()
		{
			pConfig = new FoobarConfig();
			pSong = new Song();
			pResampler = new helpers::dsp::cubic_resampler();
			pResampler->quality(helpers::dsp::resampler::quality::linear);
			pPlayer = new Player();
			pVstHost = new vst::host();
			pMacLoad = new PluginCatcher();
		}

		FoobarGlobal::~FoobarGlobal()
		{
			delete pSong; pSong = 0;
			//vst host has to be deleted after song.
			delete pVstHost; pVstHost = 0;
			delete pPlayer; pPlayer = 0;
			delete pMacLoad; pMacLoad = 0;
			delete pResampler; pResampler = 0;
			delete pConfig; pConfig = 0;
		}
	}
}

