///\file
///\brief implementation file for psycle::host::CInstrumentEditor.
#include <psycle/host/detail/project.private.hpp>
#include "InstrumentEditor.hpp"
#include "XMInstrument.hpp"
#include "MainFrm.hpp"
#include <psycle/helpers/filter.hpp>
#include <psycle/host/Song.hpp>
namespace psycle { namespace host {


IMPLEMENT_DYNAMIC(CInstrumentEditor, CPropertyPage)

		CInstrumentEditor::CInstrumentEditor()
		: CPropertyPage(CInstrumentEditor::IDD)
		, m_bInitialized(false)
		{
		}

		CInstrumentEditor::~CInstrumentEditor()
		{
		}

		void CInstrumentEditor::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_NOTETUNE, m_notelabel);
			DDX_Control(pDX, IDC_SLIDERFINE, m_finetune);
			DDX_Control(pDX, IDC_LOOPEDIT, m_loopedit);
			DDX_Control(pDX, IDC_CHECK4, m_loopcheck);
			DDX_Control(pDX, IDC_RRES, m_rres_check);
			DDX_Control(pDX, IDC_PANSLIDER, m_panslider);			
			DDX_Control(pDX, IDC_RPAN, m_rpan_check);
			DDX_Control(pDX, IDC_RCUT, m_rcut_check);
			DDX_Control(pDX, IDC_NNA_COMBO, m_nna_combo);			
			DDX_Control(pDX, IDC_SAMPINST_CMB, m_sampins_combo);			
			DDX_Control(pDX, IDC_LOCKINST, m_lockinst);
			DDX_Control(pDX, IDC_LOCKINSTCMB, m_lockinst_combo);
			DDX_Control(pDX, IDC_VIRTINST, m_virtinst);
			DDX_Control(pDX, IDC_VIRTINSTCMB, m_virtinst_combo);
			DDX_Control(pDX, IDC_SLIDERVOL, m_volumebar);
			DDX_Control(pDX, IDC_WAVELENGTH, m_wlen);
			DDX_Control(pDX, IDC_LOOPSTART, m_loopstart);
			DDX_Control(pDX, IDC_LOOPEND, m_loopend);
			DDX_Control(pDX, IDC_LOOPTYPE, m_looptype);
			DDX_Control(pDX, IDC_COMBOFILTER, m_filtercombo);
			DDX_Control(pDX, IDC_SLIDER_FRES, m_q_slider);
			DDX_Control(pDX, IDC_SLIDER_FCUT, m_cutoff_slider);
			DDX_Control(pDX, IDC_SLIDER_AMPREL, m_a_release_slider);
			DDX_Control(pDX, IDC_SLIDER_AMPSUS, m_a_sustain_slider);
			DDX_Control(pDX, IDC_SLIDER_AMPDEC, m_a_decay_slider);
			DDX_Control(pDX, IDC_SLIDER_AMPATT, m_a_attack_slider);
			DDX_Control(pDX, IDC_SLIDER_FILREL, m_f_release_slider);
			DDX_Control(pDX, IDC_SLIDER_FILSUS, m_f_sustain_slider);
			DDX_Control(pDX, IDC_SLIDER_FILDEC, m_f_decay_slider);
			DDX_Control(pDX, IDC_SLIDER_FILATT, m_f_attack_slider);
			DDX_Control(pDX, IDC_SLIDER_FILAMT, m_f_amount_slider);
			DDX_Control(pDX, IDC_AMPFRAME, m_ampframe);
			DDX_Control(pDX, IDC_FILFRAME, m_filframe);
		}

