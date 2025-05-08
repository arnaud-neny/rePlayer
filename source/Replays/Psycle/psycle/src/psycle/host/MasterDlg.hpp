///\file
///\brief interface file for psycle::host::CMasterDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "PsycleConfig.hpp"

namespace psycle {
namespace host {

		class Master;

		class CVolumeCtrl: public CSliderCtrl
		{
		public:
			CVolumeCtrl(int index) : index_(index) {}

			int index() const { return index_; }

		private:
			int index_;
		};

		class CMasterVu: public CProgressCtrl
		{
		public:
			CMasterVu();
			virtual ~CMasterVu();

			void SetOffset(int offset);

			DECLARE_MESSAGE_MAP()
			afx_msg BOOL OnEraseBkgnd(CDC* pDC);
			afx_msg void OnPaint();

		private:
			int offsetX;
			bool doStretch;
		};

		/// master machine window.
		class CMasterDlg : public CDialog
		{
		public:

			CMasterDlg(CWnd* m_wndView, Master& new_master, CMasterDlg** windowVar);
			virtual ~CMasterDlg();

			void UpdateUI(void);
			void RefreshSkin();

			///\todo should be private
			char macname[MAX_CONNECTIONS][32];

protected:
			void PaintNumbersDC(CDC* dc,float val,int x,int y);
			LRESULT DrawSliderGraphics(NMHDR* pNMHDR);
			void PaintNames(char* name,int x,int y);
			void SetSliderValues();
			void OnChangeSliderMaster(int pos);
			void OnChangeSliderMacs(CVolumeCtrl* slider);
public:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual BOOL PreTranslateMessage(MSG* pMsg);
			virtual void OnCancel();
			virtual void PostNcDestroy();
protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnClose();
			afx_msg void OnAutodec();
			afx_msg BOOL OnEraseBkgnd(CDC* pDC);
			afx_msg void OnCustomdrawSlidermaster(NMHDR* pNMHDR, LRESULT* pResult);
			afx_msg void OnCustomdrawSliderm(UINT idx, NMHDR* pNMHDR, LRESULT* pResult);
			afx_msg void OnPaint();
			afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

			enum { IDD = IDD_GEAR_MASTER };
			CStatic	m_masterpeak;
			CVolumeCtrl	m_slidermaster;
			std::vector<CVolumeCtrl*> sliders_;
			CMasterVu m_vuLeft;
			CMasterVu m_vuRight;
			CButton	m_autodec;
			
			static int masterWidth;
			static int masterHeight;
			static int numbersAddX;
			static int textYAdd;
			// "sc" for scaled values.
			int scmasterWidth;
			int scmasterHeight;
			int scnumbersAddX;
			int sctextYAdd;
			int scnumbersMasterX;
			int scnumbersMasterY;
			int scnumbersX;
			int scnumbersY;
			int sctextX;
			int sctextY;
			int sctextW;
			int sctextH;
	
			Master& machine;
			CMasterDlg** windowVar_;
			CWnd* mainView;
			bool doStretch;
		};

	}   // namespace host
}   // namespace psycle
