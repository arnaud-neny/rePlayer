/** @file 
 *  @brief SequenceBar dialog
 *  $Date: 2010-08-15 18:18:35 +0200 (dg., 15 ag 2010) $
 *  $Revision: 9831 $
 */
#include <psycle/host/detail/project.private.hpp>
#include "SequenceBar.hpp"
#include "InputHandler.hpp"
#include "PsycleConfig.hpp"
#include "MainFrm.hpp"
#include "ChildView.hpp"
#include "Song.hpp"
#include "Player.hpp"

namespace psycle{ namespace host{

	extern CPsycleApp theApp;

IMPLEMENT_DYNAMIC(SequenceBar, CDialogBar)

	SequenceBar::SequenceBar()
	{
		seqcopybufferlength = 0;
	}

	SequenceBar::~SequenceBar()
	{
	}

	void SequenceBar::DoDataExchange(CDataExchange* pDX)
	{
		CDialogBar::DoDataExchange(pDX);
		DDX_Control(pDX, IDC_LENGTH, m_duration);
		DDX_Control(pDX, IDC_SEQLIST, m_sequence);
		DDX_Control(pDX, IDC_FOLLOW, m_follow);
		DDX_Control(pDX, IDC_RECORD_NOTEOFF, m_noteoffs);
		DDX_Control(pDX, IDC_RECORD_TWEAKS, m_tweaks);
		DDX_Control(pDX, IDC_SHOWPATTERNAME, m_patNames);
		DDX_Control(pDX, IDC_MULTICHANNEL_AUDITION, m_multiChannel);
		DDX_Control(pDX, IDC_NOTESTOEFFECTS, m_notesToEffects);
		DDX_Control(pDX, IDC_MOVECURSORPASTE, m_moveWhenPaste);
	}

	//Message Maps are defined in CMainFrame, since this isn't a window, but a DialogBar.
/*
	BEGIN_MESSAGE_MAP(SequenceBar, CDialogBar)
		ON_MESSAGE(WM_INITDIALOG, OnInitDialog )
	END_MESSAGE_MAP()
*/

	void SequenceBar::InitializeValues(CMainFrame* frame, CChildView* view, Song& song)
	{
		m_pParentMain = frame;
		m_pWndView = view;
		m_pSong = &song;
	}

	// SequenceBar message handlers
	LRESULT SequenceBar::OnInitDialog ( WPARAM wParam, LPARAM lParam)
	{
		BOOL bRet = HandleInitDialog(wParam, lParam);

		if (!UpdateData(FALSE))
		{
		   TRACE0("Warning: UpdateData failed during dialog init.\n");
		}

		//m_sequence.SubclassDlgItem(IDC_SEQLIST, this );

		m_multiChannel.SetCheck(PsycleGlobal::conf().inputHandler().bMultiKey?1:0);
		m_moveWhenPaste.SetCheck(PsycleGlobal::conf().inputHandler().bMoveCursorPaste?1:0);
		m_patNames.SetCheck(PsycleGlobal::conf()._bShowPatternNames?1:0);
		m_noteoffs.SetCheck(PsycleGlobal::conf().inputHandler()._RecordNoteoff?1:0);
		m_tweaks.SetCheck(PsycleGlobal::conf().inputHandler()._RecordTweaks?1:0);
		m_notesToEffects.SetCheck(PsycleGlobal::conf().inputHandler()._notesToEffects?1:0);
		m_follow.SetCheck(PsycleGlobal::conf()._followSong?1:0);

		((CButton*)GetDlgItem(IDC_INCSHORT))->SetIcon(PsycleGlobal::conf().iconplus);
		((CButton*)GetDlgItem(IDC_DECSHORT))->SetIcon(PsycleGlobal::conf().iconminus);

		UpdatePlayOrder(true);
		UpdateSequencer();

		return bRet;
	}

