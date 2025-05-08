///\file
///\brief implementation file for psycle::host::CNativeGui.
#include <psycle/host/detail/project.private.hpp>
#include "NativeGraphics.hpp"

namespace psycle { namespace host {

	PsycleConfig::MachineParam* Knob::uiSetting = NULL;
	PsycleConfig::MachineParam* InfoLabel::uiSetting = NULL;
	PsycleConfig::MachineParam* GraphSlider::uiSetting = NULL;
	PsycleConfig::MachineParam* SwitchButton::uiSetting = NULL;
	PsycleConfig::MachineParam* CheckedButton::uiSetting = NULL;
	PsycleConfig::MachineParam* VuMeter::uiSetting = NULL;
	PsycleConfig::MachineParam* MasterUI::uiSetting = NULL;

	int InfoLabel::xoffset(3);
	int GraphSlider::xoffset(6);

	}   // namespace
}   // namespace
