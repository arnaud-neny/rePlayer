// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net
#include <psycle/host/detail/project.private.hpp>
#include "SaveWavDlg.hpp"

#include "ChildView.hpp"
#include "Configuration.hpp"
#include "MainFrm.hpp"
#include "MidiInput.hpp"

#include "Machine.hpp"
#include "Player.hpp"
#include "Song.hpp"

#include <psycle/helpers/pathname_validate.hpp>

#include <iomanip>
#include <iostream>
#include <boost/filesystem/operations.hpp>

namespace psycle { namespace host {

		extern CPsycleApp theApp;

		DWORD WINAPI __stdcall RecordThread(void *b);
		channel_mode CSaveWavDlg::channelmode = no_mode;
		int CSaveWavDlg::rate = -1;
		int CSaveWavDlg::bits = -1;
		int CSaveWavDlg::noiseshape = 0;
		int CSaveWavDlg::ditherpdf = (int)pdf::triangular;
		BOOL CSaveWavDlg::savewires = false;
		BOOL CSaveWavDlg::savetracks = false;
		BOOL CSaveWavDlg::savegens = false;

		CSaveWavDlg::CSaveWavDlg(CChildView* pChildView, CSelection* pBlockSel, CWnd* pParent /* = 0 */) : CDialog(CSaveWavDlg::IDD, pParent)
		{
			m_recmode = 0;		
			m_outputtype = 0;
			this->pChildView = pChildView;
			this->pBlockSel = pBlockSel;
		}

		void CSaveWavDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			//{{AFX_DATA_MAP(CSaveWavDlg)
			DDX_Control(pDX, IDC_FILEBROWSE, m_browse);
			DDX_Control(pDX, IDCANCEL, m_cancel);
			DDX_Control(pDX, IDC_SAVEWAVE, m_savewave);
			DDX_Control(pDX, IDC_SAVEWIRESSEPARATED, m_savewires);
			DDX_Control(pDX, IDC_SAVETRACKSSEPARATED, m_savetracks);
			DDX_Control(pDX, IDC_SAVEGENERATORSEPARATED, m_savegens);
			DDX_Control(pDX, IDC_RANGESTART, m_rangestart);
			DDX_Control(pDX, IDC_RANGEEND, m_rangeend);
			DDX_Control(pDX, IDC_LINESTART, m_linestart);
			DDX_Control(pDX, IDC_LINEEND, m_lineend);
			DDX_Control(pDX, IDC_PROGRESS, m_progress);
			DDX_Control(pDX, IDC_PATNUMBER, m_patnumber);
			DDX_Control(pDX, IDC_PATNUMBER2, m_patnumber2);
			DDX_Control(pDX, IDC_FILENAME, m_filename);
			DDX_Control(pDX, IDC_COMBO_RATE, m_rate);
			DDX_Control(pDX, IDC_COMBO_BITS, m_bits);
			DDX_Control(pDX, IDC_COMBO_CHANNELS, m_channelmode);
			DDX_Control(pDX, IDC_TEXT, m_text);
			DDX_Control(pDX, IDC_CHECK_DITHER, m_dither);
			DDX_Control(pDX, IDC_COMBO_PDF, m_pdf);
			DDX_Control(pDX, IDC_COMBO_NOISESHAPING, m_noiseshaping);
			DDX_Radio(pDX, IDC_RECSONG, m_recmode);
			DDX_Radio(pDX, IDC_OUTPUTFILE, m_outputtype);
			//}}AFX_DATA_MAP
		}

/*
		BEGIN_MESSAGE_MAP(CSaveWavDlg, CDialog)
			//{{AFX_MSG_MAP(CSaveWavDlg)
			ON_BN_CLICKED(IDC_FILEBROWSE, OnFilebrowse)
			ON_BN_CLICKED(IDC_RECSONG, OnSelAllSong)
			ON_BN_CLICKED(IDC_RECRANGE, OnSelRange)
			ON_BN_CLICKED(IDC_RECPATTERN, OnSelPattern)
			ON_BN_CLICKED(IDC_RECBLOCK, OnRecblock)
			ON_BN_CLICKED(IDC_SAVEWAVE, OnSavewave)
			ON_CBN_SELCHANGE(IDC_COMBO_BITS, OnSelchangeComboBits)
			ON_CBN_SELCHANGE(IDC_COMBO_CHANNELS, OnSelchangeComboChannels)
			ON_CBN_SELCHANGE(IDC_COMBO_RATE, OnSelchangeComboRate)
			ON_CBN_SELCHANGE(IDC_COMBO_PDF, OnSelchangeComboPdf)
			ON_CBN_SELCHANGE(IDC_COMBO_NOISESHAPING, OnSelchangeComboNoiseShaping)
			ON_BN_CLICKED(IDC_SAVETRACKSSEPARATED, OnSavetracksseparated)
			ON_BN_CLICKED(IDC_SAVEWIRESSEPARATED, OnSavewiresseparated)
			ON_BN_CLICKED(IDC_SAVEGENERATORSEPARATED, OnSavegensseparated)
			ON_BN_CLICKED(IDC_CHECK_DITHER,	OnToggleDither)
			ON_BN_CLICKED(IDC_OUTPUTFILE, OnOutputfile)
			ON_BN_CLICKED(IDC_OUTPUTCLIPBOARD, OnOutputclipboard)
			ON_BN_CLICKED(IDC_OUTPUTSAMPLE, OnOutputsample)
			ON_MESSAGE_VOID(SAVEWAV_RECORDEND, SaveEnd)

			//}}AFX_MSG_MAP			
			
		END_MESSAGE_MAP()
*/

