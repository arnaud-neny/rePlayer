///\file
///\brief implementation file for psycle::host::CVolumeDlg.
#include <psycle/host/detail/project.private.hpp>
#include "ChannelMappingDlg.hpp"
#include <psycle/host/Machine.hpp>
#include <psycle/host/Song.hpp>
#include "ExclusiveLock.hpp"
#include <string>
#include <sstream>

namespace psycle { namespace host {
		CChannelMappingDlg::CChannelMappingDlg(Wire& wire, CWnd* mainView_, CWnd* pParent)
			: CDialog(CChannelMappingDlg::IDD, pParent)
			, m_wire(wire)
			, mainView(mainView_)
		{
		}

		void CChannelMappingDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
		}

/*
		BEGIN_MESSAGE_MAP(CChannelMappingDlg, CDialog)
			ON_COMMAND(IDC_AUTOWIRE, OnAutoWire)
			ON_COMMAND(IDC_UNSELECT, OnUnselectAll)
			ON_COMMAND_RANGE(IDC_CHK_CHANMAP_0, IDC_CHK_CHANMAP_0+255, OnCheckChanMap)
			ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 255, OnToolTipText)
			ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 255, OnToolTipText)
		END_MESSAGE_MAP()
*/

		BOOL CChannelMappingDlg::PreTranslateMessage(MSG* pMsg) 
		{
			if (pMsg->message == WM_KEYDOWN)
			{
				if (pMsg->wParam == VK_UP ||pMsg->wParam == VK_DOWN
					|| pMsg->wParam == VK_LEFT ||pMsg->wParam == VK_RIGHT
					|| pMsg->wParam == VK_RETURN ||pMsg->wParam == VK_TAB
					|| pMsg->wParam == VK_SPACE) {
					return CDialog::PreTranslateMessage(pMsg);
				}
				else if (pMsg->wParam == VK_ESCAPE) {
					PostMessage (WM_CLOSE);
					return true;
				}
				else {
					mainView->SendMessage(pMsg->message,pMsg->wParam,pMsg->lParam);
					return true;
				}
			}
			else if (pMsg->message == WM_KEYUP)
			{
				if (pMsg->wParam == VK_UP ||pMsg->wParam == VK_DOWN
					|| pMsg->wParam == VK_LEFT ||pMsg->wParam == VK_RIGHT
					|| pMsg->wParam == VK_RETURN ||pMsg->wParam == VK_TAB
					|| pMsg->wParam == VK_SPACE) {
					return CDialog::PreTranslateMessage(pMsg);
				}
				else if (pMsg->wParam == VK_ESCAPE) {
					return true;
				}
				else {
					mainView->SendMessage(pMsg->message,pMsg->wParam,pMsg->lParam);
					return true;
				}
			}
			return CDialog::PreTranslateMessage(pMsg);
		}
		BOOL CChannelMappingDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			EnableToolTips(TRUE);
			EnableTrackingToolTips(TRUE);
			HGDIOBJ hFont = GetStockObject( DEFAULT_GUI_FONT );
			CFont font;
			font.Attach( hFont );

