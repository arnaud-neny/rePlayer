///\file
///\brief implementation file for psycle::host::CInfoDlg.
#include <psycle/host/detail/project.private.hpp>
#include "InfoDlg.hpp"

#include "Configuration.hpp"

#include "Machine.hpp"
#include "Player.hpp"
#include "Song.hpp"

#include "cpu_time_clock.hpp"

namespace psycle { namespace host {
		int const ID_TIMER_INFODLG = 1;

		CInfoDlg::CInfoDlg(CWnd* pParent)
		: CDialog(CInfoDlg::IDD, pParent)
		{
		}

		void CInfoDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_MEM_RESO4, m_mem_virtual);
			DDX_Control(pDX, IDC_MEM_RESO3, m_mem_pagefile);
			DDX_Control(pDX, IDC_MEM_RESO2, m_mem_phy);
			DDX_Control(pDX, IDC_MEM_RESO, m_mem_reso);
			DDX_Control(pDX, IDC_MCPULABEL2, m_cpurout);
			DDX_Control(pDX, IDC_MCPULABEL, m_machscpu);
			DDX_Control(pDX, IDC_CPUL, m_processor_label);
			DDX_Control(pDX, IDC_CPUIDLE_LABEL, m_cpuidlelabel);
			DDX_Control(pDX, IDC_MACHINELIST, m_machlist);
			DDX_Control(pDX, IDC_CPU_PERF, m_cpu_perf);
		}