	void SequenceBar::OnSelchangeSeqlist() 
	{
		int maxitems=m_sequence.GetCount();
		int const ep=m_sequence.GetCurSel();
		if(m_pWndView->editPosition<0) m_pWndView->editPosition = 0;
		int const cpid=m_pSong->playOrder[m_pWndView->editPosition];

		memset(m_pSong->playOrderSel,0,MAX_SONG_POSITIONS*sizeof(bool));
		for (int c=0;c<maxitems;c++) 
		{
			if ( m_sequence.GetSel(c) != 0) m_pSong->playOrderSel[c]=true;
		}
		
		if((ep!=m_pWndView->editPosition))// && ( m_sequence.GetSelCount() == 1))
		{
			if ((Global::player()._playing) && (PsycleGlobal::conf()._followSong))
			{
				bool b = Global::player()._playBlock;
				Global::player().Start(ep,0,false);
				Global::player()._playBlock = b;
			}
			m_pWndView->editPosition=ep;
			m_pWndView->prevEditPosition=ep;
			UpdatePlayOrder(false);
			
			if(cpid!=m_pSong->playOrder[ep])
			{
				m_pWndView->Repaint(draw_modes::pattern);
				if (Global::player()._playing) {
					m_pWndView->Repaint(draw_modes::playback);
				}
			}		
		}
		m_pParentMain->StatusBarIdle();
		m_pWndView->SetFocus();		
	}

	void SequenceBar::OnDblclkSeqlist() 
	{
		int const ep=m_sequence.GetCurSel();
		if (Global::player()._playing)
		{
			bool b = Global::player()._playBlock;
			Global::player().Start(ep,0);
			Global::player()._playBlock = b;
		}
		else
		{
			Global::player().Start(ep,0);
		}
		m_pWndView->editPosition=ep;
		m_pWndView->SetFocus();
	}
	void SequenceBar::OnSeqrename()
	{
		m_sequence.ShowEditBox(true);
	}
	void SequenceBar::OnSeqchange()
	{
		m_sequence.ShowEditBox(false);
	}

	void SequenceBar::OnIncshort()
	{
		int indexes[MAX_SONG_POSITIONS];
		PsycleGlobal::inputHandler().AddUndoSequence(m_pSong->playLength,m_pWndView->editcur.track,m_pWndView->editcur.line,m_pWndView->editcur.col,m_pWndView->editPosition);

		int const num= m_sequence.GetSelCount();
		m_sequence.GetSelItems(MAX_SONG_POSITIONS,indexes);

		for (int i = 0; i < num; i++)
		{
			if(m_pSong->playOrder[indexes[i]]<(MAX_PATTERNS-1))
			{
				m_pSong->playOrder[indexes[i]]++;
			}
		}
		UpdatePlayOrder(false);
		UpdateSequencer();
		((CButton*)GetDlgItem(IDC_INCSHORT))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->Repaint(draw_modes::pattern);
		m_pWndView->SetFocus();
		ANOTIFY(Actions::seqmodified);
	}

	void SequenceBar::OnDecshort()
	{
		int indexes[MAX_SONG_POSITIONS];
		PsycleGlobal::inputHandler().AddUndoSequence(m_pSong->playLength,m_pWndView->editcur.track,m_pWndView->editcur.line,m_pWndView->editcur.col,m_pWndView->editPosition);

		int const num= m_sequence.GetSelCount();
		m_sequence.GetSelItems(MAX_SONG_POSITIONS,indexes);

		for (int i = 0; i < num; i++)
		{
			if(m_pSong->playOrder[indexes[i]]>0)
			{
				m_pSong->playOrder[indexes[i]]--;
			}
		}
		UpdatePlayOrder(false);
		UpdateSequencer();
		((CButton*)GetDlgItem(IDC_DECSHORT))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->Repaint(draw_modes::pattern);
		m_pWndView->SetFocus();
		ANOTIFY(Actions::seqmodified);
	}

