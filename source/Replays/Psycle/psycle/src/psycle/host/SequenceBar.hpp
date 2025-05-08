#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "ExListBox.h"

namespace psycle { namespace host {
	class CMainFrame;
	class CChildView;
	class Song;

	class SequenceBar : public CDialogBar
	{
		friend class CMainFrame;
		DECLARE_DYNAMIC(SequenceBar)

	public:
		SequenceBar();   // standard constructor
		virtual ~SequenceBar();
		
		void InitializeValues(CMainFrame* frame, CChildView* view, Song& song);
	protected:
		virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

		void seqpaste(bool below);
	protected:
		DECLARE_MESSAGE_MAP()
		afx_msg LRESULT OnInitDialog ( WPARAM , LPARAM );
		afx_msg void OnSelchangeSeqlist();
		afx_msg void OnDblclkSeqlist();
		afx_msg void OnIncshort();
		afx_msg void OnDecshort();
		afx_msg void OnSeqnew();
		afx_msg void OnSeqduplicate();
		afx_msg void OnSeqins();
		afx_msg void OnSeqdelete();
		afx_msg void OnSeqrename();
		afx_msg void OnSeqchange();
		afx_msg void OnSeqcut();
		afx_msg void OnSeqcopy();
		afx_msg void OnSeqpasteAbove();
		afx_msg void OnSeqpasteBelow();
		afx_msg void OnSeqclear();
		afx_msg void OnSeqsort();
		afx_msg void OnUpdatepaste(CCmdUI* pCmdUI);
		afx_msg void OnUpdatepasteBelow(CCmdUI* pCmdUI);
		afx_msg void OnFollow();
		afx_msg void OnRecordNoteoff();
		afx_msg void OnRecordTweaks();
		afx_msg void OnShowpattername();
		afx_msg void OnMultichannelAudition();
		afx_msg void OnNotestoeffects();
		afx_msg void OnMovecursorpaste();

		void UpdatePlayOrder(bool mode);
		void UpdateSequencer(int bottom = -1);
	protected:
		CStatic m_duration;
		CExListBox m_sequence;
	public:
		CButton m_follow;
	protected:
		CButton m_noteoffs;
		CButton m_tweaks;
		CButton m_patNames;
		CButton m_multiChannel;
		CButton m_notesToEffects;
		CButton m_moveWhenPaste;

		CMainFrame* m_pParentMain;
		CChildView*  m_pWndView;
		Song* m_pSong;

		int seqcopybuffer[MAX_SONG_POSITIONS];
		int seqcopybufferlength;

	};
}}