/*
		BEGIN_MESSAGE_MAP(CInfoDlg, CDialog)
		ON_WM_TIMER()
		ON_BN_CLICKED(IDC_CPU_PERF, OnCpuPerf)
		ON_WM_SHOWWINDOW()
		ON_WM_CLOSE()
		END_MESSAGE_MAP()
*/

		BOOL CInfoDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			m_machlist.InsertColumn(0,"Name",LVCFMT_LEFT,120,0);
			m_machlist.InsertColumn(1,"Machine",LVCFMT_LEFT,96,1);
			m_machlist.InsertColumn(2,"Type",LVCFMT_LEFT,63,1);
			m_machlist.InsertColumn(3,"Ins",LVCFMT_RIGHT,38,1);
			m_machlist.InsertColumn(4,"Outs",LVCFMT_RIGHT,40,1);
			m_machlist.InsertColumn(5,"%/Thread",LVCFMT_RIGHT,62,1);
			m_cpu_perf.SetCheck(Global::player().measure_cpu_usage_? 1:0);
			UpdateInfo();
			InitTimer();
			return TRUE;
		}
		void CInfoDlg::OnShowWindow(BOOL bShow, UINT nStatus) 
		{
			CDialog::OnShowWindow(bShow, nStatus);
			if(bShow) {
				InitTimer();
			}
			else 
			{
				KillTimer(ID_TIMER_INFODLG);
			}
		}
		void CInfoDlg::OnCancel() 
		{
			KillTimer(ID_TIMER_INFODLG);
			CDialog::OnCancel();
		}
		void CInfoDlg::OnClose() 
		{
			KillTimer(ID_TIMER_INFODLG);
			CDialog::OnClose();
		}
		void CInfoDlg::InitTimer()
		{
			if(!SetTimer(ID_TIMER_INFODLG,1000,NULL))
				MessageBox("Error! Couldn't initialize timer","CPU Perfomance Dialog", MB_OK | MB_ICONERROR);
		}

		void CInfoDlg::OnTimer(UINT_PTR nIDEvent) 
		{
			Song& song = Global::song();
			if(nIDEvent==ID_TIMER_INFODLG)
			{
				CSingleLock lock(&Global::song().semaphore, FALSE);
				if (!lock.Lock(50)) return;

				size_t num_threads_running = Global::player().num_threads();
				char buffer[128];
				
				cpu_time_clock::duration total_machine_processing_time(0);
				steady_clock::time_point const now = steady_clock::now();
				float const real_time_duration = ::std::chrono::nanoseconds(now - last_update_time_).count();
				float const multicore_real_time_duration = real_time_duration * num_threads_running;
				bool cpu_usage = Global::player().measure_cpu_usage_;
				int n=0;
				for (int c=0; c<MAX_MACHINES; c++)
				{
					Machine *tmac = song._pMachine[c];
					if(tmac)
					{
						// Input numbers
						sprintf(buffer,"%d",tmac->connectedInputs());
						m_machlist.SetItem(n,3,LVIF_TEXT,buffer,0,0,0,NULL);

						// OutPut numbers
						sprintf(buffer,"%d",tmac->connectedOutputs());
						m_machlist.SetItem(n,4,LVIF_TEXT,buffer,0,0,0,NULL);

						{ // processing cpu percent
							if(cpu_usage) {
								//Showing the cpu usage of the thread, so then it is possible to know how much of a thread, a machine uses.
								float const percent = 100.0f * ::std::chrono::nanoseconds(tmac->accumulated_processing_time()).count() / real_time_duration;
								sprintf(buffer,"%.1f%%",percent);
							}
							else {
								sprintf(buffer,"N/A");
							}
							m_machlist.SetItem(n,5,LVIF_TEXT,buffer,0,0,0,NULL);
						}
						total_machine_processing_time += tmac->accumulated_processing_time();
						tmac->reset_time_measurement();
						n++;
					}
				}

				last_update_time_ = now;

				if(item_count_ != n) UpdateInfo();

				{ // total cpu percent (counts everything, not just machine processing + routing)
					//Accumulated processing time does not divide by "num_threads_running" since it is acummulated in the Player thread (single threaded)
					float const percent = 100.0f * ::std::chrono::nanoseconds(song.accumulated_processing_time()).count() / real_time_duration;
					sprintf(buffer, "%.1f%%", percent);
					m_cpuidlelabel.SetWindowText(buffer);
				}
				
				{ // total machine processing cpu percent
					if(cpu_usage) {
						float const percent = 100.0f * total_machine_processing_time.count() / multicore_real_time_duration;
						sprintf(buffer, "%.1f%%", percent);
					}
					else {
						sprintf(buffer, "N/A");
					}
					m_machscpu.SetWindowText(buffer);
				}

				{ // routing cpu percent
					if(cpu_usage) {
						float const percent = 100.0f * chrono::nanoseconds(song.accumulated_routing_time()).count() / multicore_real_time_duration;
						sprintf(buffer, "%.1f%%", percent);
					}
					else {
						sprintf(buffer, "N/A");
					}

					m_cpurout.SetWindowText(buffer);
				}


				{ // threads
					 sprintf(buffer, "%d", num_threads_running);
					m_processor_label.SetWindowText(buffer);
				}

				song.reset_time_measurement();	
				// Memory status -------------------------------------------------
				
				MEMORYSTATUSEX lpBuffer;
				lpBuffer.dwLength = sizeof(MEMORYSTATUSEX);
				GlobalMemoryStatusEx(&lpBuffer);
				
				sprintf(buffer,"%d%%",100-lpBuffer.dwMemoryLoad);
				m_mem_reso.SetWindowText(buffer);
				
				sprintf(buffer, "%.0fM (of %.0fM)", lpBuffer.ullAvailPhys/(float)(1<<20), lpBuffer.ullTotalPhys/(float)(1<<20));
				m_mem_phy.SetWindowText(buffer);
				
				sprintf(buffer,"%.0fM (of %.0fM)", (lpBuffer.ullAvailPageFile/(float)(1<<20)) , (lpBuffer.ullTotalPageFile/(float)(1<<20)));
				m_mem_pagefile.SetWindowText(buffer);
		#if defined _WIN64
				sprintf(buffer,"%.0fG (of %.0fG)",(lpBuffer.ullAvailVirtual/(float)(1<<30)), (lpBuffer.ullTotalVirtual/(float)(1<<30)));
				m_mem_virtual.SetWindowText(buffer);
		#elif defined _WIN32
				sprintf(buffer,"%.0fM (of %.0fM)",(lpBuffer.ullAvailVirtual/(float)(1<<20)), (lpBuffer.ullTotalVirtual/(float)(1<<20)));
				m_mem_virtual.SetWindowText(buffer);
		#endif
			}
			CDialog::OnTimer(nIDEvent);
		}

		void CInfoDlg::UpdateInfo()
		{
			m_machlist.DeleteAllItems();
			
			int n=0;
			Song& song = Global::song();
			for(int c=0; c<MAX_MACHINES; c++)
			{
				Machine *tmac = song._pMachine[c];
				if(tmac)
				{
					char buffer[128];
					
					// Name [Machine view editor custom name]
					sprintf(buffer,"%.3d: %s",n+1,tmac->_editName);
					m_machlist.InsertItem(n,buffer);
					
					// Gear [Gear type]
					strcpy(buffer, tmac->GetName());
					m_machlist.SetItem(n,1,LVIF_TEXT,buffer,0,0,0,NULL);
					
					// Type [Set is generator/effect/master]
					switch(tmac->_mode)
					{
					case MACHMODE_GENERATOR: strcpy(buffer,"Generator");break;
					case MACHMODE_FX: strcpy(buffer,"Effect");break;
					case MACHMODE_MASTER: strcpy(buffer,"Master");break;
					}
					m_machlist.SetItem(n,2,LVIF_TEXT,buffer,0,0,0,NULL);
					
					// Input numbers
					sprintf(buffer,"%d",tmac->connectedInputs());
					m_machlist.SetItem(n,3,LVIF_TEXT,buffer,0,0,0,NULL);
					
					// OutPut numbers
					sprintf(buffer,"%d",tmac->connectedOutputs());
					m_machlist.SetItem(n,4,LVIF_TEXT,buffer,0,0,0,NULL);
					n++;
				}
			}
			item_count_ = n;
		}
		void CInfoDlg::OnCpuPerf() {
			Global::player().measure_cpu_usage_ = m_cpu_perf.GetCheck() != 0;
		}
}}