	void SequenceBar::OnSeqnew()
	{
		if(m_pSong->playLength<(MAX_SONG_POSITIONS-1))
		{
			PsycleGlobal::inputHandler().AddUndoSequence(m_pSong->playLength,m_pWndView->editcur.track,m_pWndView->editcur.line,m_pWndView->editcur.col,m_pWndView->editPosition);
			++m_pSong->playLength;

			m_pWndView->editPosition++;
			int const pop=m_pWndView->editPosition;
			for(int c=(m_pSong->playLength-1);c>=pop;c--)
			{
				m_pSong->playOrder[c]=m_pSong->playOrder[c-1];
			}
			m_pSong->playOrder[m_pWndView->editPosition]=m_pSong->GetBlankPatternUnused();
			
			if ( m_pSong->playOrder[m_pWndView->editPosition]>= MAX_PATTERNS )
			{
				m_pSong->playOrder[m_pWndView->editPosition]=MAX_PATTERNS-1;
			}

			m_pSong->AllocNewPattern(m_pSong->playOrder[m_pWndView->editPosition],"",
				Global::configuration().GetDefaultPatLines(),FALSE);

			UpdatePlayOrder(true);
			UpdateSequencer(m_pWndView->editPosition);

			m_pWndView->Repaint(draw_modes::pattern);
		}
		((CButton*)GetDlgItem(IDC_SEQNEW))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->SetFocus();
		ANOTIFY(Actions::seqmodified);
	}

	void SequenceBar::OnSeqduplicate()
	{
		int selcount = m_sequence.GetSelCount();
		if (selcount == 0) return;
		if ( m_pSong->playLength+selcount >= MAX_SONG_POSITIONS)
		{
			MessageBox("Cannot clone the pattern(s). The maximum sequence length would be exceeded.","Clone Patterns");
			m_pWndView->SetFocus();
			return;
		}
		PsycleGlobal::inputHandler().AddUndoSequence(m_pSong->playLength,m_pWndView->editcur.track,m_pWndView->editcur.line,m_pWndView->editcur.col,m_pWndView->editPosition);
		// Moves all patterns after the selection, to make space.
		int* litems = new int[selcount];
		m_sequence.GetSelItems(selcount,litems);
		for(int i(m_pSong->playLength-1) ; i >= litems[selcount-1] ;--i)
		{
			m_pSong->playOrder[i+selcount]=m_pSong->playOrder[i];
		}
		m_pSong->playLength+=selcount;

		for(int i(0) ; i < selcount ; ++i)
		{
			int newpat = -1;
			// This for loop is in order to clone sequences like: 00 00 01 01 and avoid duplication of same patterns.
			for (int j(0); j < i; ++j)
			{
				if (m_pSong->playOrder[litems[0]+j] == m_pSong->playOrder[litems[0]+i])
				{
					newpat=m_pSong->playOrder[litems[selcount-1]+j+1];
				}
			}
			if (newpat == -1 ) 
			{
				newpat = m_pSong->GetBlankPatternUnused();
				if (newpat < MAX_PATTERNS-1)
				{
					int oldpat = m_pSong->playOrder[litems[i]];
					m_pSong->AllocNewPattern(newpat,m_pSong->patternName[oldpat],m_pSong->patternLines[oldpat],FALSE);
					memcpy(m_pSong->_ppattern(newpat),m_pSong->_ppattern(oldpat),MULTIPLY2);
					m_pSong->CopyNamesFrom(oldpat, newpat);
				}
				else 
				{
					newpat=0;
				}
			}
			m_pSong->playOrder[litems[selcount-1]+i+1]=newpat;
		}
		m_pWndView->editPosition=litems[selcount-1]+1;
		UpdatePlayOrder(true);
		UpdateSequencer(m_pWndView->editPosition);
		((CButton*)GetDlgItem(IDC_SEQDUPLICATE))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->Repaint(draw_modes::pattern);

		delete [] litems; litems = 0;
		m_pWndView->SetFocus();
		ANOTIFY(Actions::seqmodified);
	}

	void SequenceBar::OnSeqins()
	{
		if(m_pSong->playLength<(MAX_SONG_POSITIONS-1))
		{
			PsycleGlobal::inputHandler().AddUndoSequence(m_pSong->playLength,m_pWndView->editcur.track,m_pWndView->editcur.line,m_pWndView->editcur.col,m_pWndView->editPosition);
			++m_pSong->playLength;

			m_pWndView->editPosition++;
			int const pop=m_pWndView->editPosition;
			for(int c=(m_pSong->playLength-1);c>=pop;c--)
			{
				m_pSong->playOrder[c]=m_pSong->playOrder[c-1];
			}

			UpdatePlayOrder(true);
			UpdateSequencer(m_pWndView->editPosition);

			m_pWndView->Repaint(draw_modes::pattern);
		}
		((CButton*)GetDlgItem(IDC_SEQINS))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->SetFocus();
		ANOTIFY(Actions::seqmodified);
	}

