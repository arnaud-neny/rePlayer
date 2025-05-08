///\file
///\brief implementation file for psycle::host::CGreetDialog.
#include <psycle/host/detail/project.private.hpp>
#include "GreetDialog.hpp"
namespace psycle { namespace host {

		CGreetDialog::CGreetDialog(CWnd* pParent /* = 0 */) : CDialog(CGreetDialog::IDD, pParent)
		{
			//{{AFX_DATA_INIT(CGreetDialog)
				// NOTE: the ClassWizard will add member initialization here
			//}}AFX_DATA_INIT
		}

		void CGreetDialog::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_GREETSLIST, m_greetz);
		}

/*
		BEGIN_MESSAGE_MAP(CGreetDialog, CDialog)
		END_MESSAGE_MAP()
*/

		BOOL CGreetDialog::OnInitDialog() 
		{
			CDialog::OnInitDialog();
		/*
			//Original Arguru's Greetings.
			m_greetz.AddString("Hamarr Heylen 'Hymax' [Logo design]");
			m_greetz.AddString("Raul Reales 'DJLaser'");
			m_greetz.AddString("Fco. Portillo 'Titan3_4'");
			m_greetz.AddString("Juliole");
			m_greetz.AddString("Sergio 'Zuprimo'");
			m_greetz.AddString("Oskari Tammelin [buzz creator]");
			m_greetz.AddString("Amir Geva 'Photon'");
			m_greetz.AddString("WhiteNoize");
			m_greetz.AddString("Zephod");
			m_greetz.AddString("Felix Petrescu 'WakaX'");
			m_greetz.AddString("Spiril at #goa [EFNET]");
			m_greetz.AddString("Joselex 'Americano'");
			m_greetz.AddString("Lach-ST2");
			m_greetz.AddString("DrDestral");
			m_greetz.AddString("Ic3man");
			m_greetz.AddString("Osirix");
			m_greetz.AddString("Mulder3");
			m_greetz.AddString("HexDump");
			m_greetz.AddString("Robotico");
			m_greetz.AddString("Krzysztof Foltman [FSM]");

			m_greetz.AddString("All #track at Irc-Hispano");

		*/
			m_greetz.AddString("All the people in the Forums");
			m_greetz.AddString("All at #psycle [EFnet]");

			m_greetz.AddString("Alk [Extreme testing + Coding]");
//			m_greetz.AddString("BigTick [for his excellent VSTs]");
			m_greetz.AddString("bohan");
			m_greetz.AddString("Byte");
			m_greetz.AddString("CyanPhase [for porting VibraSynth]");
			m_greetz.AddString("dazld");
			m_greetz.AddString("dj_d [Beta testing]");
			m_greetz.AddString("DJMirage");
//			m_greetz.AddString("Drax_D [for asking to be here ;D]");
			m_greetz.AddString("Druttis [Machines]");
			m_greetz.AddString("Erodix");
//			m_greetz.AddString("Felix Kaplan / Spirit Of India");
//			m_greetz.AddString("Felix Petrescu 'WakaX'");
//			m_greetz.AddString("Gerwin / FreeH2o");
//			m_greetz.AddString("Imagineer");
			m_greetz.AddString("Arguru/Guru R.I.P. [We follow your steps]");
//			m_greetz.AddString("KooPer");
//			m_greetz.AddString("Krzysztof Foltman / fsm [Coding help]");
//			m_greetz.AddString("krokpitr");
			m_greetz.AddString("ksn [Psycledelics WebMaster]");
			m_greetz.AddString("lastfuture");
			m_greetz.AddString("LegoStar [asio]");
//			m_greetz.AddString("Loby [for being away]");
			m_greetz.AddString("Pikari");
			m_greetz.AddString("pooplog [Machines + Coding]");
			m_greetz.AddString("sampler");
			m_greetz.AddString("[SAS] SOLARiS");
			m_greetz.AddString("hugo Vinagre [Extreme testing]");
			m_greetz.AddString("TAo-AAS");
			m_greetz.AddString("TimEr [Site Graphics and more]");
//			m_greetz.AddString("Vir|us");
			return TRUE;
		}

}}
