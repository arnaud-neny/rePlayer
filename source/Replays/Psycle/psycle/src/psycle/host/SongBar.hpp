#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {
	class CChildView;
	class CMainFrame;
	class Song;

	class SongBar : public CDialogBar
	{
		friend class CMainFrame;
	DECLARE_DYNAMIC(SongBar)

	public:
		SongBar(void);
		virtual ~SongBar(void);
		CComboBox       m_trackcombo;
		CComboBox       m_octavecombo;
		CSliderCtrl		m_masterslider;
		CStatic			m_bpmlabel;
		CStatic			m_lpblabel;


		void InitializeValues(CMainFrame* frame, CChildView* view, Song& song);
		void SetAppSongBpm(int x);
		void SetAppSongTpb(int x);
		void ShiftOctave(int x);
		void UpdateMasterValue(int newvalue);
		void UpdateVumeters();
	protected:
		virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	protected:
		DECLARE_MESSAGE_MAP()
		afx_msg LRESULT OnInitDialog ( WPARAM , LPARAM );
		afx_msg void OnSelchangeTrackcombo();
		afx_msg void OnCloseupTrackcombo();
		afx_msg void OnBpmAddOne();
		afx_msg void OnBpmDecOne();
		afx_msg void OnDecTPB();
		afx_msg void OnIncTPB();
		afx_msg void OnCloseupCombooctave();
		afx_msg void OnSelchangeCombooctave();
		afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
		afx_msg void OnClipbut();
		afx_msg BOOL OnToolTipNotify( UINT unId, NMHDR *pstNMHDR, LRESULT *pstResult );

	protected:
		CMainFrame* m_pParentMain;
		CChildView*  m_pWndView;
		Song* m_pSong;

		int vuprevL;
		int vuprevR;
	};
}}