	void SequenceBar::OnSeqdelete()
	{
		int indexes[MAX_SONG_POSITIONS];
		PsycleGlobal::inputHandler().AddUndoSequence(m_pSong->playLength,m_pWndView->editcur.track,m_pWndView->editcur.line,m_pWndView->editcur.col,m_pWndView->editPosition);

		int const num= m_sequence.GetSelCount();
		m_sequence.GetSelItems(MAX_SONG_POSITIONS,indexes);

		// our list can be in any order so we must be careful
		int smallest = indexes[0]; // we need a good place to put the cursor when we are done, above the topmost selection seems most intuitive
		for (int i=0; i < num; i++)
		{
			int c;
			for(c = indexes[i] ; c < m_pSong->playLength - 1 ; ++c)
			{
				m_pSong->playOrder[c]=m_pSong->playOrder[c+1];
			}
			m_pSong->playOrder[c]=0;
			m_pSong->playLength--;
			if (m_pSong->playLength <= 0)
			{
				m_pSong->playLength =1;
			}
			for(int j(i + 1) ; j < num ; ++j)
			{
				if (indexes[j] > indexes[i])
				{
					indexes[j]--;
				}
			}
			if (indexes[i] < smallest)
			{
				smallest = indexes[i];
			}
		}
		m_pWndView->editPosition = smallest-1;

		if (m_pWndView->editPosition<0)
		{
			m_pWndView->editPosition = 0;
		}
		else if (m_pWndView->editPosition>=m_pSong->playLength)
		{
			m_pWndView->editPosition=m_pSong->playLength-1;
		}

		UpdatePlayOrder(true);
		UpdateSequencer(m_pWndView->editPosition);
		((CButton*)GetDlgItem(IDC_SEQDELETE))->ModifyStyle(BS_DEFPUSHBUTTON, 0);
		m_pWndView->Repaint(draw_modes::pattern);
		m_pWndView->SetFocus();
		ANOTIFY(Actions::seqmodified);
	}

	void SequenceBar::OnSeqcut()
	{
		OnSeqcopy();
		OnSeqdelete();
	}

	void SequenceBar::OnSeqcopy()
	{
		seqcopybufferlength= m_sequence.GetSelCount();
		m_sequence.GetSelItems(MAX_SONG_POSITIONS,seqcopybuffer);

		// sort our table so we can paste it in a sensible manner later
		for (int i=0; i < seqcopybufferlength; i++)
		{
			for (int j=i+1; j < seqcopybufferlength; j++)
			{
				if (seqcopybuffer[j] < seqcopybuffer[i])
				{
					int k = seqcopybuffer[i];
					seqcopybuffer[i] = seqcopybuffer[j];
					seqcopybuffer[j] = k;
				}
			}
			// convert to actual index
			seqcopybuffer[i] = m_pSong->playOrder[seqcopybuffer[i]];
		}
		m_pWndView->SetFocus();
		ANOTIFY(Actions::seqmodified);
	}

