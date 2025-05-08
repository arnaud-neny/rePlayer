///\file
///\brief implementation file for psycle::host::CSongpDlg.
#include <psycle/host/detail/project.private.hpp>
#include "SongpDlg.hpp"
#include "Song.hpp"
#include "Player.hpp"
namespace psycle { namespace host {

		CSongpDlg::CSongpDlg(Song& song) : CDialog(CSongpDlg::IDD)
		,readonlystate(false)
		,initialized(false)
		,_pSong(song)
		{
		}

		void CSongpDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_EDIT_COMMENTS, m_songcomments);
			DDX_Control(pDX, IDC_EDIT_AUTHOR, m_songcredits);
			DDX_Control(pDX, IDC_EDIT_TITLE, m_songtitle);
			DDX_Control(pDX, IDC_EDIT_TEMPO, m_tempo);
			DDX_Control(pDX, IDC_SPIN_TEMPO, m_tempoSpin);
			DDX_Control(pDX, IDC_EDIT_LPB, m_linespb);
			DDX_Control(pDX, IDC_SPIN_LPB, m_linespbSpin);
			DDX_Control(pDX, IDC_EDIT_TPB, m_tickspb);
			DDX_Control(pDX, IDC_SPIN_TPB, m_tickspbSpin);
			DDX_Control(pDX, IDC_EDIT_EXTRATICK, m_extraTicks);
			DDX_Control(pDX, IDC_SPIN_EXTRATICK, m_extraTicksSpin);
			DDX_Control(pDX, IDC_EDIT_REALTEMPO, m_realTempo);
			DDX_Control(pDX, IDC_EDIT_REALTPB, m_realTpb);
		}

