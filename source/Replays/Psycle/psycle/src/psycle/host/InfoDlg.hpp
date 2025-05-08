///\file
///\brief interface file for psycle::host::CInfoDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "cpu_time_clock.hpp"
namespace psycle { namespace host {

		/// info window.
		class CInfoDlg : public CDialog
		{
		public:
			CInfoDlg(CWnd* pParent = 0);
			virtual ~CInfoDlg() {}

		// Dialog Data
			enum { IDD = IDD_INFO };

			void UpdateInfo();
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
			virtual void OnCancel();

			DECLARE_MESSAGE_MAP()
			afx_msg void OnTimer(UINT_PTR nIDEvent);
			afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
			afx_msg void OnClose();
			afx_msg void OnCpuPerf();

		private:
			void InitTimer();

			CStatic	m_mem_virtual;
			CStatic	m_mem_pagefile;
			CStatic	m_mem_phy;
			CStatic	m_mem_reso;
			CStatic	m_cpurout;
			CStatic	m_machscpu;
			CStatic	m_processor_label;
			CStatic	m_cpuidlelabel;
			CListCtrl	m_machlist;
			CButton m_cpu_perf;

			unsigned int item_count_;
			steady_clock::time_point last_update_time_;

		};
}}