	void SequenceBar::OnSeqpasteAbove()
	{
		seqpaste(false);	
	}
	void SequenceBar::OnSeqpasteBelow()
	{
		seqpaste(true);
	}
	void SequenceBar::seqpaste(bool below)
	{
		if (seqcopybufferlength > 0)
		{
			PsycleGlobal::inputHandler().AddUndoSequence(m_pSong->playLength,m_pWndView->editcur.track,m_pWndView->editcur.line,m_pWndView->editcur.col,m_pWndView->editPosition);
			if(m_pSong->playLength+seqcopybufferlength>MAX_SONG_POSITIONS) {
				seqcopybufferlength=MAX_SONG_POSITIONS-m_pSong->playLength;
			}
			if(below) m_pWndView->editPosition+=1;
			int movepos = m_pSong->playLength-1;
			
			m_pSong->playLength+=seqcopybufferlength;
			int c,i;
			for(c=m_pSong->playLength-1, i=movepos; i >= m_pWndView->editPosition; --c,--i)
			{
				m_pSong->playOrder[c]=m_pSong->playOrder[i];
			}
			for(int i=seqcopybufferlength-1; c >= m_pWndView->editPosition; --c, --i)
			{
				m_pSong->playOrder[c]=seqcopybuffer[i];
			}

			UpdatePlayOrder(true);
			for(int i=0; i <MAX_SONG_POSITIONS; ++i)
			{
				m_pSong->playOrderSel[i] = false;
			}
			m_pSong->playOrderSel[m_pWndView->editPosition] = true;
			UpdateSequencer(m_pWndView->editPosition);
			m_pWndView->Repaint(draw_modes::pattern);
		}
		m_pWndView->SetFocus();
		ANOTIFY(Actions::seqmodified);
	}
	void SequenceBar::OnSeqclear()
	{
		if (MessageBox("Do you really want to clear the sequence and pattern data?","Sequencer",MB_YESNO) == IDYES)
		{
			PsycleGlobal::inputHandler().AddUndoSong(m_pWndView->editcur.track,m_pWndView->editcur.line,m_pWndView->editcur.col,m_pWndView->editPosition);
			{
				CExclusiveLock lock(&m_pSong->semaphore, 2, true);
				// clear sequence
				for(int c=0;c<MAX_SONG_POSITIONS;c++)
				{
					m_pSong->playOrder[c]=0;
				}
				// clear pattern data
				m_pSong->DeleteAllPatterns();
				// init a pattern for #0
				m_pSong->AllocNewPattern(0,"",PsycleGlobal::conf().GetDefaultPatLines(),false);	

				m_pWndView->editPosition=0;
				m_pSong->playLength=1;
			}
			UpdatePlayOrder(true);
			UpdateSequencer();
			m_pWndView->Repaint(draw_modes::pattern);
		}
		m_pWndView->SetFocus();
		ANOTIFY(Actions::seqmodified);
	}

	void SequenceBar::OnSeqsort()
	{
		PsycleGlobal::inputHandler().AddUndoSong(m_pWndView->editcur.track,m_pWndView->editcur.line,m_pWndView->editcur.col,m_pWndView->editPosition);
		unsigned char oldtonew[MAX_PATTERNS];
		unsigned char newtoold[MAX_PATTERNS];
		memset(oldtonew,255,MAX_PATTERNS*sizeof(char));
		memset(newtoold,255,MAX_PATTERNS*sizeof(char));

		if (Global::player()._playing)
		{
			Global::player().Stop();
		}


	// Part one, Read patterns from sequence and assign them a new ordered number.
		unsigned char freep=0;
		for ( int i=0 ; i<m_pSong->playLength ; i++ )
		{
			const unsigned char cp=m_pSong->playOrder[i];
			if ( oldtonew[cp] == 255 ) // else, we have processed it already
			{
				oldtonew[cp]=freep;
				newtoold[freep]=cp;
				freep++;
			}
		}
	// Part one and a half. End filling the order numbers.
		for(int i(0) ; i < MAX_PATTERNS ; ++i)
		{
			if ( oldtonew[i] == 255 )
			{
				oldtonew[i] = freep;
				newtoold[freep] = i;
				freep++;
			}
		}
	// Part two. Sort Patterns. Take first "invalid" out, and start putting patterns in their place.
	//			 When we have to put the first read one back, do it and find next candidate.

		int patl; // first one is initial one, next one is temp one
		char patn[32]; // ""
		unsigned char * pData; // ""


		int idx=0;
		int idx2=0;
		for(int i(0) ; i < MAX_PATTERNS ; ++i)
		{
			if ( newtoold[i] != i ) // check if this place belongs to another pattern
			{
				pData = m_pSong->ppPatternData[i];
				memcpy(&patl,&m_pSong->patternLines[i],sizeof(int));
				memcpy(patn,&m_pSong->patternName[i],sizeof(char)*32);

				idx = i;
				while ( newtoold[idx] != i ) // Start moving patterns while it is not the stored one.
				{
					idx2 = newtoold[idx]; // get pattern that goes here and move.

					m_pSong->ppPatternData[idx] = m_pSong->ppPatternData[idx2];
					memcpy(&m_pSong->patternLines[idx],&m_pSong->patternLines[idx2],sizeof(int));
					memcpy(&m_pSong->patternName[idx],&m_pSong->patternName[idx2],sizeof(char)*32);
					
					newtoold[idx]=idx; // and indicate that this pattern has been corrected.
					idx = idx2;
				}

				// Put pattern back.
				m_pSong->ppPatternData[idx] = pData;
				memcpy(&m_pSong->patternLines[idx],&patl,sizeof(int));
				memcpy(m_pSong->patternName[idx],patn,sizeof(char)*32);

				newtoold[idx]=idx; // and indicate that this pattern has been corrected.
			}
		}
	// Part three. Update the sequence

		for(int i(0) ; i < m_pSong->playLength ; ++i)
		{
			m_pSong->playOrder[i]=oldtonew[m_pSong->playOrder[i]];
		}

	// Part four. All the needed things.

		seqcopybufferlength = 0;
		UpdateSequencer();
		m_pWndView->Repaint(draw_modes::pattern);
		m_pWndView->SetFocus();
		ANOTIFY(Actions::seqmodified);
	}

