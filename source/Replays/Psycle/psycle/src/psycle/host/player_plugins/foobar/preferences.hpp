
#include <psycle/host/Global.hpp>
#include <foobar2000/ATLHelpers/ATLHelpers.h>
#include "resources.hpp"

class CPsyclePreferences : public CDialogImpl<CPsyclePreferences>, public preferences_page_instance {
public:
	//Constructor - invoked by preferences_page_impl helpers - don't do Create() in here, preferences_page_impl does this for us
	CPsyclePreferences(preferences_page_callback::ptr callback) : m_callback(callback) {}

	//Note that we don't bother doing anything regarding destruction of our class.
	//The host ensures that our dialog is destroyed first, then the last reference to our preferences_page_instance object is released, causing our object to be deleted.


	//dialog resource ID
	enum {IDD = IDD_CONFIG};
	// preferences_page_instance methods (not all of them - get_wnd() is supplied by preferences_page_impl helpers)
	t_uint32 get_state();
	void apply();
	void reset();

	//WTL message map
	BEGIN_MSG_MAP(CPsyclePreferences)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDC_REGENERATE, BN_CLICKED, OnRegenerate)
		COMMAND_HANDLER_EX(IDC_BWNATIVE, BN_CLICKED, OnBrowseNative)
		COMMAND_HANDLER_EX(IDC_BWVST, BN_CLICKED, OnBrowseVst)
		COMMAND_HANDLER_EX(IDC_BWVST64, BN_CLICKED, OnBrowseVst64)
		COMMAND_HANDLER_EX(IDC_CKBRIDGING, BN_CLICKED, OnCheckBridging)
	END_MSG_MAP()
private:
	BOOL OnInitDialog(CWindow, LPARAM);
	bool HasChanged();
	void OnChanged();

	void OnRegenerate(UINT, int, CWindow);
	void OnBrowseNative(UINT, int, CWindow);
	void OnBrowseVst(UINT, int, CWindow);
	void OnBrowseVst64(UINT, int, CWindow);
	void OnCheckBridging(UINT, int, CWindow);

	bool BrowseForFolder(HWND hWnd,std::string& rpath);

	const preferences_page_callback::ptr m_callback;
};


class psycle_preferences_page_impl : public preferences_page_impl<CPsyclePreferences>
{
public:
	const char * get_name() {return "Psycle Decoder";}
	GUID get_guid() {
		// This is our GUID. Replace with your own when reusing the code.
		// {607B8D60-2ACA-43de-B6CA-09A912DD0E27}
		static const GUID guid = { 0x607b8d60, 0x2aca, 0x43de, { 0xb6, 0xca, 0x9, 0xa9, 0x12, 0xdd, 0xe, 0x27 } };
		return guid;
	}
	GUID get_parent_guid() {return guid_input;}
};

