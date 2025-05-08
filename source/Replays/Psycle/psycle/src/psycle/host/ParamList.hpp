///\file
///\brief interface file for psycle::host::CVstParamList.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle {
	namespace host {

		class Machine;
		class CFrameMachine;

		class CParamList : public CDialog
		{
		public:
			CParamList(Machine& effect, CFrameMachine* parent, CParamList** windowVar);   // standard constructor
			virtual ~CParamList();

			enum { IDD = IDD_GEAR_VSTPARAMS };
			CComboBox	m_program;
			CSliderCtrl	m_slider;
			CStatic		m_text;
			CListBox	m_parlist;
		
		// Attributes
		public:
			Machine& machine;

		protected:
			int _quantizedvalue;
			CFrameMachine* parentFrame;
			CParamList** windowVar;

		// Operations
		public:
			void Init();
			void UpdateOne();
			void UpdateParList();
			void UpdateText(int value);
			void UpdateNew(int par,int value);
			void InitializePrograms(void);
			void SelectProgram(long index);

		// Overrides
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual BOOL PreTranslateMessage(MSG* pMsg);
			virtual void PostNcDestroy();
			virtual void OnCancel();
		#if !defined NDEBUG
			virtual void AssertValid() const;
			virtual void Dump(CDumpContext& dc) const;
		#endif
		protected:
			// Generated message map functions
			DECLARE_MESSAGE_MAP()
			afx_msg void OnClose();
			afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
			afx_msg void OnSelchangeList();
			afx_msg void OnSelchangeProgram();
			afx_msg void OnDeltaposSpin(NMHDR* pNMHDR, LRESULT* pResult);
		};
	}   // namespace
}   // namespace
