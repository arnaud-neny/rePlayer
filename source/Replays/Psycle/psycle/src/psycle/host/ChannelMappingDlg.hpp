///\file
///\brief interface file for psycle::host::CVolumeDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {
		/// volume window.
		class Wire;
		class CChannelMappingDlg : public CDialog
		{
		public:
			CChannelMappingDlg(Wire& wire, CWnd* mainView_, CWnd* pParent = 0);
			enum { IDD = IDD_WIRE_CHANMAP };
		protected:
			virtual BOOL PreTranslateMessage(MSG* pMsg);
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual INT_PTR OnToolHitTest(CPoint point, TOOLINFO * pTI) const;

			virtual BOOL OnInitDialog();
			virtual void OnOK();
			virtual void OnCancel();
			int CheckFromPoint(CPoint point, RECT& rect) const;
			void FillCheckedVector(std::vector<std::vector<bool>>& checked, int srcpins, int dstpins);
		protected:
			void AddButton(int yRel, int xRel, int amountx, bool checked, CFont& font, std::string text);
			DECLARE_MESSAGE_MAP()
			afx_msg void OnCheckChanMap(UINT index);
			afx_msg BOOL OnToolTipText( UINT id, NMHDR * pNMHDR, LRESULT * pResult );
			afx_msg void OnAutoWire();
			afx_msg void OnUnselectAll();

			Wire &         m_wire;
			std::vector<CStatic*> dstnames;
			std::vector<CButton*> buttons;
			CWnd*        mainView;
		};

	}   // namespace
}   // namespace
