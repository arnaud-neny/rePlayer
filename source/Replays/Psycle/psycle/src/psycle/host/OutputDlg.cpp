///\file
///\brief implementation file for psycle::host::COutputDlg.

#include <psycle/host/detail/project.private.hpp>
#include "OutputDlg.hpp"
#include "PsycleConfig.hpp"

#include "MidiInput.hpp"
#include "AudioDriver.hpp"
#include "Player.hpp"

namespace psycle { namespace host {

		IMPLEMENT_DYNCREATE(COutputDlg, CPropertyPage)

		COutputDlg::COutputDlg() :CPropertyPage(IDD)
			, m_driverIndex(0)
			, m_midiDriverIndex(0)
			, m_syncDriverIndex(0)
			, m_oldDriverIndex(0)
			, m_oldMidiDriverIndex(0)
			, m_oldSyncDriverIndex(0)
			, m_midiHeadroom(0)
			, m_numberThreads(0)
		{
		}

		void COutputDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_DRIVER, m_driverComboBox);
			DDX_Control(pDX, IDC_MIDI_DRIVER, m_midiDriverComboBox);
			DDX_Control(pDX, IDC_MIDI_KEYBOARD, m_inmediate);
			DDX_Control(pDX, IDC_MIDI_SEQUENCED, m_sequenced);
			DDX_Control(pDX, IDC_SYNC_DRIVER, m_midiSyncComboBox);
			DDX_Control(pDX, IDC_MIDI_HEADROOM_SPIN, m_midiHeadroomSpin);
			DDX_Control(pDX, IDC_MIDI_HEADROOM, m_midiHeadroomEdit);
			DDX_Text(pDX, IDC_MIDI_HEADROOM, m_midiHeadroom);
			DDX_Control(pDX, IDC_NUMBER_THREADS, m_numberThreadsEdit);
			DDX_Text(pDX, IDC_NUMBER_THREADS, m_numberThreads);
			DDV_MinMaxInt(pDX, m_midiHeadroom, MIN_HEADROOM, MAX_HEADROOM);
		}