	void SequenceBar::OnFollow()
	{
		PsycleGlobal::conf()._followSong = m_follow.GetCheck()?true:false;

		if ( PsycleGlobal::conf()._followSong )
		{
			if  ( Global::player()._playing )
			{
				m_pWndView->ChordModeOffs = 0;
				m_pWndView->bScrollDetatch=false;
				if (m_sequence.GetCurSel() != Global::player()._playPosition)
				{
					m_sequence.SelItemRange(false,0,m_sequence.GetCount()-1);
					m_sequence.SetSel(Global::player()._playPosition,true);
				}
				if ( m_pWndView->editPosition  != Global::player()._playPosition )
				{
					m_pWndView->editPosition=Global::player()._playPosition;
					m_pWndView->Repaint(draw_modes::pattern);
				}
				int top = Global::player()._playPosition - 0xC;
				if (top < 0) top = 0;
				m_sequence.SetTopIndex(top);
			}
			else
			{
				m_sequence.SelItemRange(false,0,m_sequence.GetCount()-1);
				for (int i=0;i<MAX_SONG_POSITIONS;i++ )
				{
					if (m_pSong->playOrderSel[i]) m_sequence.SetSel(i,true);
				}
				int top = m_pWndView->editPosition - 0xC;
				if (top < 0) top = 0;
				m_sequence.SetTopIndex(top);
			}
		}
		m_pWndView->SetFocus();
	}

	void SequenceBar::OnRecordNoteoff()
	{
		if ( m_noteoffs.GetCheck() ) PsycleGlobal::conf().inputHandler()._RecordNoteoff=true;
		else PsycleGlobal::conf().inputHandler()._RecordNoteoff=false;
		m_pWndView->SetFocus();}

	void SequenceBar::OnRecordTweaks()
	{
		if ( m_tweaks.GetCheck() ) PsycleGlobal::conf().inputHandler()._RecordTweaks=true;
		else PsycleGlobal::conf().inputHandler()._RecordTweaks=false;
		m_pWndView->SetFocus();
	}

	void SequenceBar::OnShowpattername()
	{
		PsycleGlobal::conf()._bShowPatternNames=m_patNames.GetCheck();
		
		/*

		//trying to set the size of the sequencer bar... how to do this!?

		CRect borders;
		GetWindowRect(&borders);
		TRACE("borders.right = %i", borders.right);
		if (PsycleGlobal::conf()._bShowPatternNames)
		{
		   //SetBorders(borders.left, borders.top, 6, borders.bottom);
		}
		else
		{
			//SetBorders(borders.left, borders.top, 3, borders.bottom);
		}
		*/

		int selpos = ((Global::player()._playing)?Global::player()._playPosition:m_pWndView->editPosition);
		UpdateSequencer(selpos);
		m_pWndView->SetFocus();
	}
	void SequenceBar::OnMultichannelAudition()
	{
		PsycleGlobal::conf().inputHandler().bMultiKey = !PsycleGlobal::conf().inputHandler().bMultiKey;
		m_pWndView->SetFocus();
	}

	void SequenceBar::OnNotestoeffects()
	{
		if ( m_notesToEffects.GetCheck() ) PsycleGlobal::conf().inputHandler()._notesToEffects=true;
		else PsycleGlobal::conf().inputHandler()._notesToEffects=false;
		m_pWndView->SetFocus();
	}

