#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include <psycle/host/XMInstrument.hpp>
#include <psycle/host/LoaderHelper.hpp>

namespace psycle { namespace host {

	class CMainFrame;
	class CChildView;
	class Song;

	class MachineBar : public CDialogBar
	{
		friend class CMainFrame;
	DECLARE_DYNAMIC(MachineBar)

		static int wavFilterSelected;
		static int insFilterSelected;
	public:
		MachineBar(void);
		virtual ~MachineBar(void);

		void InitializeValues(CMainFrame* frame, CChildView* view, Song& song);
		void ChangeAux(int i);
		void ChangeGen(int i);
		void ChangeWave(int i);
		void ChangeIns(int i);
		void UpdateComboIns(bool updatelist=true);
		void UpdateComboGen(bool updatelist=true);
		void EditQuantizeChange(int diff);
	protected:
		virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	protected:
		DECLARE_MESSAGE_MAP()
		afx_msg LRESULT OnInitDialog ( WPARAM , LPARAM );
		afx_msg void OnSelchangeCombostep();
		afx_msg void OnCloseupCombostep();
		afx_msg void OnSelchangeBarCombogen();
		afx_msg void OnCloseupBarCombogen();
		afx_msg void OnCloseupAuxselect();
		afx_msg void OnSelchangeAuxselect();
		afx_msg void OnSelchangeBarComboins();
		afx_msg void OnCloseupBarComboins();
		afx_msg void OnBDecgen();
		afx_msg void OnBIncgen();
		afx_msg void OnGearRack();
		afx_msg void OnBDecAux();
		afx_msg void OnBIncAux();
		afx_msg void OnLoadwave();
		afx_msg void OnSavewave();
		afx_msg void OnEditwave();
		afx_msg void OnWavebut();

	protected:
		int GetNumFromCombo(CComboBox* cb);
		int GetNumFromComboInv(CComboBox *cb);
		bool LoadWave(int waveIdx);
		bool LoadInstrument(int instIdx, bool indexIsSample);
		void SaveWave(int waveIdx);
		void SaveInstrument(int instIdx);
		int SetupDefaultInstrument(LoaderHelper& loadhelp, const XMInstrument::WaveData<>& wave, int waveIdx);
	public:
		CComboBox       m_stepcombo;
		CComboBox       m_gencombo;
		CComboBox       m_auxcombo;
		CComboBox       m_inscombo;
	protected:

		CMainFrame* m_pParentMain;
		CChildView*  m_pWndView;
		Song* m_pSong;

		bool macComboInitialized;
	};
}}