			const Machine & dstMac = m_wire.GetDstMachine();
			const Machine & srcMac = m_wire.GetSrcMachine();
			//This is done only to prevent a window too big.
			int srcpins=std::min(srcMac.GetNumOutputPins(),24);
			int dstpins=std::min(dstMac.GetNumInputPins(),16);
			//
			// Add names.
			//
			{
				int y = 16;
				for (int i=0;i< dstpins; i++) {
					std::stringstream s;
					s << std::setprecision(2) << i << ": " << dstMac.GetInputPinName(i).substr(0,9);
					int x = 60 + i*40;
					CRect rect(x,y,x+40,y+9);
					MapDialogRect(rect);
					CStatic* label = new CStatic();
					label->Create(s.str().c_str(),WS_CHILD|WS_VISIBLE,rect,this);
					label->SetFont(&font,false);
					dstnames.push_back(label);
				}
			}
			//
			// Create, position and mark checkboxes
			//
			{
				std::vector<std::vector<bool>> checked;
				FillCheckedVector(checked, srcpins, dstpins);

				for (int s=0;s< srcpins; s++) {
					for (int d=0;d< dstpins; d++) {
						std::stringstream str;
						if (d==0) {
							str << std::setprecision(2) << s << ": " << srcMac.GetOutputPinName(s).substr(0,9);
						}
						AddButton(s,d,dstpins, checked[s][d], font, str.str());
					}
				}
			}
			//
			// Position buttons and resize window.
			//
			{
				CRect rect2;
				CRect margins(0,0,7,7);
				// calculate frame size (See in AddButton method for sizes)
				CRect rect(0,0,60 + dstpins*40, 30 + srcpins*9);
				MapDialogRect(&margins);
				MapDialogRect(&rect);
				rect.bottom+=margins.bottom; //Add space between checks and buttons.
				GetDlgItem(IDOK)->GetClientRect(&rect2);
				rect.bottom+=2*rect2.bottom; //add size of (two) buttons.
				GetDlgItem(IDOK)->SetWindowPos(NULL,
					rect.right - rect2.right,
					rect.bottom - (2*rect2.bottom),0,0,SWP_NOZORDER | SWP_NOSIZE);
				GetDlgItem(IDCANCEL)->SetWindowPos(NULL,
					rect.right - rect2.right,
					rect.bottom - rect2.bottom,0,0,SWP_NOZORDER | SWP_NOSIZE);
				GetDlgItem(IDC_AUTOWIRE)->GetClientRect(&rect2);
				GetDlgItem(IDC_AUTOWIRE)->SetWindowPos(NULL,
					margins.right,
					rect.bottom - (2*rect2.bottom),0,0,SWP_NOZORDER | SWP_NOSIZE);
				GetDlgItem(IDC_UNSELECT)->SetWindowPos(NULL,
					margins.right,
					rect.bottom - rect2.bottom,0,0,SWP_NOZORDER | SWP_NOSIZE);
				//add frame margins
				rect.bottom+=margins.bottom;
				rect.right+=margins.right;
				CalcWindowRect(rect);
				SetWindowPos(NULL,0,0,rect.right - rect.left,rect.bottom-rect.top, SWP_NOZORDER | SWP_NOMOVE);
			}
			return FALSE;
		}

		void CChannelMappingDlg::OnAutoWire()
		{
			CExclusiveLock lock(&Global::song().semaphore, 2, true);
			const Machine & dstMac = m_wire.GetDstMachine();
			const Machine & srcMac = m_wire.GetSrcMachine();
			//This is done only to prevent a window too big.
			int srcpins=std::min(srcMac.GetNumOutputPins(),24);
			int dstpins=std::min(dstMac.GetNumInputPins(),16);

			m_wire.SetBestMapping();
			std::vector<std::vector<bool>> vector;
			FillCheckedVector(vector,srcpins, dstpins);
			int count=0;
			for (int s=0;s< srcpins; s++) {
				for (int d=0;d< dstpins; d++, count++) {
					buttons[count]->SetCheck(vector[s][d]);
				}
			}
		}
		void CChannelMappingDlg::OnUnselectAll()
		{
			Wire::Mapping mapping;
			for(int count(0);count<buttons.size();count++) {
				buttons[count]->SetCheck(false);
			}
		}

		void CChannelMappingDlg::OnCheckChanMap(UINT index) 
		{
			//int i = index-IDC_CHK_CHANMAP_0;
			//buttons[i]->DoSomething();
		}

		INT_PTR CChannelMappingDlg::OnToolHitTest(CPoint point, TOOLINFO * pTI) const
		{
/*
			int row;
			RECT cellrect;   // cellrect		- to hold the bounding rect

			row = CheckFromPoint(point, cellrect);
			if ( row < 0) 
				return -1;

			//set up the TOOLINFO structure.
			pTI->rect = cellrect;
			pTI->hwnd = m_hWnd;
			pTI->uId = (UINT)((row));   //The ‘uId’ is assigned a value according to the check.
			pTI->lpszText = LPSTR_TEXTCALLBACK;
			pTI->uFlags |= TTF_ALWAYSTIP;
			return pTI->uId;
*/
			return -1;
		}
		//Define OnToolTipText(). This is the handler for the TTN_NEEDTEXT notification from 
		//support ansi and unicode 
		BOOL CChannelMappingDlg::OnToolTipText( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
		{
/*
			// need to handle both ANSI and UNICODE versions of the message
			TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
			TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;
			const Machine & dstMac = m_wire.GetDstMachine();
			const Machine & srcMac = m_wire.GetSrcMachine();
			//This is done only to prevent a window too big.
			UINT dstpins = std::min(dstMac.GetNumInputPins(),16);
			UINT srcpos= pNMHDR->idFrom/dstpins;
			UINT dstpos= pNMHDR->idFrom%dstpins;
			std::stringstream ss;
			ss << srcpos << ":" << srcMac.GetOutputPinName(srcpos) 
				<< " => " << dstpos << ":" << dstMac.GetInputPinName(dstpos);
				

		#ifndef _UNICODE
			if (pNMHDR->code == TTN_NEEDTEXTA)
				lstrcpyn(pTTTA->szText, ss.str().c_str(), 80);
			else
				_mbstowcsz(pTTTW->szText, ss.str().c_str(), 80);
		#else
			if (pNMHDR->code == TTN_NEEDTEXTA)
				_wcstombsz(pTTTA->szText, ss.str().c_str(), 80);
			else
				lstrcpyn(pTTTW->szText, ss.str().c_str(), 80);
		#endif
			*pResult = 0;
*/

			return TRUE;    
		}
		void CChannelMappingDlg::OnOK() 
		{
			CExclusiveLock lock(&Global::song().semaphore, 2, true);
			Wire::Mapping mapping;
			//This is done only to prevent a window too big.
			int srcpins = std::min(m_wire.GetSrcMachine().GetNumOutputPins(),24);
			int dstpins = std::min(m_wire.GetDstMachine().GetNumInputPins(),16);
			int count=0;
			for (int s=0;s< srcpins; s++) {
				for (int d=0;d< dstpins; d++, count++) {
					if (buttons[count]->GetCheck()) {
						mapping.push_back(Wire::PinConnection(s,d));
					}
				}
			}
			m_wire.ChangeMapping(mapping);

			for(int i=0; i < buttons.size();i++) {
				delete buttons[i];
			}
			for(int i=0; i < dstnames.size();i++) {
				delete dstnames[i];
			}

			CDialog::OnOK();
		}

		void CChannelMappingDlg::OnCancel() 
		{
			for(int i=0; i < buttons.size();i++) {
				delete buttons[i];
			}
			for(int i=0; i < dstnames.size();i++) {
				delete dstnames[i];
			}

			CDialog::OnCancel();
		}

		void CChannelMappingDlg::FillCheckedVector(std::vector<std::vector<bool>>& checked, int srcpins, int dstpins)
		{
			checked.resize(srcpins);
			for (int s=0;s< srcpins; s++) {
				checked[s].resize(dstpins);
			}
			const Wire::Mapping &mapping = m_wire.GetMapping();
			for (int i(0);i<mapping.size();i++) {
				const Wire::PinConnection & con = mapping[i];
				//This is done only to prevent a window too big.
				if (con.first < srcpins && con.second < dstpins) {
					checked[con.first][con.second] = true;
				}
			}
		}
		void CChannelMappingDlg::AddButton(int yRel, int xRel, int amountx, bool checked, CFont &font, std::string text)
		{
			int x = 30 + xRel*40;
			int y = 30 + yRel*9;
			CButton* m_button = new CButton();
			if (xRel==0) {
				CRect rect(7,y,x+38,y+8);
				MapDialogRect(&rect);
				m_button->Create(text.c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON|BS_CHECKBOX|BS_LEFTTEXT,
					rect,this,IDC_CHK_CHANMAP_0+((yRel*amountx)+xRel));
			}
			else {
				CRect rect(x,y,x+38,y+8);
				MapDialogRect(&rect);
				m_button->Create(_T(""),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON|BS_CHECKBOX|BS_LEFTTEXT,
					rect,this,IDC_CHK_CHANMAP_0+((yRel*amountx)+xRel));
			}
			m_button->SetFont(&font,false);
			m_button->ShowWindow(SW_SHOW);
			
			m_button->SetCheck(checked);
			buttons.push_back(m_button);
		}


		int CChannelMappingDlg::CheckFromPoint(CPoint point, RECT& rect) const
		{
			int result=-1;
			WINDOWPLACEMENT plc;
			for(int i=0; i < buttons.size();i++) {
				BOOL done = buttons[i]->GetWindowPlacement(&plc);
				CRect brect(plc.rcNormalPosition);
				if (done && brect.PtInRect(point)) {
					rect = plc.rcNormalPosition;
					result = i;
					break;
				}
			}
			return result;
		}
	}   // namespace
}   // namespace