/*
		BEGIN_MESSAGE_MAP(CInstrumentEditor, CDialog)
			ON_WM_HSCROLL()
			ON_BN_CLICKED(IDC_LOOPOFF, OnLoopoff)
			ON_BN_CLICKED(IDC_LOOPFORWARD, OnLoopforward)
			ON_BN_CLICKED(IDC_LOCKINST, OnLockinst)
			ON_CBN_SELCHANGE(IDC_LOCKINSTCMB, OnSelchangeLockInstCombo)
			ON_BN_CLICKED(IDC_VIRTINST, OnVirtualinst)
			ON_CBN_SELCHANGE(IDC_VIRTINSTCMB, OnSelchangeVirtInstCombo)
			ON_CBN_SELCHANGE(IDC_NNA_COMBO, OnSelchangeNnaCombo)
			ON_CBN_SELENDOK(IDC_SAMPINST_CMB, OnCbnSelendokInstrument)
			ON_BN_CLICKED(IDC_INST_DECONE, OnPrevInstrument)
			ON_BN_CLICKED(IDC_INST_ADDONE, OnNextInstrument)
			ON_BN_DOUBLECLICKED(IDC_INST_DECONE, OnPrevInstrument)
			ON_BN_DOUBLECLICKED(IDC_INST_ADDONE, OnNextInstrument)
			ON_BN_CLICKED(IDC_RPAN, OnRpan)
			ON_BN_CLICKED(IDC_RCUT, OnRcut)
			ON_BN_CLICKED(IDC_RRES, OnRres)
			ON_BN_CLICKED(IDC_CHECK4, OnLoopCheck)
			ON_EN_CHANGE(IDC_LOOPEDIT, OnChangeLoopedit)
			ON_BN_CLICKED(IDC_KILL_INSTRUMENT, OnKillInstrument)
			ON_BN_CLICKED(IDC_INS_DECOCTAVE, OnInsDecoctave)
			ON_BN_CLICKED(IDC_INS_DECNOTE, OnInsDecnote)
			ON_BN_CLICKED(IDC_INS_INCNOTE, OnInsIncnote)
			ON_BN_CLICKED(IDC_INS_INCOCTAVE, OnInsIncoctave)
			ON_CBN_SELCHANGE(IDC_COMBOFILTER, OnSelchangeFilterType)
			ON_NOTIFY_RANGE(NM_CUSTOMDRAW, IDC_SLIDERVOL, IDC_SLIDER_FRES, OnCustomdrawSliderm)
			ON_MESSAGE(PSYC_ENVELOPE_CHANGED, OnEnvelopeChanged)
		END_MESSAGE_MAP()
*/

		
		BOOL CInstrumentEditor::PreTranslateMessage(MSG* pMsg) 
		{
			Machine *tmac = Global::song().GetSamplerIfExists();
			InstrumentEditorUI* parent = dynamic_cast<InstrumentEditorUI*>(GetParent());
			BOOL res = parent->PreTranslateChildMessage(pMsg, GetFocus()->GetSafeHwnd(), tmac);
			if (res == FALSE ) return CPropertyPage::PreTranslateMessage(pMsg);
			return res;
		}

		BOOL CInstrumentEditor::OnInitDialog() 
		{
			CPropertyPage::OnInitDialog();
		
			((CButton*)GetDlgItem(IDC_INST_DECONE))->SetIcon(PsycleGlobal::conf().iconless);
			((CButton*)GetDlgItem(IDC_INST_ADDONE))->SetIcon(PsycleGlobal::conf().iconmore);

			for (int i=MAX_MACHINES;i<MAX_VIRTUALINSTS;i++) {
				std::ostringstream os;
				os << std::hex << std::uppercase << i;
				int idx = m_virtinst_combo.AddString(os.str().c_str());
				m_virtinst_combo.SetItemData(idx, i);
			}
			m_nna_combo.AddString("Note Cut");
			m_nna_combo.AddString("Note Release");
			m_nna_combo.AddString("None");

			m_filtercombo.AddString("LowPass 2P (old)");
			m_filtercombo.AddString("HighPass 2P (old)");
			m_filtercombo.AddString("BandPass 2P (old)");
			m_filtercombo.AddString("NotchBand 2P (old)");
			m_filtercombo.AddString("None");
			m_filtercombo.AddString("Unused");
			m_filtercombo.AddString("Unused");
			m_filtercombo.AddString("Unused");
			m_filtercombo.AddString("LowPass 2P (new)");
			m_filtercombo.AddString("HighPass 2P (new)");
			m_filtercombo.AddString("BandPass 2P (new)");
			m_filtercombo.AddString("NotchBand 2P (new)");

			m_cutoff_slider.SetRange(0,127);
			m_q_slider.SetRange(0,127);

			m_volumebar.SetRange(0,256*4);
			m_volumebar.SetTicFreq(16*4);
			m_finetune.SetRange(-100,100);
			//Hack to fix "0 placed on leftmost on start".
			m_finetune.SetPos(-100);
			m_panslider.SetRange(0,128);

			// Set slider ranges (number of 5 milliseconds)
			m_a_attack_slider.SetRange(1,1000);
			m_a_decay_slider.SetRange(1,1000);
			m_a_sustain_slider.SetRange(0,100);
			m_a_release_slider.SetRange(1,2000);

			m_f_attack_slider.SetRange(1,1000);
			m_f_decay_slider.SetRange(1,1000);
			m_f_sustain_slider.SetRange(0,128);
			m_f_release_slider.SetRange(1,2000);
				
			m_f_amount_slider.SetRange(-128,128);
			//Hack to fix "0 placed on leftmost on start".
			m_f_amount_slider.SetPos(-128);
			
			m_ampEnv.Clear();
			m_ampEnv.Mode(XMInstrument::Envelope::Mode::MILIS);
			m_ampframe.envelopeIdx(0);
			m_ampframe.canSustain(false);
			m_ampframe.canLoop(false);
			m_ampframe.Initialize(m_ampEnv, 0, 5);
			m_ampframe.ConvertToADSR();

			m_filEnv.Clear();
			m_filEnv.Mode(XMInstrument::Envelope::Mode::MILIS);
			m_filframe.envelopeIdx(1);
			m_filframe.canSustain(false);
			m_filframe.canLoop(false);
			m_filframe.Initialize(m_filEnv,0,5);
			m_filframe.ConvertToADSR();
			return TRUE;
		}

		//////////////////////////////////////////////////////////////////////
		// Auxiliary members
		BOOL CInstrumentEditor::OnSetActive()
		{
			TRACE("in CInstrumentEditor:setActive\n");
			UpdateWavesCombo();
			SetInstrument(Global::song().waveSelected);
			m_bInitialized=true;
			return CPropertyPage::OnSetActive();
		}
		void CInstrumentEditor::WaveUpdate()
		{
			if (m_bInitialized) {
				UpdateWavesCombo();
				SetInstrument(Global::song().waveSelected);
			}
		}
		void CInstrumentEditor::SetInstrument(int si)
		{
			char buffer[64];
			Song &song = Global::song();
			Instrument *pins = song._pInstrument[si];
			XMInstrument::WaveData<> wavetmp;
			bool enabled = song.samples.IsEnabled(si);
			const XMInstrument::WaveData<>& wave = (enabled) ? song.samples[si] : wavetmp;

			UpdateVirtInstOptions();

			m_lockinst_combo.ResetContent();
			for (int i=0;i<MAX_BUSES;i++) {
				if (song._pMachine[i] != NULL && song._pMachine[i]->_type == MACH_SAMPLER) {
					std::ostringstream os;
					os << std::hex << std::uppercase << i << std::nouppercase << ": "<< song._pMachine[i]->_editName;
					int idx = m_lockinst_combo.AddString(os.str().c_str());
					m_lockinst_combo.SetItemData(idx, i);
				}
			}

			UpdateComboNNA();

			bool const ils = pins->_loop;
			m_loopcheck.SetCheck(ils);
			sprintf(buffer,"%d",pins->_lines);
			m_loopedit.EnableWindow(ils);
			m_loopedit.SetWindowText(buffer);

			m_rpan_check.SetCheck(pins->_RPAN);
			m_rcut_check.SetCheck(pins->_RCUT);
			m_rres_check.SetCheck(pins->_RRES);

			m_filtercombo.SetCurSel(pins->ENV_F_TP);
			m_cutoff_slider.SetPos(pins->ENV_F_CO);
			m_q_slider.SetPos(pins->ENV_F_RQ);


			// Volume bar
			int volpos = helpers::dsp::AmountToSliderHoriz(wave.WaveGlobVolume());
			m_volumebar.SetPos(volpos);
			m_finetune.SetPos(wave.WaveFineTune());
			m_panslider.SetPos(wave.PanFactor()*128);

			UpdateNoteLabel();	
			
			// Set looptype
			if(wave.WaveLoopType() == XMInstrument::WaveData<>::LoopType::NORMAL) sprintf(buffer,"Forward");
			else sprintf(buffer,"Off");
			
			m_looptype.SetWindowText(buffer);

			// Display Loop Points & Wave Length
			sprintf(buffer,"%d",wave.WaveLoopStart());
			m_loopstart.SetWindowText(buffer);

			sprintf(buffer,"%d",wave.WaveLoopEnd());
			m_loopend.SetWindowText(buffer);

			sprintf(buffer,"%d",wave.WaveLength());
			m_wlen.SetWindowText(buffer);

			RefreshEnvelopes();
		}

		void CInstrumentEditor::UpdateVirtInstOptions()
		{
			const int si = Global::song().waveSelected;
			Instrument *pins = Global::song()._pInstrument[si];
			int virtualIndex = Global::song().VirtualInstrumentInverted(si,false);
			if (virtualIndex != -1) {
				m_virtinst.SetCheck(BST_CHECKED);
				m_lockinst.SetCheck(BST_UNCHECKED);
				m_virtinst_combo.EnableWindow(true);
				m_lockinst_combo.EnableWindow(true);

				Song::macinstpair pair = Global::song().VirtualInstrument(virtualIndex);
				SelectByData(m_virtinst_combo,virtualIndex);
				SelectByData(m_lockinst_combo,pair.first);
			}
			else if (pins->_LOCKINST) {
				m_lockinst.SetCheck(BST_CHECKED);
				m_lockinst_combo.EnableWindow(true);
				m_virtinst_combo.EnableWindow(false);
				SelectByData(m_lockinst_combo,pins->sampler_to_use);
				m_virtinst_combo.SetCurSel(-1);
			}
			else {
				m_virtinst.SetCheck(BST_UNCHECKED);
				m_lockinst.SetCheck(BST_UNCHECKED);
				m_lockinst_combo.EnableWindow(false);
				m_virtinst_combo.EnableWindow(false);
				m_lockinst_combo.SetCurSel(-1);
				m_virtinst_combo.SetCurSel(-1);
			}
		}
		void CInstrumentEditor::UpdateWavesCombo()
		{
			char line[48];
			const SampleList& list = Global::song().samples;
			m_sampins_combo.ResetContent();
			for (int i=0;i<list.size();i++)
			{
				if (list.Exists(i)) {
					const XMInstrument::WaveData<>& wave = list[i];
					sprintf(line,"%02X%s: ",i,wave.WaveLength()>0?"*":" ");
					strncat(line,wave.WaveName().c_str(),40);
				}
				else {
					sprintf(line,"%02X : ",i);
				}
				m_sampins_combo.AddString(line);
			}
			m_sampins_combo.SetCurSel(Global::song().waveSelected);
		}
		void CInstrumentEditor::RefreshEnvelopes()
		{
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			// Update sliders (envelope graphic is updated from the oncustomdrawslider event).
			m_a_attack_slider.SetPos(pins->ENV_AT/220.5);//Reduced original range from samples to 5 milliseconds
			m_a_decay_slider.SetPos(pins->ENV_DT/220.5);//Reduced original range from samples to 5 milliseconds
			m_a_sustain_slider.SetPos(pins->ENV_SL);
			m_a_release_slider.SetPos(pins->ENV_RT/220.5);//Reduced original range from samples to 5 milliseconds

			m_f_attack_slider.SetPos(pins->ENV_F_AT/220.5);//Reduced original range from samples to 5 milliseconds
			m_f_decay_slider.SetPos(pins->ENV_F_DT/220.5);//Reduced original range from samples to 5 milliseconds
			m_f_sustain_slider.SetPos(pins->ENV_F_SL);
			m_f_release_slider.SetPos(pins->ENV_F_RT/220.5);//Reduced original range from samples to 5 milliseconds
			
			m_f_amount_slider.SetPos(pins->ENV_F_EA);
		}

		LRESULT CInstrumentEditor::OnEnvelopeChanged(WPARAM wParam, LPARAM lParam)
		{
		    UNREFERENCED_PARAMETER(lParam);
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			int envIdx = wParam;
			if (envIdx == 0) { //amp
				pins->ENV_AT = m_ampframe.AttackTime()*220.5;
				pins->ENV_DT = m_ampframe.DecayTime()*220.5;
				pins->ENV_SL = m_ampframe.SustainValue()*100.f;
				pins->ENV_RT = m_ampframe.ReleaseTime()*220.5;
				// Ensure values different than zero (it is used in a division)
				if (pins->ENV_AT == 0 ) pins->ENV_AT = 1;
				if (pins->ENV_DT == 0 ) pins->ENV_DT = 1;
				if (pins->ENV_RT == 0 ) pins->ENV_RT = 1;
			}
			else { //filter
				pins->ENV_F_AT = m_filframe.AttackTime()*220.5;
				pins->ENV_F_DT = m_filframe.DecayTime()*220.5;
				pins->ENV_F_SL = value_mapper::map_1_128<int>(m_filframe.SustainValue());
				pins->ENV_F_RT = m_filframe.ReleaseTime()*220.5;
				// Ensure values different than zero (it is used in a division)
				if (pins->ENV_F_AT == 0 ) pins->ENV_F_AT = 1;
				if (pins->ENV_F_DT == 0 ) pins->ENV_F_DT = 1;
				if (pins->ENV_F_RT == 0 ) pins->ENV_F_RT = 1;
			}
			RefreshEnvelopes();

			return 0;
		}

		//////////////////////////////////////////////////////////////////////
		// Sliders handler
		void CInstrumentEditor::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
		{
			CSliderCtrl* the_slider = reinterpret_cast<CSliderCtrl*>(pScrollBar);
			int uId = the_slider->GetDlgCtrlID();

			switch(nSBCode){
			case TB_BOTTOM: //fallthrough
			case TB_LINEDOWN: //fallthrough
			case TB_PAGEDOWN: //fallthrough
			case TB_TOP: //fallthrough
			case TB_LINEUP: //fallthrough
			case TB_PAGEUP: //fallthrough
			case TB_THUMBPOSITION: //fallthrough
			case TB_THUMBTRACK:
				if (uId == IDC_SLIDER_FCUT) { SliderFilterCut(*the_slider); }
				else if (uId == IDC_SLIDER_FRES) { SliderFilterRes(*the_slider); }
				else if (uId == IDC_SLIDERVOL) { SliderVolume(*the_slider); }
				else if (uId == IDC_PANSLIDER) { SliderPan(*the_slider); }
				else if (uId == IDC_SLIDERFINE) { SliderFinetune(*the_slider); }
				else if (uId == IDC_SLIDER_AMPATT) { SliderAmpAtt(*the_slider); }
				else if (uId == IDC_SLIDER_AMPDEC) { SliderAmpDec(*the_slider); }
				else if (uId == IDC_SLIDER_AMPSUS) { SliderAmpSus(*the_slider); }
				else if (uId == IDC_SLIDER_AMPREL) { SliderAmpRel(*the_slider); }
				else if (uId == IDC_SLIDER_FILATT) { SliderFilterAtt(*the_slider); }
				else if (uId == IDC_SLIDER_FILDEC) { SliderFilterDec(*the_slider); }
				else if (uId == IDC_SLIDER_FILSUS) { SliderFilterSus(*the_slider); }
				else if (uId == IDC_SLIDER_FILREL) { SliderFilterRel(*the_slider); }
				else if (uId == IDC_SLIDER_FILAMT) { SliderFilterMod(*the_slider); }
				break;
			case TB_ENDTRACK:
				break;
			}
			CPropertyPage::OnHScroll(nSBCode, nPos, pScrollBar);
		}

		void CInstrumentEditor::SliderFilterCut(CSliderCtrl& the_slider)
		{
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			pins->ENV_F_CO = the_slider.GetPos();
			//Force update of q value.
			int tmp = m_q_slider.GetPos();
			m_q_slider.SetPos(127-tmp);
			m_q_slider.SetPos(tmp);
		}
		void CInstrumentEditor::SliderFilterRes(CSliderCtrl& the_slider)
		{
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			pins->ENV_F_RQ = the_slider.GetPos();
		}
		void CInstrumentEditor::SliderVolume(CSliderCtrl& the_slider)
		{
			int si = Global::song().waveSelected;
			if (Global::song().samples.Exists(si)) {
				float invol = helpers::dsp::SliderToAmountHoriz(the_slider.GetPos());
				Global::song().samples.get(si).WaveGlobVolume(invol);
			}
		}
		void CInstrumentEditor::SliderPan(CSliderCtrl& the_slider)
		{
			int si = Global::song().waveSelected;
			if (Global::song().samples.Exists(si)) {
				 Global::song().samples.get(si).PanFactor(value_mapper::map_128_1(the_slider.GetPos()));
			}
		}
		void CInstrumentEditor::SliderFinetune(CSliderCtrl& the_slider)
		{
			int si = Global::song().waveSelected;
			if (Global::song().samples.Exists(si)) {
				 Global::song().samples.get(si).WaveFineTune(the_slider.GetPos());
			}
		}
		void CInstrumentEditor::SliderAmpAtt(CSliderCtrl& the_slider)
		{
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			pins->ENV_AT = the_slider.GetPos()*220.5;//Reduced original range from samples to 5 milliseconds
			// Ensure values different than zero (it is used in a division)
			if (pins->ENV_AT == 0 ) pins->ENV_AT = 1;
		}
		void CInstrumentEditor::SliderAmpDec(CSliderCtrl& the_slider)
		{
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			pins->ENV_DT = the_slider.GetPos()*220.5;//Reduced original range from samples to 5 milliseconds
			// Ensure values different than zero (it is used in a division)
			if (pins->ENV_DT == 0 ) pins->ENV_DT = 1;
		}
		void CInstrumentEditor::SliderAmpSus(CSliderCtrl& the_slider)
		{
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			pins->ENV_SL = the_slider.GetPos();
		}
		void CInstrumentEditor::SliderAmpRel(CSliderCtrl& the_slider)
		{
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			pins->ENV_RT = the_slider.GetPos()*220.5;//Reduced original range from samples to 5 milliseconds
			// Ensure values different than zero (it is used in a division)
			if (pins->ENV_RT == 0 ) pins->ENV_RT = 1;
		}
		void CInstrumentEditor::SliderFilterAtt(CSliderCtrl& the_slider)
		{
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			pins->ENV_F_AT = the_slider.GetPos()*220.5;//Reduced original range from samples to 5 milliseconds
			// Ensure values different than zero (it is used in a division)
			if (pins->ENV_F_AT == 0 ) pins->ENV_F_AT = 1;
		}
		void CInstrumentEditor::SliderFilterDec(CSliderCtrl& the_slider)
		{
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			pins->ENV_F_DT = the_slider.GetPos()*220.5;//Reduced original range from samples to 5 milliseconds
			// Ensure values different than zero (it is used in a division)
			if (pins->ENV_F_DT == 0 ) pins->ENV_F_DT = 1;
		}
		void CInstrumentEditor::SliderFilterSus(CSliderCtrl& the_slider)
		{
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			pins->ENV_F_SL = the_slider.GetPos();
		}
		void CInstrumentEditor::SliderFilterRel(CSliderCtrl& the_slider)
		{
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			pins->ENV_F_RT = the_slider.GetPos()*220.5;//Reduced original range from samples to 5 milliseconds
			// Ensure values different than zero (it is used in a division)
			if (pins->ENV_F_RT == 0 ) pins->ENV_F_RT = 1;
		}
		void CInstrumentEditor::SliderFilterMod(CSliderCtrl& the_slider)
		{
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			pins->ENV_F_EA = the_slider.GetPos();
		}

		//////////////////////////////////////////////////////////////////////
		// GUI Handlers
		void CInstrumentEditor::OnCbnSelendokInstrument()
		{
			const int si=m_sampins_combo.GetCurSel();
			if (si != Global::song().waveSelected) {
				CMainFrame* win = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
				win->ChangeWave(si);
				win->UpdateComboIns(false);
			}
		}

		void CInstrumentEditor::OnPrevInstrument() 
		{
			const int si=Global::song().waveSelected;
			if(si>0)
			{
				CMainFrame* win = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
				win->ChangeWave(si-1);
				win->UpdateComboIns(false);
			}
		}

		void CInstrumentEditor::OnNextInstrument() 
		{
			const int si=Global::song().waveSelected;
			if(si<MAX_INSTRUMENTS)
			{
				CMainFrame* win = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
				if (Global::song().samples.size() <= si+1) {
					XMInstrument::WaveData<> wave;
					wave.Init();
					Global::song().samples.SetSample(wave,si+1);
					UpdateWavesCombo();
					win->ChangeWave(si+1);
					win->UpdateComboIns(true);
				}
				else {
					win->ChangeWave(si+1);
					win->UpdateComboIns(false);
				}
			}
		}
		
		void CInstrumentEditor::OnKillInstrument() 
		{
			{
				CExclusiveLock lock(&Global::song().semaphore, 2, true);
				Global::song().DeleteInstrument(Global::song().waveSelected);
			}
			CMainFrame* win = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
			win->UpdateComboIns();
			win->WaveEditorBackUpdate();
			WaveUpdate();
			win->RedrawGearRackList();
		}


		void CInstrumentEditor::OnLockinst()
		{
			bool lockselected = static_cast<bool>(m_lockinst.GetCheck());
			bool virtselected = static_cast<bool>(m_virtinst.GetCheck());
			int si = Global::song().waveSelected;
			m_lockinst_combo.EnableWindow(lockselected);
			Global::song()._pInstrument[si]->_LOCKINST = lockselected;

			if (virtselected) {
				m_virtinst.SetCheck(BST_UNCHECKED);
				m_virtinst_combo.EnableWindow(false);
				m_virtinst_combo.SetCurSel(-1);
				int virtualIndex = Global::song().VirtualInstrumentInverted(si,false);
				Global::song().DeleteVirtualInstrument(virtualIndex);
				CMainFrame* win = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
				win->UpdateComboGen(true);
			}
			if (!lockselected) {
				m_lockinst_combo.SetCurSel(-1);
				Global::song()._pInstrument[si]->sampler_to_use=-1;
			}
		}
		void CInstrumentEditor::OnVirtualinst()
		{
			bool lockselected = static_cast<bool>(m_lockinst.GetCheck());
			bool virtselected = static_cast<bool>(m_virtinst.GetCheck());
			int si = Global::song().waveSelected;
			m_lockinst_combo.EnableWindow(virtselected);
			m_virtinst_combo.EnableWindow(virtselected);

			if (lockselected) {
				m_lockinst.SetCheck(BST_UNCHECKED);
			}
			if (!virtselected) {
				int virtualIndex = Global::song().VirtualInstrumentInverted(si,false);
				m_lockinst_combo.SetCurSel(-1);
				m_virtinst_combo.SetCurSel(-1);
				Global::song().DeleteVirtualInstrument(virtualIndex);
			}
			CMainFrame* win = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
			win->UpdateComboGen(true);
		}

		void CInstrumentEditor::OnSelchangeLockInstCombo()
		{
			bool lockselected = static_cast<bool>(m_lockinst.GetCheck());
			bool virtselected = static_cast<bool>(m_virtinst.GetCheck());
			int si = Global::song().waveSelected;
			DWORD_PTR macIdx = m_lockinst_combo.GetItemData(m_lockinst_combo.GetCurSel());
			if (macIdx != CB_ERR)
			{
				if (lockselected) {
					Global::song()._pInstrument[si]->sampler_to_use = macIdx;
				}
				else if (virtselected) {
					DWORD_PTR instIdx = m_virtinst_combo.GetItemData(m_virtinst_combo.GetCurSel());
					if (instIdx != CB_ERR ) {
						Global::song().SetVirtualInstrument(instIdx,macIdx,si);
					}
				}
			}
			else {
				int virtualIndex = Global::song().VirtualInstrumentInverted(si,false);
				m_lockinst_combo.SetCurSel(-1);
				m_virtinst_combo.SetCurSel(-1);
				Global::song().DeleteVirtualInstrument(virtualIndex);
			}
			CMainFrame* win = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
			win->UpdateComboGen(true);
		}
		void CInstrumentEditor::OnSelchangeVirtInstCombo()
		{
			int si = Global::song().waveSelected;
			DWORD_PTR instIdx = m_virtinst_combo.GetItemData(m_virtinst_combo.GetCurSel());
			if (instIdx != CB_ERR) {
				Song::macinstpair pairinst = Global::song().VirtualInstrument(instIdx);
				if (pairinst.first != -1 ) {
					int result = MessageBox("Warning! This virtual instrument is already used. If you confirm, the previous association will be lost","Set virtual instrument",MB_YESNO | MB_ICONEXCLAMATION);
					if ( result == IDNO) {
						m_virtinst_combo.SetCurSel(-1);
						return;
					}
				}
				DWORD_PTR macIdx = m_lockinst_combo.GetItemData(m_lockinst_combo.GetCurSel());
				if ( macIdx != CB_ERR ) {
					Global::song().SetVirtualInstrument(instIdx,macIdx,si);
				}
			}
			else {
				int virtualIndex = Global::song().VirtualInstrumentInverted(si,false);
				m_lockinst_combo.SetCurSel(-1);
				m_virtinst_combo.SetCurSel(-1);
				Global::song().DeleteVirtualInstrument(virtualIndex);
			}
		}


		void CInstrumentEditor::OnSelchangeNnaCombo() 
		{
			Global::song()._pInstrument[Global::song().waveSelected]->_NNA = m_nna_combo.GetCurSel();	
		}

		void CInstrumentEditor::OnLoopCheck() 
		{
			int si=Global::song().waveSelected;
			bool looped = static_cast<bool>(m_loopcheck.GetCheck());
			Global::song()._pInstrument[si]->_loop = looped;
			m_loopedit.EnableWindow(looped);
		}

		void CInstrumentEditor::OnChangeLoopedit() 
		{
			int si = Global::song().waveSelected;
			CString buffer;
			m_loopedit.GetWindowText(buffer);
			Global::song()._pInstrument[si]->_lines = atoi(buffer);
			if (Global::song()._pInstrument[si]->_lines < 1)
			{
				Global::song()._pInstrument[si]->_lines = 1;
			}
		}

		void CInstrumentEditor::OnRpan() 
		{
			int si = Global::song().waveSelected;
			Global::song()._pInstrument[si]->_RPAN = static_cast<bool>(m_rpan_check.GetCheck());
		}

		void CInstrumentEditor::OnRcut() 
		{
			int si=Global::song().waveSelected;
			Global::song()._pInstrument[si]->_RCUT = static_cast<bool>(m_rcut_check.GetCheck());
		}

		void CInstrumentEditor::OnRres() 
		{
			int si=Global::song().waveSelected;
			
			Global::song()._pInstrument[si]->_RRES = static_cast<bool>(m_rres_check.GetCheck());
		}


		void CInstrumentEditor::OnSelchangeFilterType() 
		{
			Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
			pins->ENV_F_TP = static_cast<dsp::FilterType>(m_filtercombo.GetCurSel());
			//Force update of freq and q value.
			int tmp = m_cutoff_slider.GetPos();
			m_cutoff_slider.SetPos(127-tmp);
			m_cutoff_slider.SetPos(tmp);
			tmp = m_q_slider.GetPos();
			m_q_slider.SetPos(127-tmp);
			m_q_slider.SetPos(tmp);
		}

		void CInstrumentEditor::OnInsDecoctave() 
		{
			const int si=Global::song().waveSelected;
			if (Global::song().samples.Exists(si)) {
				XMInstrument::WaveData<>& wave = Global::song().samples.get(si);

				if ( wave.WaveTune()>-37)
					wave.WaveTune(wave.WaveTune()-12);
				else wave.WaveTune(-48);
				UpdateNoteLabel();
			}
		}

		void CInstrumentEditor::OnInsDecnote() 
		{
			const int si=Global::song().waveSelected;
			if (Global::song().samples.Exists(si)) {
				XMInstrument::WaveData<>& wave = Global::song().samples.get(si);
				if ( wave.WaveTune()>-47)
					wave.WaveTune(wave.WaveTune()-1);
				else wave.WaveTune(-48);
				UpdateNoteLabel();	
			}
		}

		void CInstrumentEditor::OnInsIncnote() 
		{
			const int si=Global::song().waveSelected;
			if (Global::song().samples.Exists(si)) {
				XMInstrument::WaveData<>& wave = Global::song().samples.get(si);
				if ( wave.WaveTune()<70)
					wave.WaveTune(wave.WaveTune()+1);
				else wave.WaveTune(71);
				UpdateNoteLabel();	
			}
		}

		void CInstrumentEditor::OnInsIncoctave() 
		{
			const int si=Global::song().waveSelected;
			if (Global::song().samples.Exists(si)) {
				XMInstrument::WaveData<>& wave = Global::song().samples.get(si);
				if ( wave.WaveTune()<60)
					wave.WaveTune(wave.WaveTune()+12);
				else wave.WaveTune(71);
				UpdateNoteLabel();	
			}
		}

		void CInstrumentEditor::OnLoopoff() 
		{
			int si = Global::song().waveSelected;
			if (Global::song().samples.Exists(si)) {
				XMInstrument::WaveData<>& wave = Global::song().samples.get(si);
				if(wave.WaveLoopType() == XMInstrument::WaveData<>::LoopType::NORMAL)
				{
					wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::DO_NOT);
					WaveUpdate();
				}
			}
		}

		void CInstrumentEditor::OnLoopforward() 
		{
			int si=Global::song().waveSelected;
			if (Global::song().samples.Exists(si)) {
				XMInstrument::WaveData<>& wave = Global::song().samples.get(si);
				if(wave.WaveLoopType() == XMInstrument::WaveData<>::LoopType::DO_NOT)
				{
					wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
					WaveUpdate();
				}
			}
		}

		
		void CInstrumentEditor::UpdateComboNNA() 
		{
			switch(Global::song()._pInstrument[Global::song().waveSelected]->_NNA)
			{
			case 0:m_nna_combo.SelectString(0,"Note Cut");break;
			case 1:m_nna_combo.SelectString(0,"Note Release");break;
			case 2:m_nna_combo.SelectString(0,"None");break;
			}
		}

		void CInstrumentEditor::UpdateNoteLabel()
		{
			const int si = Global::song().waveSelected;
			char buffer[64];
			if (Global::song().samples.Exists(si)) {
				const XMInstrument::WaveData<>& wave = Global::song().samples[si];
				
				const int octave= ((wave.WaveTune()+48)/12);
				switch ((wave.WaveTune()+48)%12)
				{
				case 0:  sprintf(buffer,"C-%i",octave);break;
				case 1:  sprintf(buffer,"C#%i",octave);break;
				case 2:  sprintf(buffer,"D-%i",octave);break;
				case 3:  sprintf(buffer,"D#%i",octave);break;
				case 4:  sprintf(buffer,"E-%i",octave);break;
				case 5:  sprintf(buffer,"F-%i",octave);break;
				case 6:  sprintf(buffer,"F#%i",octave);break;
				case 7:  sprintf(buffer,"G-%i",octave);break;
				case 8:  sprintf(buffer,"G#%i",octave);break;
				case 9:  sprintf(buffer,"A-%i",octave);break;
				case 10:  sprintf(buffer,"A#%i",octave);break;
				case 11:  sprintf(buffer,"B-%i",octave);break;
				}
				m_notelabel.SetWindowText(buffer);
			}
		}

		void CInstrumentEditor::OnCustomdrawSliderm(UINT idx, NMHDR* pNMHDR, LRESULT* pResult) 
		{
/*
			NMCUSTOMDRAW nmcd = *reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
			if (nmcd.dwDrawStage == CDDS_POSTPAINT)
			{
				char buffer[64];
				int label = 0;
				CSliderCtrl* slider = reinterpret_cast<CSliderCtrl*>(GetDlgItem(pNMHDR->idFrom));
				int si=Global::song().waveSelected;
				int nPos = slider->GetPos();
				Instrument* pins = Global::song()._pInstrument[Global::song().waveSelected];
				if (pNMHDR->idFrom == IDC_SLIDER_FCUT) {
					if (pins->ENV_F_TP != dsp::F_NONE &&
						pins->ENV_F_TP != dsp::F_ITLOWPASS &&
						pins->ENV_F_TP != dsp::F_MPTLOWPASSE &&
						pins->ENV_F_TP != dsp::F_MPTHIGHPASSE ) {
						sprintf(buffer,"%.0fHz",dsp::FilterCoeff::Cutoff(pins->ENV_F_TP, nPos));
					}
					else {
						sprintf(buffer,"-");
					}
					label = IDC_CUTOFF_LBL;
				}
				else if (pNMHDR->idFrom == IDC_SLIDER_FRES) {
					Instrument* pins = Global::song()._pInstrument[si];
					if (pins->ENV_F_TP != dsp::F_NONE &&
						pins->ENV_F_TP != dsp::F_ITLOWPASS &&
						pins->ENV_F_TP != dsp::F_MPTLOWPASSE &&
						pins->ENV_F_TP != dsp::F_MPTHIGHPASSE ) {
						sprintf(buffer,"%.02fQ",dsp::FilterCoeff::Resonance(pins->ENV_F_TP, m_cutoff_slider.GetPos(),nPos));
					}
					else {
						sprintf(buffer,"-");
					}
					label = IDC_LABELQ;
				}
				else if (pNMHDR->idFrom == IDC_SLIDERVOL) {
					if (Global::song().samples.Exists(si)) {
						if(m_volumebar.GetPos()==0) {
							sprintf(buffer,"-inf dB");
						} else {
							float invol = helpers::dsp::SliderToAmountHoriz(nPos);
							float db = 20 * log10(invol);
							sprintf(buffer,"%.1f dB",db);
						}
					}
					else {
						buffer[0]='\0';
					}
					label = IDC_VOLABEL;
				}
				else if (pNMHDR->idFrom == IDC_PANSLIDER) {
					if (Global::song().samples.Exists(si)) {
						if ( nPos == 0) sprintf(buffer,"||%02d  ",nPos);
						else if ( nPos < 32) sprintf(buffer,"<<%02d  ",nPos);
						else if ( nPos < 64) sprintf(buffer," <%02d< ",nPos);
						else if ( nPos == 64) sprintf(buffer," |%02d| ",nPos);
						else if ( nPos <= 96) sprintf(buffer," >%02d> ",nPos);
						else if (nPos < 128) sprintf(buffer,"  %02d>>",nPos);
						else sprintf(buffer,"  %02d||",nPos);
					}
					else {
						buffer[0]='\0';
					}
					label = IDC_PANLABEL;
				}
				else if (pNMHDR->idFrom == IDC_SLIDERFINE) {
					if (Global::song().samples.Exists(si)) {
						sprintf(buffer,"%d ct.",m_finetune.GetPos());
					}
					else {
						buffer[0]='\0';
					}
					label = IDC_FINELABEL;
				}
				else if (pNMHDR->idFrom == IDC_SLIDER_AMPATT) {
					m_ampframe.AttackTime(nPos);
					m_ampframe.Invalidate();
					sprintf(buffer,"%.2f ms.",nPos*5.f);
					label = IDC_A_A_LABEL;
				}
				else if (pNMHDR->idFrom == IDC_SLIDER_AMPDEC) {
					m_ampframe.DecayTime(nPos);
					m_ampframe.Invalidate();
					sprintf(buffer,"%.2f ms.",nPos*5.f);
					label = IDC_A_D_LABEL;
				}
				else if (pNMHDR->idFrom == IDC_SLIDER_AMPSUS) {
					m_ampframe.SustainValue((float)nPos*0.01f);
					m_ampframe.Invalidate();
					sprintf(buffer,"%d%%",nPos);
					label = IDC_A_S_LABEL;
				}
				else if (pNMHDR->idFrom == IDC_SLIDER_AMPREL) {
					m_ampframe.ReleaseTime(nPos);
					m_ampframe.Invalidate();
					sprintf(buffer,"%.2f ms.",nPos*5.f);
					label = IDC_A_R_LABEL;
				}
				else if (pNMHDR->idFrom == IDC_SLIDER_FILATT) {
					m_filframe.AttackTime(nPos);
					m_filframe.Invalidate();
					sprintf(buffer,"%.2f ms.",nPos*5.f);
					label = IDC_F_A_LABEL;
				}
				else if (pNMHDR->idFrom == IDC_SLIDER_FILDEC) {
					m_filframe.DecayTime(nPos);
					m_filframe.Invalidate();
					sprintf(buffer,"%.2f ms.",nPos*5.f);
					label = IDC_F_D_LABEL;
				}
				else if (pNMHDR->idFrom == IDC_SLIDER_FILSUS) {
					m_filframe.SustainValue(value_mapper::map_128_1(nPos));
					m_filframe.Invalidate();
					sprintf(buffer,"%d%%",nPos*0.78125f);
					label = IDC_F_S_LABEL;
				}
				else if (pNMHDR->idFrom == IDC_SLIDER_FILREL) {
					m_filframe.ReleaseTime(nPos);
					m_filframe.Invalidate();
					sprintf(buffer,"%.2f ms.",nPos*5.f);
					label = IDC_F_R_LABEL;
				}
				else if (pNMHDR->idFrom == IDC_SLIDER_FILAMT) {
					sprintf(buffer,"%.0f%%",nPos*0.78125f);
					label = IDC_F_AMOUNTLABEL;
				}
				if (label != 0) {
					((CStatic*)GetDlgItem(label))->SetWindowText(buffer);
				}
				*pResult = CDRF_DODEFAULT;
			}
			else if (nmcd.dwDrawStage == CDDS_PREPAINT ){
				*pResult = CDRF_NOTIFYITEMDRAW|CDRF_NOTIFYPOSTPAINT;
			}
			else {
				*pResult = CDRF_DODEFAULT;
			}
*/
		}

		void CInstrumentEditor::SelectByData(CComboBox& combo,DWORD_PTR data) 
		{
			for (int i=0; i < combo.GetCount(); i++) {
				if (combo.GetItemData(i) == data) {
					combo.SetCurSel(i);
					break;
				}
			}
		}

	}   // namespace
}   // namespace
