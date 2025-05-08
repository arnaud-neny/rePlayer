#include <psycle/host/detail/project.private.hpp>
#include <foobar2000/SDK/foobar2000.h>
#include <foobar2000/helpers/helpers.h>
#include "FoobarGlobal.hpp"
#include "FoobarConfig.hpp"
#include <psycle/host/Song.hpp>
#include <psycle/host/Player.hpp>
#include <psycle/host/machineloader.hpp>

static psycle::host::FoobarGlobal global_;

class myinitquit : public initquit {
public:
	void on_init() {
		global_.conf().LoadFoobarSettings();
		global_.conf().RefreshSettings();
		global_.song().fileName[0] = '\0';
		global_.song().New();
		global_.conf().RefreshAudio();
		global_.machineload().LoadPluginInfo(false);
	}
	void on_quit() {
		global_.conf().audioDriver().Reset();
		global_.player().stop_threads();
		/*
		if(uninstallDelete) {
			global_.conf().DeleteFoobarSettings(mod);
		}
		*/
	}
};

static initquit_factory_t<myinitquit> g_myinitquit_factory;
