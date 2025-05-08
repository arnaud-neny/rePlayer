///\file
///\interface psycle::host::CMidiInputDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include <vector>

namespace psycle { namespace host {

		/// midi input config window.
		class CMidiInputDlg : public CPropertyPage
		{
			DECLARE_DYNCREATE(CMidiInputDlg)
		public:
			CMidiInputDlg();
			virtual ~CMidiInputDlg();
			int const static IDD = IDD_CONFIG_MIDI_INPUT;

		protected:
			virtual BOOL OnInitDialog();
			virtual void OnOK();
			virtual void DoDataExchange(CDataExchange* pDX);

		public:
			class group_with_message;
			class group
			{
				public:
					CButton record;
					CComboBox type;
					CEdit command;
					CEdit from;
					CEdit to;
					typedef group_with_message with_message;
			};
			class group_with_message : public group
			{
				public:
					CEdit message;
			};
			group velocity;
			group pitch;
			typedef std::vector<group::with_message*> groups_type;
			groups_type groups;
			CButton raw;
			CComboBox gen_select_type;
			CComboBox inst_select_type;
		protected:
			DECLARE_MESSAGE_MAP()
		};

	}   // namespace
}   // namespace