/*
		BEGIN_MESSAGE_MAP(COutputDlg, CDialog)
			ON_BN_CLICKED(IDC_CONFIG, OnConfig)
			ON_BN_CLICKED(IDC_MIDI_KEYBOARD, OnEnableInmediate)
			ON_BN_CLICKED(IDC_MIDI_SEQUENCED, OnEnableSequenced)
			ON_CBN_SELCHANGE(IDC_DRIVER, OnSelChangeOutput)
			ON_CBN_SELCHANGE(IDC_MIDI_DRIVER, OnSelChangeMidi)
			ON_CBN_SELCHANGE(IDC_SYNC_DRIVER, OnSelChangeSync)
		END_MESSAGE_MAP()
*/

		BOOL COutputDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			PsycleConfig& conf = PsycleGlobal::conf();
			m_driverIndex = conf.outputDriverIndex();
			m_midiDriverIndex = conf.midiDriverIndex();
			m_syncDriverIndex = conf.syncDriverIndex();

			for (int i=0; i < conf.numOutputDrivers(); i++)
			{
				const char* psDesc = conf.audioSettings[i]->GetInfo()._psName;
				m_driverComboBox.AddString(psDesc);
			}
			m_driverComboBox.SetCurSel(m_driverIndex);
			m_oldDriverIndex = m_driverIndex;

			{
				std::stringstream s;
				s << conf.GetNumThreads();
				m_numberThreads = conf.GetNumThreads();
				m_numberThreadsEdit.SetWindowText(s.str().c_str());
			}

			int _numMidiDrivers = CMidiInput::GetNumDevices();
			PopulateListbox( &m_midiDriverComboBox, _numMidiDrivers, false );
			m_midiDriverComboBox.SetCurSel(m_midiDriverIndex);
			m_oldMidiDriverIndex = m_midiDriverIndex;

			PopulateListbox( &m_midiSyncComboBox, _numMidiDrivers, true );
			m_midiSyncComboBox.SetCurSel(m_syncDriverIndex);
			m_oldSyncDriverIndex = m_syncDriverIndex;


			// setup spinner
			{
				std::stringstream s;
				s << conf.midi()._midiHeadroom;
				m_midiHeadroom = conf.midi()._midiHeadroom;
				m_midiHeadroomEdit.SetWindowText(s.str().c_str());
			}
			m_midiHeadroomSpin.SetRange32(MIN_HEADROOM, MAX_HEADROOM);
			UDACCEL acc;
			acc.nSec = 0;
			acc.nInc = 50;
			m_midiHeadroomSpin.SetAccel(1, &acc);

			if(conf.midi()._midiMachineViewSeqMode) {
				m_sequenced.SetCheck(TRUE);
				EnableClockOptions();
			}
			else {
				m_inmediate.SetCheck(TRUE);
				DisableClockOptions();
			}

			return TRUE;
		}

		void COutputDlg::OnSelChangeOutput()
		{
			m_driverIndex = m_driverComboBox.GetCurSel();
			PsycleGlobal::conf().OutputChanged(m_driverIndex);
		}

		void COutputDlg::OnSelChangeMidi()
		{
			m_midiDriverIndex = m_midiDriverComboBox.GetCurSel();
			PsycleGlobal::conf().MidiChanged(m_midiDriverIndex);
		}

		void COutputDlg::OnSelChangeSync()
		{
			m_syncDriverIndex = m_midiSyncComboBox.GetCurSel();
			PsycleGlobal::conf().SyncChanged(m_syncDriverIndex);
		}

		void COutputDlg::OnOK() 
		{
			PsycleConfig::Midi& config = PsycleGlobal::conf().midi();
			config._midiHeadroom = m_midiHeadroom;
			config._midiMachineViewSeqMode = m_sequenced.GetCheck();
			PsycleGlobal::conf().SetNumThreads(m_numberThreads);
			CDialog::OnOK();
		}

		void COutputDlg::OnCancel() 
		{
			PsycleConfig& config = PsycleGlobal::conf();
			if( m_oldDriverIndex != m_driverIndex )
			{
				config.OutputChanged(m_oldDriverIndex);
			}
			if( m_oldMidiDriverIndex != m_midiDriverIndex )
			{
				config.MidiChanged(m_oldMidiDriverIndex);
			}
			if( m_oldSyncDriverIndex != m_syncDriverIndex )
			{
				config.SyncChanged(m_oldSyncDriverIndex);
			}
		}

		void COutputDlg::OnConfig() 
		{
			PsycleGlobal::conf()._pOutputDriver->Configure();
			Global::player().SetSampleRate(Global::configuration()._pOutputDriver->GetSamplesPerSec());
		}
		void COutputDlg::OnEnableInmediate()
		{
			DisableClockOptions();
		}
		void COutputDlg::OnEnableSequenced()
		{
			EnableClockOptions();
		}
		void COutputDlg::EnableClockOptions()
		{
			((CButton*)GetDlgItem(IDC_TEXT_CLOCKDEVICE))->EnableWindow(TRUE);
			((CButton*)GetDlgItem(IDC_TEXT_HEADROOM))->EnableWindow(TRUE);
			m_midiSyncComboBox.EnableWindow(TRUE);
			m_midiHeadroomEdit.EnableWindow(TRUE);
			m_midiHeadroomSpin.EnableWindow(TRUE);
		}
		void COutputDlg::DisableClockOptions()
		{
			((CButton*)GetDlgItem(IDC_TEXT_CLOCKDEVICE))->EnableWindow(FALSE);
			((CButton*)GetDlgItem(IDC_TEXT_HEADROOM))->EnableWindow(FALSE);
			m_midiSyncComboBox.EnableWindow(FALSE);
			m_midiHeadroomEdit.EnableWindow(FALSE);
			m_midiHeadroomSpin.EnableWindow(FALSE);
		}

		void COutputDlg::PopulateListbox( CComboBox * listbox, int numDevs, bool issync )
		{
			// clear listbox
			listbox->ResetContent();

			// always add the null dev
			listbox->AddString( issync?"Same as MIDI Input Device":"None" );

			// add each to the listbox
			for( int idx = 0; idx < numDevs; idx++ )
			{
				std::string desc;
				CMidiInput::GetDeviceDesc(idx, desc );
				listbox->AddString( desc.c_str() );
			}
		}
	}   // namespace
}   // namespace