/*
		BEGIN_MESSAGE_MAP(CSongpDlg, CDialog)
			ON_BN_CLICKED(IDOK, OnOk)
			ON_EN_CHANGE(IDC_EDIT_TEMPO, OnEnchangeTempo)
			ON_EN_CHANGE(IDC_EDIT_LPB, OnEnchangeLinesPB)
			ON_EN_CHANGE(IDC_EDIT_TPB, OnEnchangeTicksPB)
			ON_EN_CHANGE(IDC_EDIT_EXTRATICK, OnEnchangeExtraTicks)
		END_MESSAGE_MAP()
*/

		BOOL CSongpDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			m_songtitle.SetLimitText(128);
			m_songcredits.SetLimitText(64);
			m_songcomments.SetLimitText(65535);
			m_tempo.SetLimitText(3);
			m_tempoSpin.SetRange(1, 999);
			m_linespb.SetLimitText(2);
			m_linespbSpin.SetRange(1, 31);
			m_tickspb.SetLimitText(2);
			m_tickspbSpin.SetRange(1, 99);
			m_extraTicks.SetLimitText(2);
			m_extraTicksSpin.SetRange(0, 15);
			UDACCEL acc[2];
			acc[0].nSec = 0;
			acc[0].nInc = 1;
			acc[1].nSec = 1;
			acc[1].nInc = 5;
			m_tempoSpin.SetAccel(2, acc);
			UDACCEL acc2;
			acc2.nSec = 0;
			acc2.nInc = 1;
			m_linespbSpin.SetAccel(1, &acc2);
			m_tickspbSpin.SetAccel(1, &acc2);
			m_extraTicksSpin.SetAccel(1, &acc2);


			m_songtitle.SetWindowText(_pSong.name.c_str());
			m_songcredits.SetWindowText(_pSong.author.c_str());
			m_songcomments.SetWindowText(_pSong.comments.c_str());
			{ std::ostringstream os; os << _pSong.BeatsPerMin();
				m_tempo.SetWindowText(os.str().c_str());
			}
			{ std::ostringstream os; os << _pSong.LinesPerBeat();
				m_linespb.SetWindowText(os.str().c_str());
			}
			{ std::ostringstream os; os << _pSong.TicksPerBeat();
				m_tickspb.SetWindowText(os.str().c_str());
			}
			{ std::ostringstream os; os << _pSong.ExtraTicksPerLine();
				m_extraTicks.SetWindowText(os.str().c_str());
			}
			{ std::ostringstream os; os << RealBPM();
				m_realTempo.SetWindowText(os.str().c_str());
			}
			{ std::ostringstream os; os << RealTPB();
				m_realTpb.SetWindowText(os.str().c_str());
			}

			m_songtitle.SetFocus();
			m_songtitle.SetSel(0,-1);

			if ( readonlystate )
			{
				m_songtitle.SetReadOnly();
				m_songcredits.SetReadOnly();
				m_songcomments.SetReadOnly();
				m_tempo.SetReadOnly();
				m_linespb.SetReadOnly();
				m_tickspb.SetReadOnly();
				m_extraTicks.SetReadOnly();
				((CButton*)GetDlgItem(IDCANCEL))->ShowWindow(SW_HIDE);
				((CButton*)GetDlgItem(IDOK))->SetWindowText("Close");
			}
			initialized=true;

			return TRUE;
		}
		void CSongpDlg::SetReadOnly()
		{
			readonlystate=true;
		}

		void CSongpDlg::OnOk() 
		{
			if (!readonlystate)
			{
				char name[129]; char author[65]; char comments[65536]; char value[5];
				m_songtitle.GetWindowText(name,128);
				m_songcredits.GetWindowText(author,64);
				m_songcomments.GetWindowText(comments,65535);
				_pSong.name = name;
				_pSong.author = author;
				_pSong.comments = comments;

				m_tempo.GetWindowText(value,4);
				{ std::stringstream os; os << value; int val; os >> val;
					_pSong.BeatsPerMin(val);
				}
				m_linespb.GetWindowText(value,4);
				{ std::stringstream os; os << value; int val; os >> val;
					_pSong.LinesPerBeat(val);
				}
				m_tickspb.GetWindowText(value,4);
				{ std::stringstream os; os << value; int val; os >> val;
					_pSong.TicksPerBeat(val);
				}
				m_extraTicks.GetWindowText(value,4);
				{ std::stringstream os; os << value; int val; os >> val;
					_pSong.ExtraTicksPerLine(val);
				}

				CDialog::OnOK();
			}
			else CDialog::OnCancel();
		}
		void CSongpDlg::OnEnchangeTempo()
		{
			if (initialized) {
				{ std::ostringstream os; os << RealBPM();
					m_realTempo.SetWindowText(os.str().c_str());
				}
				{ std::ostringstream os; os << RealTPB();
					m_realTpb.SetWindowText(os.str().c_str());
				}
			}
		}
		void CSongpDlg::OnEnchangeLinesPB()
		{
			if (initialized) {
				{ std::ostringstream os; os << RealBPM();
					m_realTempo.SetWindowText(os.str().c_str());
				}
				{ std::ostringstream os; os << RealTPB();
					m_realTpb.SetWindowText(os.str().c_str());
				}
			}
		}
		void CSongpDlg::OnEnchangeTicksPB()
		{
			if (initialized) {
				{ std::ostringstream os; os << RealBPM();
					m_realTempo.SetWindowText(os.str().c_str());
				}
				{ std::ostringstream os; os << RealTPB();
					m_realTpb.SetWindowText(os.str().c_str());
				}
			}
		}
		void CSongpDlg::OnEnchangeExtraTicks()
		{
			if (initialized) {
				{ std::ostringstream os; os << RealBPM();
					m_realTempo.SetWindowText(os.str().c_str());
				}
				{ std::ostringstream os; os << RealTPB();
					m_realTpb.SetWindowText(os.str().c_str());
				}
			}
		}

		
		int CSongpDlg::RealBPM() 
		{
			int bpm,lpb,tpb,extra;
			char value[5];
			m_tempo.GetWindowText(value,4);
			{ std::stringstream os; os << value; os >> bpm; }
			m_linespb.GetWindowText(value,4);
			{ std::stringstream os; os << value; os >> lpb; }
			m_tickspb.GetWindowText(value,4);
			{ std::stringstream os; os << value; os >> tpb; }
			m_extraTicks.GetWindowText(value,4);
			{ std::stringstream os; os << value; os >> extra; }
			
			return (bpm*tpb)/static_cast<float>(extra*lpb+tpb);
		}
		int CSongpDlg::RealTPB()
		{
			int lpb,tpb,extra;
			char value[5];
			m_linespb.GetWindowText(value,4);
			{ std::stringstream os; os << value; os >> lpb; }
			m_tickspb.GetWindowText(value,4);
			{ std::stringstream os; os << value; os >> tpb; }
			m_extraTicks.GetWindowText(value,4);
			{ std::stringstream os; os << value; os >> extra; }

			return tpb+(extra*lpb);
		}


}}