	void SequenceBar::OnMovecursorpaste()
	{
		PsycleGlobal::conf().inputHandler().bMoveCursorPaste = !PsycleGlobal::conf().inputHandler().bMoveCursorPaste;
		m_pWndView->SetFocus();
	}
	void SequenceBar::OnUpdatepaste(CCmdUI* pCmdUI)
	{
		if (seqcopybufferlength > 0)
		{
			pCmdUI->Enable(true);
		}
		else {
			pCmdUI->Enable(false);
		}
	}
	void SequenceBar::OnUpdatepasteBelow(CCmdUI* pCmdUI) 
	{
		if (seqcopybufferlength > 0)
		{
			pCmdUI->Enable(true);
		}
		else {
			pCmdUI->Enable(false);
		}
	}

	void SequenceBar::UpdateSequencer(int selectedpos)
	{
		char buf[38];

		int top = m_sequence.GetTopIndex();
		m_sequence.ResetContent();
		
		if (PsycleGlobal::conf()._bShowPatternNames)
		{
			for(int n=0;n<m_pSong->playLength;n++)
			{
				sprintf(buf,"%.2X: %s",n,m_pSong->patternName[m_pSong->playOrder[n]]);
				m_sequence.AddString(buf);
			}
		}
		else
		{
			for(int n=0;n<m_pSong->playLength;n++)
			{
				sprintf(buf,"%.2X: %.2X",n,m_pSong->playOrder[n]);
				m_sequence.AddString(buf);
			}
		}
		
		if (selectedpos >= 0)
		{
			m_sequence.SetSel(selectedpos);
			top = selectedpos - 0xC;
			if (top < 0) top = 0;
		}
		else {
			m_sequence.SelItemRange(false,0,m_sequence.GetCount()-1);
			for (int i=0; i<MAX_SONG_POSITIONS;i++)
			{
				if (m_pSong->playOrderSel[i]) {
					m_sequence.SetSel(i,true);
					if (i < top) top = i;
				}
			}
		}
		m_sequence.SetTopIndex(top);
		m_pParentMain->StatusBarIdle();
	}




	void SequenceBar::UpdatePlayOrder(bool mode)
	{

		char buffer[64];
		int seq=-1;
		int pos=-1;
		int time=-1;
		int linecountloc=-1;
		Global::player().CalcPosition(*m_pSong,seq,pos,time, linecountloc);
		int songLength = time/1000;

		sprintf(buffer, "%02d:%02d", ((int)songLength) / 60, ((int)songLength) % 60);
		m_duration.SetWindowText(buffer);
		
		// Update sequencer line
		
		if (mode)
		{
			const int ls=m_pWndView->editPosition;
			const int le=m_pSong->playOrder[ls];
			m_sequence.DeleteString(ls);

			if (PsycleGlobal::conf()._bShowPatternNames)
				sprintf(buffer,"%.2X:%s",ls,m_pSong->patternName[le]);
			else
				sprintf(buffer,"%.2X: %.2X",ls,le);
			m_sequence.InsertString(ls,buffer);
			// Update sequencer selection	
			m_sequence.SelItemRange(false,0,m_sequence.GetCount()-1);
			m_sequence.SetSel(ls,true);
			int top = ls - 0xC;
			if (top < 0) top = 0;
			m_sequence.SetTopIndex(top);
			memset(m_pSong->playOrderSel,0,MAX_SONG_POSITIONS*sizeof(bool));
			m_pSong->playOrderSel[ls] = true;
		}
		else
		{
			int top = m_sequence.GetTopIndex();
			m_sequence.SelItemRange(false,0,m_sequence.GetCount()-1);
			if (Global::player()._playing && PsycleGlobal::conf()._followSong) {
				m_sequence.SetSel(Global::player()._playPosition,true);
				if (Global::player()._playPosition < top) top = Global::player()._playPosition;
			}
			else {
				for (int i=0;i<MAX_SONG_POSITIONS;i++ )
				{
					if (m_pSong->playOrderSel[i]) {
						m_sequence.SetSel(i,true);
						if (i < top) top = i;
					}
				}
			}
			m_sequence.SetTopIndex(top);
		}
	}

}}