		BOOL CSaveWavDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();

			threadopen = 0;
			Song& thesong = Global::song();
			thread_handle=INVALID_HANDLE_VALUE;
			kill_thread=1;
			lastpostick=0;
			lastlinetick=0;
			saving=false;

			if (PsycleGlobal::conf().IsRecInPSYDir()) {
				PsycleGlobal::conf().SetCurrentWaveRecDir(PsycleGlobal::conf().GetCurrentSongDir());
			}

			std::string name = PsycleGlobal::conf().GetCurrentWaveRecDir();
			name+='\\';
			name+=thesong.fileName;
			std::size_t dotpos = thesong.fileName.find_last_of(".");
			if ( dotpos == std::string::npos ) dotpos = thesong.fileName.length();
			name+=thesong.fileName.substr(0,dotpos);
			name+=".wav";
			m_filename.SetWindowText(name.c_str());
			
			m_rangeend.EnableWindow(false);
			m_rangestart.EnableWindow(false);
			m_patnumber.EnableWindow(false);
			
			m_lineend.EnableWindow(false);
			m_linestart.EnableWindow(false);
			m_patnumber2.EnableWindow(false);

			char num[4];
			sprintf(num,"%02X",thesong.playOrder[((CMainFrame *)theApp.m_pMainWnd)->m_wndView.editPosition]);
			m_patnumber.SetWindowText(num);
			sprintf(num,"%02X",0);
			m_rangestart.SetWindowText(num);
			sprintf(num,"%02X",thesong.playLength-1);
			m_rangeend.SetWindowText(num);			

			sprintf(num,"%02X",thesong.playOrder[((CMainFrame *)theApp.m_pMainWnd)->m_wndView.editPosition]);
			m_patnumber2.SetWindowText(num);

			if (pChildView->blockSelected)
			{
				sprintf(num,"%02X",pBlockSel->start.line);
				m_linestart.SetWindowText(num);
				sprintf(num,"%02X",pBlockSel->end.line+1);
				m_lineend.SetWindowText(num);
			}
			else
			{
				sprintf(num,"%02X",0);
				m_linestart.SetWindowText(num);
				sprintf(num,"%02X",1);
				m_lineend.SetWindowText(num);
			}

			m_progress.SetRange(0,1);
			m_progress.SetPos(0);

			if ((rate < 0) || (rate >8))
			{
				int outRate = Global::configuration()._pOutputDriver->GetSamplesPerSec();
				if (outRate <= 8000) { rate = 0; }
				else if (outRate <= 11025) { rate = 1; }
				else if (outRate <= 16000) { rate = 2; }
				else if (outRate <= 22050) { rate = 3; }
				else if (outRate <= 32000) { rate = 4; }
				else if (outRate <= 44100) { rate = 5; }
				else if (outRate <= 48000) { rate = 6; }
				else if (outRate <= 88200) { rate = 7; }
				else {
					rate = 8;
				}
			}

			m_rate.AddString("8000 hz");
			m_rate.AddString("11025 hz");
			m_rate.AddString("16000 hz");
			m_rate.AddString("22050 hz");
			m_rate.AddString("32000 hz");
			m_rate.AddString("44100 hz");
			m_rate.AddString("48000 hz");
			m_rate.AddString("88200 hz");
			m_rate.AddString("96000 hz");
			m_rate.SetCurSel(rate);

			if ((bits < 0) || (bits > 4))
			{
				int outBits = Global::configuration()._pOutputDriver->GetSampleValidBits();
				if ( outBits<= 8 ) { bits = 0; }
				else if (outBits <= 16) { bits = 1; }
				else if (outBits <= 24) { bits = 2;	}
				else //if (outBits <= 32)
				{
					bits = 4;
				}
			}

			m_bits.AddString("8 bit");
			m_bits.AddString("16 bit");
			m_bits.AddString("24 bit");
			m_bits.AddString("32 bit (int)");
			m_bits.AddString("32 bit (float)");

			m_bits.SetCurSel(bits);

			m_channelmode.AddString("Mono (Mix)");
			m_channelmode.AddString("Mono (Left)");
			m_channelmode.AddString("Mono (Right)");
			m_channelmode.AddString("Stereo");

			if ((channelmode < 0) || (channelmode > 3))
			{
				channelmode = Global::configuration()._pOutputDriver->GetChannelMode();
			}
			m_channelmode.SetCurSel(channelmode);

			if(bits < 2) m_dither.SetCheck(BST_CHECKED);

