///\file
///\brief interface file for psycle::host::CMasterDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

struct lua_State;

namespace psycle {
namespace host {

		/// master machine window.
		class CPlotterDlg : public CDialog
		{
		public:
			CPlotterDlg(CWnd* m_wndView);
			virtual ~CPlotterDlg();

public:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual void OnCancel();
			virtual void PostNcDestroy();

			void set_data(float* data, int len) { 
				data_ = std::vector<float>(data, data + len);
				len_ = len; parse_peak();
			}

			static int plotter_count;

			void do_script();

protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnClose();
			afx_msg void OnAutodec();
			afx_msg BOOL OnEraseBkgnd(CDC* pDC);
			afx_msg void OnPaint();

			enum { IDD = IDD_PLOTTER };
			CWnd* mainView;
			std::vector<float> data_;
			int len_;
			float minpeak_, maxpeak_;
			// float testdata[6]; testcase
			void parse_peak();		
		};

	}   // namespace host
}   // namespace psycle