			m_pdf.AddString("Triangular");
			m_pdf.AddString("Rectangular");
			m_pdf.AddString("Gaussian");
			ditherpdf = (int)pdf::triangular;
			m_pdf.SetCurSel(ditherpdf);

			m_noiseshaping.AddString("None");
			m_noiseshaping.AddString("High-Pass Contour");
			noiseshape=0;
			m_noiseshaping.SetCurSel(noiseshape);

			if (bits == 3 )
			{
				m_dither.EnableWindow(false);
				m_pdf.EnableWindow(false);
				m_noiseshaping.EnableWindow(false);
			}

			m_savetracks.SetCheck(savetracks);
			m_savegens.SetCheck(savegens);
			m_savewires.SetCheck(savewires);

			m_text.SetWindowText("");		

			return true;  // return true unless you set the focus to a control
			// EXCEPTION: OCX Property Pages should return false
		}

		void CSaveWavDlg::OnOutputfile()
		{
			m_savewave.EnableWindow(true);
			m_outputtype=0;
			m_savewires.EnableWindow(true);
			m_savetracks.EnableWindow(true);
			m_savegens.EnableWindow(true);
			m_filename.EnableWindow(true);
			m_browse.EnableWindow(true);
			m_bits.EnableWindow(true);
		}

		void CSaveWavDlg::OnOutputclipboard()
		{
			m_savewave.EnableWindow(true);
			m_outputtype=1;
			m_savewires.EnableWindow(false);
			m_savetracks.EnableWindow(false);
			m_savegens.EnableWindow(false);
			m_filename.EnableWindow(false);
			m_browse.EnableWindow(false);
			m_bits.EnableWindow(true);
		}
		
		void CSaveWavDlg::OnOutputsample()
		{
			m_savewave.EnableWindow(true);
			m_outputtype=2;
			m_savewires.EnableWindow(false);
			m_savetracks.EnableWindow(false);
			m_savegens.EnableWindow(false);
			m_filename.EnableWindow(false);
			m_browse.EnableWindow(false);
			m_bits.EnableWindow(false);
			m_bits.SetCurSel(1);
			OnSelchangeComboBits();
		}

		void CSaveWavDlg::OnFilebrowse() 
		{
			static const char szFilter[] = "Wav Files (*.wav)|*.wav|All Files (*.*)|*.*||";
			CString direct;
			m_filename.GetWindowText(direct);
			std::string nametemp = direct;
			pathname_validate::validate(nametemp);

			CFileDialog dlg(false,"wav",nametemp.c_str(),OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN | OFN_DONTADDTORECENT,szFilter);
			if ( dlg.DoModal() == IDOK ) 
			{
				CString str = dlg.GetPathName();
				CString str2 = str.Right(4);
				if ( str2.CompareNoCase(".wav") != 0 ) 
				{
					str.Insert(str.GetLength(),".wav");
				}
				m_filename.SetWindowText(str);
				int index = str.ReverseFind('\\');
				if (index != -1)
				{
					str.Truncate(index);
					PsycleGlobal::conf().SetCurrentWaveRecDir(str.GetString());
				}

			}
		}

		void CSaveWavDlg::OnSelAllSong() 
		{
			m_rangeend.EnableWindow(false);
			m_rangestart.EnableWindow(false);
			m_patnumber.EnableWindow(false);
			m_lineend.EnableWindow(false);
			m_linestart.EnableWindow(false);
			m_patnumber2.EnableWindow(false);
			m_recmode=0;
		}

		void CSaveWavDlg::OnSelPattern() 
		{
			m_rangeend.EnableWindow(false);
			m_rangestart.EnableWindow(false);
			m_patnumber.EnableWindow(true);
			m_lineend.EnableWindow(false);
			m_linestart.EnableWindow(false);
			m_patnumber2.EnableWindow(false);
			m_recmode=1;
		}

		void CSaveWavDlg::OnSelRange() 
		{
			m_rangeend.EnableWindow(true);
			m_rangestart.EnableWindow(true);
			m_patnumber.EnableWindow(false);
			m_lineend.EnableWindow(false);
			m_linestart.EnableWindow(false);
			m_patnumber2.EnableWindow(false);
			m_recmode=2;
		}

		void CSaveWavDlg::OnRecblock()
		{
			m_rangeend.EnableWindow(false);
			m_rangestart.EnableWindow(false);
			m_patnumber.EnableWindow(false);
			m_lineend.EnableWindow(true);
			m_linestart.EnableWindow(true);
			m_patnumber2.EnableWindow(true);
			m_recmode=3;
		}

		void CSaveWavDlg::OnSavewave() 
		{
			const int real_rate[]={8000,11025,16000,22050,32000,44100,48000,88200,96000};
			const int real_bits[]={8,16,24,32,32};
			bool isFloat = (bits == 4);
			Song& thesong = Global::song();
			Player& theplayer = Global::player();

			CString name;
			m_filename.GetWindowText(name);
			rootname=name;
			std::size_t dotpos = rootname.find_last_of(".");
			if ( dotpos == std::string::npos ) dotpos = rootname.length();
			rootname=rootname.substr(0,dotpos);

			boost::filesystem::path mypath(name.GetString());

			if (!boost::filesystem::exists(mypath.parent_path())) {
				MessageBox("The folder where to store the file does not exists. Please, create it first","Save Wav dialog");
				return;
			}

			GetDlgItem(IDC_RECSONG)->EnableWindow(false);
			GetDlgItem(IDC_RECPATTERN)->EnableWindow(false);
			GetDlgItem(IDC_RECRANGE)->EnableWindow(false);
			GetDlgItem(IDC_RECBLOCK)->EnableWindow(false);
			GetDlgItem(IDC_FILEBROWSE)->EnableWindow(false);

			m_filename.EnableWindow(false);
			m_savetracks.EnableWindow(false);
			m_savegens.EnableWindow(false);
			m_savewires.EnableWindow(false);
			m_rate.EnableWindow(false);
			m_bits.EnableWindow(false);
			m_channelmode.EnableWindow(false);
			m_pdf.EnableWindow(false);
			m_noiseshaping.EnableWindow(false);
			m_dither.EnableWindow(false);

			m_rangeend.EnableWindow(false);
			m_rangestart.EnableWindow(false);
			m_patnumber.EnableWindow(false);

			m_savewave.EnableWindow(false);
			m_cancel.SetWindowText("Stop");


			//If autoStopMachines enabled, disable while recording.
			autostop = Global::configuration().UsesAutoStopMachines();
			if ( autostop )
			{
				Global::configuration().UseAutoStopMachines(false);
				for (int c=0; c<MAX_MACHINES; c++)
				{
					if (thesong._pMachine[c])
					{
						thesong._pMachine[c]->Standby(false);
					}
				}
			}
			playblock = theplayer._playBlock;
			loopsong = theplayer._loopSong;
			memcpy(sel,thesong.playOrderSel,MAX_SONG_POSITIONS);
			memset(thesong.playOrderSel,0,MAX_SONG_POSITIONS);


			if (m_outputtype == 0)
			{	//record to file
				if (m_savetracks.GetCheck())
				{
					memcpy(_Muted,thesong._trackMuted,sizeof(thesong._trackMuted));

					int count = 0;

					for (int i = 0; i < thesong.SONGTRACKS; i++)
					{
						if (!_Muted[i])
						{
							count++;
							current = i;
							for (int j = 0; j < thesong.SONGTRACKS; j++)
							{
								if (j != i)
								{
									thesong._trackMuted[j] = true;
								}
								else
								{
									thesong._trackMuted[j] = false;
								}
							}
							// now save the song
							std::ostringstream filename;
							filename << rootname;
							filename << "-track "
								<< std::setprecision(2) << (unsigned)i;
							SaveWav(filename.str().c_str(),real_bits[bits],real_rate[rate],channelmode,isFloat);
							return;
						}
					}
					current = 256;
					SaveEnd();
				}
				else if (m_savewires.GetCheck())
				{
					Master& master = *((Master*)thesong._pMachine[MASTER_INDEX]);
					// this is tricky - sort of
					// back up our connections first
					for (int i = 0; i < MAX_CONNECTIONS; i++)
					{
						if (master.inWires[i].Enabled())
						{
							_Muted[i] = master.inWires[i].GetSrcMachine()._mute;
						}
						else
						{
							_Muted[i] = true;
						}
					}

					for (int i = 0; i < MAX_CONNECTIONS; i++)
					{
						if (!_Muted[i])
						{
							current = i;
							for (int j = 0; j < MAX_CONNECTIONS; j++)
							{
								if (master.inWires[j].Enabled())
								{
									if (j != i)
									{
										master.inWires[j].GetSrcMachine()._mute = true;
									}
									else
									{
										master.inWires[j].GetSrcMachine()._mute = false;
									}
								}
							}
							// now save the song
							char filename[MAX_PATH];
							sprintf(filename,"%s-wire %.2u %s.wav",rootname.c_str(),i,master.inWires[i].GetSrcMachine()._editName);
							SaveWav(filename,real_bits[bits],real_rate[rate],channelmode,isFloat);
							return;
						}
					}
					current = 256;
					SaveEnd();
				}
				else if (m_savegens.GetCheck())
				{
					// this is tricky - sort of
					// back up our connections first

					for (int i = 0; i < MAX_BUSES; i++)
					{
						if (thesong._pMachine[i])
						{
							_Muted[i] = thesong._pMachine[i]->_mute;
						}
						else
						{
							_Muted[i] = true;
						}
					}

					for (int i = 0; i < MAX_BUSES; i++)
					{
						if (!_Muted[i])
						{
							current = i;
							for (int j = 0; j < MAX_BUSES; j++)
							{
								if (thesong._pMachine[j])
								{
									if (j != i)
									{
										thesong._pMachine[j]->_mute = true;
									}
									else
									{
										thesong._pMachine[j]->_mute = false;
									}
								}
							}
							// now save the song
							char filename[MAX_PATH];
							sprintf(filename,"%s-generator %.2u %s.wav",rootname.c_str(),i,thesong._pMachine[i]->_editName);
							SaveWav(filename,real_bits[bits],real_rate[rate],channelmode,isFloat);
							return;
						}
					}
					current = 256;
					SaveEnd();
				}
				else
				{
					SaveWav(mypath.string(),real_bits[bits],real_rate[rate],channelmode,isFloat);
				}
			}
			else if (m_outputtype == 1 || m_outputtype == 2)
			{
				// Clear clipboardmem if needed (not needed. it's a safety measure)
				if ( clipboardmem.size() > 0)
				{
					for (unsigned int i=0;i<clipboardmem.size();i++)
					{
						delete[] clipboardmem[i];
					}
					clipboardmem.clear();
				}
				//allocate first vector value to store the size of the clipboard memory.
				int *size = new int[1];
				memset(size,0,sizeof(int));
				clipboardmem.push_back(reinterpret_cast<char*>(size));
				// No name -> record to clipboard.
				SaveWav("",real_bits[bits],real_rate[rate],channelmode,isFloat);
			}
		}

		void CSaveWavDlg::SaveWav(std::string file, int bits, int rate, channel_mode channelmode,bool isFloat)
		{
			saving=true;
			Player& theplayer = Global::player();
			Song& thesong = Global::song();
			theplayer.StopRecording();
			Global::configuration()._pOutputDriver->Enable(false);
			PsycleGlobal::midi().Close();

			std::string::size_type pos = file.rfind('\\');
			if (pos == std::string::npos)
			{
				m_text.SetWindowText(file.c_str());
			}
			else
			{
				m_text.SetWindowText(file.substr(pos+1).c_str());
			}

			theplayer.StartRecording(file,bits,rate,channelmode,isFloat,
									m_dither.GetCheck()==BST_CHECKED && bits!=32, ditherpdf, noiseshape,&clipboardmem);

			if (!theplayer._recording) {
				//if startrecording fails, it calls stoprecording with an error that shows a message. Probably the correct place would
				//be here, so that player doesn't have ui specific code.
				return;
			}

			int tmp;
			int cont;
			CString name;

			int pstart;
			kill_thread = 0;
			tickcont=0;
			lastlinetick=0;
			int i,j;
			
			int blockSLine;
			int blockELine;
			std::stringstream ss;

			switch (m_recmode)
			{
				using helpers::hexstring_to_integer;
			case 0:
				j=0; // Calculate progress bar range.
				for (i=0;i<thesong.playLength;i++)
				{
					j+=thesong.patternLines[thesong.playOrder[i]];
				}
				m_progress.SetRange(0,j);
				
				lastpostick=0;
				theplayer.Start(0,0);
				break;
			case 1:
				m_patnumber.GetWindowText(name);
				ss << name;
				hexstring_to_integer(ss.str(), pstart);
				m_progress.SetRange(0,thesong.patternLines[pstart]);
				for (cont=0;cont<thesong.playLength;cont++)
				{
					if ( (int)thesong.playOrder[cont] == pstart)
					{
						pstart= cont;
						break;
					}
				}
				lastpostick=pstart;
				thesong.playOrderSel[cont]=true;
				theplayer.Start(pstart,0);
				theplayer._playBlock=true;
				break;
			case 2:
				m_rangestart.GetWindowText(name);
				ss << name;
				hexstring_to_integer(ss.str() , pstart);
				ss.clear();
				m_rangeend.GetWindowText(name);
				ss << name;
				hexstring_to_integer(ss.str(), tmp);
				j=0;
				for (cont=pstart;cont<=tmp;cont++)
				{
					thesong.playOrderSel[cont]=true;
					j+=thesong.patternLines[thesong.playOrder[cont]];
				}
				m_progress.SetRange(0,j);

				lastpostick=pstart;
				theplayer.Start(pstart,0);
				theplayer._playBlock=true;
				break;
			case 3:
				m_patnumber.GetWindowText(name);
				ss << name;
				hexstring_to_integer(ss.str(), pstart);
				m_linestart.GetWindowText(name);
				ss.clear();
				ss << name;
				hexstring_to_integer(ss.str(), blockSLine);
				m_lineend.GetWindowText(name);
				ss.clear();
				ss << name;
				hexstring_to_integer(ss.str(), blockELine);

				m_progress.SetRange(blockSLine,blockELine);
				//find the position in the sequence where the pstart pattern is located.
				for (cont=0;cont<thesong.playLength;cont++)
				{
					if ( (int)thesong.playOrder[cont] == pstart)
					{
						pstart= cont;
						break;
					}
				}
				lastpostick=pstart;
				thesong.playOrderSel[cont]=true;
				theplayer.Start(pstart,blockSLine, blockELine);
				theplayer._playBlock=true;
				break;
			default:
				SaveEnd();
				return;
			}
			unsigned long tmp2;
			thread_handle = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) RecordThread,(void *) this,0,&tmp2);
		}

		DWORD WINAPI __stdcall RecordThread(void *b)
		{
			CSaveWavDlg& dlg = *reinterpret_cast<CSaveWavDlg*>(b);
			dlg.threadopen++;
			Player& theplayer = Global::player();
			theplayer._loopSong=false;
			int stream_size = 16384; // Player has just a single buffer of 65535 samples to allocate both channels
			while(!dlg.kill_thread && theplayer._playing)  // the player automatically stops at end, if not looping.
			{
				Player::Work(&theplayer,stream_size);
				dlg.SaveTick();
			}
			theplayer.Stop();
			theplayer.StopRecording();
			dlg.threadopen--;
			dlg.PostMessage(SAVEWAV_RECORDEND);
			ExitThread(0);
			//return 0;
		}

		void CSaveWavDlg::OnCancel() 
		{
			if (saving || (threadopen > 0))
			{
				current = 256;
				kill_thread=1;
			}
			else if (threadopen <= 0)
			{
				CDialog::OnCancel();
			}
		}

		void CSaveWavDlg::SaveEnd()
		{
			saving=false;
			kill_thread=1;
			//If autostop was enabled, restore the setting.
			if ( autostop ) 
			{
				Global::configuration().UseAutoStopMachines(true);
			}
			Global::player()._playBlock=playblock;
			Global::player()._loopSong=loopsong;
			memcpy(Global::song().playOrderSel,sel,MAX_SONG_POSITIONS);
			Global::configuration()._pOutputDriver->Enable(true);
			PsycleGlobal::midi().Open();

			if (m_outputtype == 1)
			{
				SaveToClipboard();
			}

			else if ( m_outputtype == 2)
			{
				const int real_rate[]={8000,11025,16000,22050,32000,44100,48000,88200,96000};
				Song& thesong = Global::song();
				int length = *reinterpret_cast<int*>(clipboardmem[0]);
				XMInstrument::WaveData<> wave;
				bool isStereo = (channelmode == stereo || channelmode == no_mode);
				wave.AllocWaveData(isStereo?length/4:length/2, isStereo);
				wave.WaveName("Recorded");
				wave.WaveSampleRate(real_rate[rate]);
				if (isStereo) {
					//If it is stereo, we need to deinterlace it.
					int copiedsize=0;
					int i=1;
					uint16_t samps[32768];
					// 1000000 bytes = 250000 stereo 16bit samples.
					length >>= 2;
					uint32_t amount=0;
					while (copiedsize+250000<=length)
					{
						int b=copiedsize;
						for(uint32_t io = 0 ; io < 250000 ; io+=amount)
						{
							//amount in bytes required to be copied (1 stereo 16bit sample = 4 bytes)
							amount = std::min(32768U/2U,250000-io);
							CopyMemory(samps, clipboardmem[i]+(io*4), amount*4);
							uint16_t* psamps = samps;
							for (int a=0; a < amount; a++,b++) {
								wave.pWaveDataL()[b]=*psamps;
								psamps++;
								wave.pWaveDataR()[b]=*psamps;
								psamps++;
							}
						}
						i++;
						copiedsize+=250000;
					}
					int b=copiedsize;
					uint32_t remaining=static_cast<uint32_t>(length-copiedsize);
					// 1000000 bytes = 250000 stereo 16bit samples.
					for(uint32_t io = 0 ; io < remaining ; io+=amount)
					{
						//amount in samples required to be copied (1 stereo 16bit sample = 4 bytes)
						amount = std::min(32768U/2U,(remaining-io));
						CopyMemory(samps, clipboardmem[i]+(io*4), amount*4);
						uint16_t* psamps = samps;
						for (int a=0; a < amount; a++,b++) {
							wave.pWaveDataL()[b]=*psamps;
							psamps++;
							wave.pWaveDataR()[b]=*psamps;
							psamps++;
						}
					}
				}
				else {
					int copiedsize=0;
					int i=1;
					while (copiedsize+1000000<=length)
					{
						CopyMemory(wave.pWaveDataL()+copiedsize,clipboardmem[i], 1000000);
						i++;
						copiedsize+=1000000;
					}
					CopyMemory(wave.pWaveDataL()+copiedsize,clipboardmem[i], length-copiedsize);
				}
				thesong.samples.AddSample(wave);
				for (unsigned int i=0;i<clipboardmem.size();i++)
				{
					delete[] clipboardmem[i];
				}
				clipboardmem.clear();

			}
			else if (m_savetracks.GetCheck())
			{
				Song& thesong = Global::song();

				const int real_rate[]={8000,11025,16000,22050,32000,44100,48000,88200,96000};
				const int real_bits[]={8,16,24,32,32};
				const bool isFloat = (bits == 4);

				for (int i = current+1; i < thesong.SONGTRACKS; i++)
				{
					if (!_Muted[i])
					{
						current = i;
						for (int j = 0; j < thesong.SONGTRACKS; j++)
						{
							if (j != i)
							{
								thesong._trackMuted[j] = true;
							}
							else
							{
								thesong._trackMuted[j] = false;
							}
						}
						// now save the song
						char filename[MAX_PATH];
						sprintf(filename,"%s-track %.2u.wav",rootname.c_str(),i);
						SaveWav(filename,real_bits[bits],real_rate[rate],channelmode,isFloat);
						return;
					}
				}
				memcpy(thesong._trackMuted,_Muted,sizeof(thesong._trackMuted));
			}

			else if (m_savewires.GetCheck())
			{
				Song& thesong = Global::song();
				Master& master = *((Master*)thesong._pMachine[MASTER_INDEX]);

				const int real_rate[]={8000,11025,16000,22050,32000,44100,48000,88200,96000};
				const int real_bits[]={8,16,24,32,32};
				const bool isFloat = (bits == 4);

				for (int i = current+1; i < MAX_CONNECTIONS; i++)
				{
					if (!_Muted[i])
					{
						current = i;
						for (int j = 0; j < MAX_CONNECTIONS; j++)
						{
							if (master.inWires[j].Enabled())
							{
								if (j != i)
								{
									master.inWires[i].GetSrcMachine()._mute = true;
								}
								else
								{
									master.inWires[i].GetSrcMachine()._mute = false;
								}
							}
						}
						// now save the song
						char filename[MAX_PATH];
						sprintf(filename,"%s-wire %.2u %s.wav",rootname.c_str(),i,master.inWires[i].GetSrcMachine()._editName);
						SaveWav(filename,real_bits[bits],real_rate[rate],channelmode,isFloat);
						return;
					}
				}

				for (int i = 0; i < MAX_CONNECTIONS; i++)
				{
					if (master.inWires[i].Enabled())
					{
						master.inWires[i].GetSrcMachine()._mute = _Muted[i];
					}
				}
			}

			else if (m_savegens.GetCheck())
			{
				Song& thesong = Global::song();

				const int real_rate[]={8000,11025,16000,22050,32000,44100,48000,88200,96000};
				const int real_bits[]={8,16,24,32,32};
				const bool isFloat = (bits == 4);

				for (int i = current+1; i < MAX_BUSES; i++)
				{
					if (!_Muted[i])
					{
						current = i;
						for (int j = 0; j < MAX_BUSES; j++)
						{
							if (thesong._pMachine[j])
							{
								if (j != i)
								{
									thesong._pMachine[j]->_mute = true;
								}
								else
								{
									thesong._pMachine[j]->_mute = false;
								}
							}
						}
						// now save the song
						char filename[MAX_PATH];
						sprintf(filename,"%s-generator %.2u %s.wav",rootname.c_str(),i,thesong._pMachine[i]->_editName);
						SaveWav(filename,real_bits[bits],real_rate[rate],channelmode,isFloat);
						return;
					}
				}

				for (int i = 0; i < MAX_BUSES; i++)
				{
					if (thesong._pMachine[i])
					{
						thesong._pMachine[i]->_mute = _Muted[i];
					}
				}
			}

			m_text.SetWindowText("");

			GetDlgItem(IDC_RECSONG)->EnableWindow(true);
			GetDlgItem(IDC_RECPATTERN)->EnableWindow(true);
			GetDlgItem(IDC_RECRANGE)->EnableWindow(true);
			GetDlgItem(IDC_RECBLOCK)->EnableWindow(true);
			GetDlgItem(IDC_FILEBROWSE)->EnableWindow(true);

			m_filename.EnableWindow(true);
			m_rate.EnableWindow(true);
			m_channelmode.EnableWindow(true);
			if (bits < 3 ) {
				m_pdf.EnableWindow(m_dither.GetCheck());
				m_noiseshaping.EnableWindow(m_dither.GetCheck());
				m_dither.EnableWindow(true);
			}
			m_savetracks.EnableWindow(false);
			m_savegens.EnableWindow(false);
			m_savewires.EnableWindow(false);

			switch (m_recmode)
			{
			case 0:
				m_savetracks.EnableWindow(true);
				m_savegens.EnableWindow(true);
				m_savewires.EnableWindow(true);
				m_rangeend.EnableWindow(false);
				m_rangestart.EnableWindow(false);
				m_patnumber.EnableWindow(false);
				m_bits.EnableWindow(true);
				break;
			case 1:
				m_rangeend.EnableWindow(false);
				m_rangestart.EnableWindow(false);
				m_patnumber.EnableWindow(true);
				m_bits.EnableWindow(true);
				break;
			case 2:
				m_rangeend.EnableWindow(true);
				m_rangestart.EnableWindow(true);
				m_patnumber.EnableWindow(false);
				m_bits.EnableWindow(false);
				break;
			}

			m_progress.SetPos(0);
			m_savewave.EnableWindow(true);
			m_cancel.SetWindowText("Close");
		}

		void CSaveWavDlg::SaveToClipboard()
		{
			OpenClipboard();
			EmptyClipboard();

			const int real_rate[]={8000,11025,16000,22050,32000,44100,48000,88200,96000};
			const int real_bits[]={8,16,24,32,32};
			//not supported for now
			//const bool isFloat = (bits == 4);

			///Note: Audacity does not use windows clipboard. This works for modplug tracker
			///\todo: support waveformatextensible?
			clipboardwavheader.head = FourCC("RIFF");
			clipboardwavheader.head2= FourCC("WAVE");
			clipboardwavheader.fmthead = FourCC("fmt ");
			clipboardwavheader.fmtsize = sizeof(WAVEFORMATEX);
			clipboardwavheader.fmtcontent.wFormatTag = WAVE_FORMAT_PCM;
			clipboardwavheader.fmtcontent.nChannels = (channelmode == stereo || channelmode == no_mode) ? 2 : 1;
			clipboardwavheader.fmtcontent.nSamplesPerSec = real_rate[rate];
			clipboardwavheader.fmtcontent.wBitsPerSample = real_bits[bits];
			clipboardwavheader.fmtcontent.nBlockAlign = clipboardwavheader.fmtcontent.wBitsPerSample/8*clipboardwavheader.fmtcontent.nChannels;
			clipboardwavheader.fmtcontent.nAvgBytesPerSec =clipboardwavheader.fmtcontent.nBlockAlign*clipboardwavheader.fmtcontent.nSamplesPerSec;
			clipboardwavheader.fmtcontent.cbSize = 0;
			clipboardwavheader.datahead = FourCC("data");

			int length = *reinterpret_cast<int*>(clipboardmem[0]);

			clipboardwavheader.datasize = length;
			clipboardwavheader.size = clipboardwavheader.datasize + sizeof(fullheader) - 8;


			HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, clipboardwavheader.datasize + sizeof(fullheader));
			char*	pClipboardData = (char*) GlobalLock(hClipboardData);

			CopyMemory(pClipboardData, &clipboardwavheader, sizeof(fullheader) );

			// In bytes
			int copiedsize=0;
			int i=1;
			pClipboardData += sizeof(fullheader);
			while (copiedsize+1000000<=length)
			{
				CopyMemory(pClipboardData +copiedsize,clipboardmem[i], 1000000);
				i++;
				copiedsize+=1000000;
			}
			CopyMemory(pClipboardData +copiedsize,clipboardmem[i], length-copiedsize);

			for (unsigned int i=0;i<clipboardmem.size();i++)
			{
				delete[] clipboardmem[i];
			}
			clipboardmem.clear();

			GlobalUnlock(hClipboardData);
			SetClipboardData(CF_WAVE, hClipboardData);
			CloseClipboard();

		}

		void CSaveWavDlg::SaveTick()
		{
			Song& thesong = Global::song();
			Player& theplayer = Global::player();
			for (int i=lastpostick+1;i<theplayer._playPosition;i++)
			{
				tickcont+=thesong.patternLines[thesong.playOrder[i]];
			}
			if (lastpostick!= theplayer._playPosition ) 
			{
				tickcont+=thesong.patternLines[thesong.playOrder[lastpostick]]-(lastlinetick+1)+theplayer._lineCounter;
			}
			else tickcont+=theplayer._lineCounter-lastlinetick;

			lastlinetick = theplayer._lineCounter;
			lastpostick = theplayer._playPosition;

			if (!kill_thread ) 
			{
				m_progress.SetPos(tickcont);
			}
		}

		void CSaveWavDlg::OnSelchangeComboBits() 
		{
			bits = m_bits.GetCurSel();
			if (bits >= 3 )
			{
				m_dither.EnableWindow(false);
				m_pdf.EnableWindow(false);
				m_noiseshaping.EnableWindow(false);
			}
			else
			{
				m_dither.EnableWindow(true);
				m_pdf.EnableWindow(true);
				m_noiseshaping.EnableWindow(true);
			}
		}

		void CSaveWavDlg::OnSelchangeComboChannels() 
		{
			channelmode = (channel_mode) m_channelmode.GetCurSel();
		}

		void CSaveWavDlg::OnSelchangeComboRate() 
		{
			rate = m_rate.GetCurSel();
		}

		void CSaveWavDlg::OnSelchangeComboPdf()
		{
			ditherpdf = m_pdf.GetCurSel();
		}
		void CSaveWavDlg::OnSelchangeComboNoiseShaping()
		{
			noiseshape = m_noiseshaping.GetCurSel();
		}
		void CSaveWavDlg::OnToggleDither()
		{
			m_noiseshaping.EnableWindow(m_dither.GetCheck());
			m_pdf.EnableWindow(m_dither.GetCheck());
		}
		void CSaveWavDlg::OnSavetracksseparated() 
		{
			if (savetracks = m_savetracks.GetCheck())
			{
				m_savewires.SetCheck(false);
				savewires = false;
				m_savegens.SetCheck(false);
				savegens = false;
			}
		}

		void CSaveWavDlg::OnSavewiresseparated() 
		{
			if (savewires = m_savewires.GetCheck())
			{
				m_savetracks.SetCheck(false);
				savetracks = false;
				m_savegens.SetCheck(false);
				savegens = false;
			}
		}

		void CSaveWavDlg::OnSavegensseparated() 
		{
			if (savewires = m_savegens.GetCheck())
			{
				m_savetracks.SetCheck(false);
				savetracks = false;
				m_savewires.SetCheck(false);
				savewires = false;
			}
		}

}}
