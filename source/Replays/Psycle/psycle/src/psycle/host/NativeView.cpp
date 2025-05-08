///\file
///\brief implementation file for psycle::host::CNativeView.
#include <psycle/host/detail/project.private.hpp>
#include "NativeView.hpp"
#include "NativeGraphics.hpp"
#include "FrameMachine.hpp"
#include "Machine.hpp"
#include "LuaPlugin.hpp"
#include "NewVal.hpp"
#include "Canvas.hpp"
#include "MfcUi.hpp"

#include "Plugin.hpp" // For default parameter value.
#include "InputHandler.hpp" //for undos

#define DEFAULT_ROWWIDTH  150


namespace psycle { namespace host {

	extern CPsycleApp theApp;

  using namespace ui;
		
	PsycleConfig::MachineParam* CNativeView::uiSetting;

/*
    BEGIN_MESSAGE_MAP(PsycleUiParamView, CWnd)
			ON_WM_CREATE()	
      ON_WM_DESTROY()
			ON_WM_PAINT()
      ON_WM_LBUTTONDOWN()
      ON_WM_RBUTTONDOWN()
			ON_WM_LBUTTONDBLCLK()
			ON_WM_MOUSEMOVE()
			ON_WM_LBUTTONUP()
			ON_WM_RBUTTONUP()      
		END_MESSAGE_MAP()

*/
    PsycleUiParamView::PsycleUiParamView(CFrameMachine* frame, Machine* effect) :
          CBaseParamView(frame), menu_container_(new ui::MenuContainer())  {         
    }

    int PsycleUiParamView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
		{
			if (CWnd::OnCreate(lpCreateStruct) == -1)
			{
				return -1;
			}            
			return 0;
		}

    void PsycleUiParamView::OnDestroy() {       
    }

		BOOL PsycleUiParamView::PreCreateWindow(CREATESTRUCT& cs)
		{
			if (!CWnd::PreCreateWindow(cs))
				return FALSE;
			
			cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
			cs.style &= ~WS_BORDER;
			return TRUE;
		}				    

    void PsycleUiParamView::WindowIdle() {       
      if (!viewport_.expired() && viewport_.lock()->IsSaving()) {
        viewport_.lock()->Flush();
      }      
    }

    void PsycleUiParamView::OnSize(UINT nType, int cx, int cy) {
      CWnd* child = GetWindow(GW_CHILD);
      if (child) {
        child->MoveWindow(0, 0, cx, cy);
      }      
    }

    bool PsycleUiParamView::GetViewSize(CRect& rect) {
      rect.top = 0;
      rect.left = 0;
      rect.right = 1000;
      rect.bottom = 1000;
      if (!viewport_.expired()) {
        rect.right = viewport_.lock()->dim().width();  
        rect.bottom = viewport_.lock()->dim().height();
      }
      return true;
    }

    void PsycleUiParamView::OnReload(Machine* mac)
    {      
      LuaPlugin* plugin = dynamic_cast<LuaPlugin*>(mac);
      assert(plugin);
      using namespace ui;
      set_viewport(!plugin->viewport().expired() ? plugin->viewport().lock() 
                                                 : Viewport::Ptr());      
      Node::Ptr menu_root_node;
      if (!plugin->proxy().menu_root_node().expired()) {
        menu_root_node = plugin->proxy().menu_root_node().lock();
      }
      set_menu(menu_root_node);
    }

	  void PsycleUiParamView::Close(Machine* mac)
    {     
      if (!viewport_.expired()) {
        set_viewport(Viewport::Ptr());        
      }
    }

    void PsycleUiParamView::set_menu(const ui::Node::Ptr& root_node) {
      using namespace ui;
      menu_container_->clear();              
      machine_menu_.reset(new Node());
      if (root_node) {        
        machine_menu_->AddNode(root_node);              
        menu_container_->set_root_node(machine_menu_);
//         mfc::MenuContainerImp* menucontainer_imp = dynamic_cast<mfc::MenuContainerImp*>(menu_container_->imp());
//         assert(menucontainer_imp);
//         menucontainer_imp->set_menu_window(parentFrame, machine_menu_);
      }      
    }

    void PsycleUiParamView::set_viewport(const ui::Viewport::Ptr& viewport) {      
      if (!viewport_.expired()) {
        viewport_.lock()->set_parent(0);        
      }
      if (viewport) {
//         ui::mfc::WindowImp* imp = (ui::mfc::WindowImp*) viewport->imp();            
//         imp->SetParent(this);       
        viewport->Show();
        viewport->UpdateAlign();
        SetActiveWindow();
        viewport_ = viewport;
      }
    }

/*
   BEGIN_MESSAGE_MAP(CNativeView, CWnd)
			ON_WM_CREATE()
			ON_WM_SETFOCUS()
			ON_WM_PAINT()
			ON_WM_LBUTTONDOWN()
      ON_WM_RBUTTONDOWN()
			ON_WM_LBUTTONDBLCLK()
			ON_WM_MOUSEMOVE()
			ON_WM_LBUTTONUP()
			ON_WM_RBUTTONUP()
      ON_WM_KEYDOWN()
			ON_WM_KEYUP()
		END_MESSAGE_MAP()
*/


		CNativeView::CNativeView(CFrameMachine* frame, Machine* effect)
		:CBaseParamView(frame)
		,ncol(0)
		,numParameters(0)
		,parspercol(0)
		,colwidth(DEFAULT_ROWWIDTH)
		,istweak(false)
		,finetweak(false)
		,ultrafinetweak(false)
		,tweakpar(0)
		,tweakbase(0)
		,visualtweakvalue(0.0f)
		,minval(0)
		,maxval(0)
		,sourcepoint(0)
		,sourcex(0)
		,positioning(false)
		,allowmove(false)
		,prevval(0)
		,painttimer(0)
		{
			SelectMachine(effect);      
		}

		int CNativeView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
		{
			if (CWnd::OnCreate(lpCreateStruct) == -1)
			{
				return -1;
			}      
			return 0;
		}
		BOOL CNativeView::PreCreateWindow(CREATESTRUCT& cs)
		{
			if (!CWnd::PreCreateWindow(cs))
				return FALSE;
			
			cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
			cs.style &= ~WS_BORDER;
			return TRUE;
		}

		void CNativeView::OnSetFocus(CWnd* pOldWnd) 
		{
			CWnd::OnSetFocus(pOldWnd);
			GetParent()->SetFocus();
		}

		void CNativeView::OnPaint() 
		{
			CRgn pRgn;
			pRgn.CreateRectRgn(0, 0, 0, 0);
			GetUpdateRgn(&pRgn, FALSE);

			int realheight = uiSetting->dialheight+1;
			int realwidth = colwidth+1;
			int const K_XSIZE2=uiSetting->dialwidth;
			int const infowidth=colwidth-uiSetting->dialwidth;
			const COLORREF titleColor = uiSetting->titleColor;

			CPaintDC dc(this);
			CRect rect;
			GetClientRect(&rect);

			CDC bufferDC;
			CBitmap bufferBmp;
			bufferDC.CreateCompatibleDC(&dc);
			bufferBmp.CreateCompatibleBitmap(&dc,rect.right-rect.left,rect.bottom-rect.top);
			CBitmap* oldbmp = bufferDC.SelectObject(&bufferBmp);
			CFont *oldfont=bufferDC.SelectObject(&uiSetting->font);
			CDC knobDC;
			CBitmap* oldKnobbmp;
			knobDC.CreateCompatibleDC(&bufferDC);
			oldKnobbmp=knobDC.SelectObject(&uiSetting->dial);

			int y_knob, x_knob,knob_c;
			char parName[64];
			y_knob = x_knob = knob_c = 0;
			std::memset(parName,0,64);
			++painttimer;
			for (int c=0; c<numParameters; c++)
			{
				_pMachine->GetParamName(c,parName);
				int type = _pMachine->GetParamType(c);

				if(type == 5) // ON/OFF Switch
				{
					char buffer[64];					
					_pMachine->GetParamValue(c, buffer);
					int min_v, max_v;
					min_v = max_v = 0;				  
					_pMachine->GetParamRange(c,min_v,max_v);				  				  
					float rel_v = _pMachine->GetParamValue(c) - min_v;				  
					int koffset = K_XSIZE2;
					int maxf = 5;				  
					int oldoffset = InfoLabel::xoffset;
					InfoLabel::xoffset += koffset;
					InfoLabel::Draw(bufferDC,x_knob,y_knob,colwidth,parName,"");
					InfoLabel::xoffset = oldoffset;
					std::vector<int> on;
					on.push_back(rel_v);
					InfoLabel::DrawLEDs(bufferDC,x_knob+koffset,y_knob, colwidth-koffset, maxf, on, blink[c]);
				} else
				if(type == 4) // LED
				{
					if (painttimer % 15 == 0) blink[c] = !blink[c];			    
					int maxf, koffset, amp_v, x_knob, y_knob;        
					std::vector<int> on;
					ComputeLeds(c, on, maxf, koffset, amp_v, x_knob, y_knob);				  
					InfoLabel::xoffset += koffset;
					InfoLabel::Draw(bufferDC, x_knob, y_knob, colwidth, parName, "");
					InfoLabel::xoffset -= koffset;;
					InfoLabel::DrawLEDs(bufferDC,x_knob+koffset,y_knob, colwidth-koffset, maxf, on, blink[c]);
				} else
				if(type == 3) // INFO unformatted
				{
					char buffer[64];					
					_pMachine->GetParamValue(c, buffer);
					InfoLabel::Draw(bufferDC,x_knob,y_knob,colwidth,parName,buffer);					
				} else
				if(type == 2) // STATE
				{
					char buffer[64];
					int min_v, max_v;
					min_v = max_v = 0;

					_pMachine->GetParamRange(c,min_v,max_v);
					_pMachine->GetParamValue(c,buffer);

					int const amp_v = max_v - min_v;
					float rel_v;
					if ( istweak && c == tweakpar)
					{
						rel_v = visualtweakvalue - min_v;
					} else {
						rel_v = _pMachine->GetParamValue(c) - min_v;
					}
					Knob::Draw(bufferDC,knobDC,x_knob,y_knob,rel_v/amp_v);
					if ( istweak && c == tweakpar)
					{
						InfoLabel::DrawHLight(bufferDC,K_XSIZE2+x_knob,y_knob,infowidth,parName,buffer);
					}
					else
					{
						InfoLabel::Draw(bufferDC,K_XSIZE2+x_knob,y_knob,infowidth,parName,buffer);
					}
				}
				else if(type == 1) // HEADER
				{
					InfoLabel::DrawHeader(bufferDC,x_knob,y_knob,colwidth,parName);
				}
				else
				{
					InfoLabel::Draw(bufferDC,x_knob,y_knob,colwidth,"","");
				}
				bufferDC.Draw3dRect(x_knob-1,y_knob-1,realwidth+1,realheight+1,titleColor,titleColor);
				y_knob += realheight;

				++knob_c;

				if (knob_c >= parspercol)
				{
					knob_c = 0;
					x_knob += realwidth;
					y_knob = 0;
				}
			}

			int exess= parspercol*ncol;
			if ( exess > numParameters )
			{
				for (int c=numParameters; c<exess; c++)
				{
					InfoLabel::Draw(bufferDC,x_knob,y_knob,colwidth,"","");
					dc.Draw3dRect(x_knob-1,y_knob-1,colwidth+2,uiSetting->dialheight+2,titleColor,titleColor);
					y_knob += realheight;
				}
			}

			dc.BitBlt(0,0,rect.right,rect.bottom,&bufferDC,0,0,SRCCOPY);
			knobDC.SelectObject(oldKnobbmp);
			knobDC.DeleteDC();

			bufferDC.SelectObject(oldbmp);
			bufferDC.SelectObject(oldfont);
			bufferDC.DeleteDC();

			if(istweak  && !allowmove) {
				//Reposition the cursor, so that it doesn't move out of the window.
				//This solves the problem where the cursor cannot move more because it reaches the top or bottom of the screen.
				CPoint point;
				point.x = sourcex;
				point.y = sourcepoint;
				tweakbase = helpers::math::round<int,float>(visualtweakvalue);
				ClientToScreen(&point);
				positioning = true;
				SetCursorPos(point.x, point.y);
			}
		}

		void CNativeView::OnLButtonDown(UINT nFlags, CPoint point) 
		{      
			tweakpar = ConvertXYtoParam(point.x,point.y);
			if ((tweakpar > -1) && (tweakpar < numParameters))
			{
				sourcepoint = point.y;
				sourcex = point.x;
				tweakbase = _pMachine->GetParamValue(tweakpar);
				prevval = tweakbase;
				_pMachine->GetParamRange(tweakpar,minval,maxval);
				istweak = true;
				visualtweakvalue= tweakbase;
				PsycleGlobal::inputHandler().AddMacViewUndo();
				while (ShowCursor(FALSE) >= 0);
				SetCapture();
			}
			else
			{
				istweak = false;
			}
			CWnd::OnLButtonDown(nFlags, point);
		}

    void CNativeView::OnRButtonDown(UINT nFlags, CPoint point) 
		{
      CWnd::OnRButtonDown(nFlags, point);
    }

		void CNativeView::OnLButtonDblClk(UINT nFlags, CPoint pt)
		{      
			if( _pMachine->_type == MACH_PLUGIN)
			{
				int par = ConvertXYtoParam(pt.x,pt.y);
				if(par>=0 && par <= _pMachine->GetNumParams() )
				{
					PsycleGlobal::inputHandler().AddMacViewUndo();
					_pMachine->SetParameter(par,  ((Plugin*)_pMachine)->GetInfo()->Parameters[par]->DefValue);
				}
			}
			Invalidate(false);
			CWnd::OnLButtonDblClk(nFlags, pt);
		}

		void CNativeView::OnMouseMove(UINT nFlags, CPoint point) 
		{   
			///\todo: positioning true causes a lag tweaking lua machines 
			/// Reason ?  
			if (_pMachine->_type == MACH_LUA) {        
			  positioning = false;
			}
			if (istweak && !positioning && _pMachine->GetParamType(tweakpar)!=4)
			{
				///\todo: This code fools some VST's that have quantized parameters (i.e. tweaking to 0x3579 rounding to 0x3000)
				///       It should be interesting to know what is "somewhere else".
/*				int curval = _pMachine->GetParamValue(tweakpar);
				tweakbase -= prevval-curval;					//adjust base for tweaks from somewhere else
				if(tweakbase<minval) tweakbase=minval;
				if(tweakbase>maxval) tweakbase=maxval;
*/
				if (( ultrafinetweak && !(nFlags & MK_SHIFT )) || //shift-key has been left.
					( !ultrafinetweak && (nFlags & MK_SHIFT))) //shift-key has just been pressed
				{
					tweakbase = _pMachine->GetParamValue(tweakpar);
					ultrafinetweak=!ultrafinetweak;
				}
				else if (( finetweak && !(nFlags & MK_CONTROL )) || //control-key has been left.
					( !finetweak && (nFlags & MK_CONTROL))) //control-key has just been pressed
				{
					tweakbase = _pMachine->GetParamValue(tweakpar);
					finetweak=!finetweak;
				}

				double freak;
				int screenh = uiSetting->deskrect.bottom;
				if ( ultrafinetweak ) freak = 0.5f;
				else if (maxval-minval < screenh/4) freak = (maxval-minval)/float(screenh/4);
				else if (maxval-minval < screenh*2/3) freak = (maxval-minval)/float(screenh/3);
				else freak = (maxval-minval)/float(screenh*3/5);
				if (finetweak) freak/=5;

				double nv = (double)(sourcepoint - point.y)*freak + (double)tweakbase;

				allowmove = (prevval==helpers::math::round<int,float>(nv));
				if (nv < minval) {
					nv = minval;
					allowmove = false;
				}
				if (nv > maxval) {
					nv = maxval;
					allowmove = false;
				}
				visualtweakvalue = nv;
				prevval=helpers::math::round<int,float>(nv);
				_pMachine->SetParameter(tweakpar,prevval);
				parentFrame->Automate(tweakpar, prevval, false, minval);
				Invalidate(false);
			}
			positioning=false;
			CWnd::OnMouseMove(nFlags, point);
		}

		void CNativeView::OnLButtonUp(UINT nFlags, CPoint point) 
		{           
			istweak = false;
			ReleaseCapture();			
			if (_pMachine->GetParamType(tweakpar)==5) {							
				int realheight = uiSetting->dialheight+1;
			    int realwidth = colwidth+1;
				int min_v, max_v;
				min_v = max_v = 0;				  
				_pMachine->GetParamRange(tweakpar,min_v,max_v);				
				int x_knob = (tweakpar / parspercol)*realwidth;
				int y_knob = (tweakpar % parspercol)*realheight;				
			    bool found = false;				
				char buffer[64];					
				_pMachine->GetParamValue(tweakpar, buffer);
				std::string v(buffer);				
				int koffset =uiSetting->dialwidth;				
				const int maxw = (colwidth-koffset)/5;
				const int w = std::min(maxw, (colwidth-koffset)/1);
				const int border = w/5;				
				for (int i=0; i < 1; ++i) {
				  CRect r = CRect(koffset+x_knob+i*w+border, y_knob, koffset+x_knob+i*w+border+w-2*border, y_knob+realheight);
				  if (r.PtInRect(point)) {
					 found = true;					 
					 break;
				  }
				}
				if (found) {
				  _pMachine->SetParameter(tweakpar, !_pMachine->GetParamValue(tweakpar));
				}
			} else
			if (_pMachine->GetParamType(tweakpar)==4) {																
				int realheight = uiSetting->dialheight+1;			  
				int maxf, koffset, amp_v, x_knob, y_knob;
				std::vector<int> on;
				ComputeLeds(tweakpar, on, maxf, koffset, amp_v, x_knob, y_knob);
				const int maxw = (colwidth-koffset)/maxf;
				const int w = std::min(maxw, (colwidth-koffset)/amp_v);
				const int border = w/5;				
				bool found = false;
				int val = -1;
				for (int i=0; i < amp_v; ++i) {
				  CRect r = CRect(koffset+x_knob+i*w+border, y_knob, koffset+x_knob+i*w+border+w-2*border, y_knob+realheight);
				  if (r.PtInRect(point)) {
					 found = true;
					 val = i;
					 break;
				  }
				}
				if (found) {
				  _pMachine->SetParameter(tweakpar,val);
				}
			}
			while (ShowCursor(TRUE) < 0);
			Invalidate();
			_pMachine->AfterTweaked(tweakpar);
			CWnd::OnLButtonUp(nFlags, point);
		}

		void CNativeView::OnRButtonUp(UINT nFlags, CPoint point) 
		{      
			tweakpar = ConvertXYtoParam(point.x,point.y);

			if ((tweakpar > -1) && (tweakpar < numParameters))
			{
				if (nFlags & MK_CONTROL)
				{
					
/*					Global::song().seqBus = machine()._macIndex;
					CMainFrame& mframe = *static_cast<CMainFrame*>(theApp.m_pMainWnd);
					mframe.UpdateComboGen(FALSE);
					CComboBox *cb2=reinterpret_cast<CComboBox *>(mframe.m_machineBar.GetDlgItem(IDC_AUXSELECT));
					cb2->SetCurSel(AUX_PARAMS);
					Global::song().auxcolSelected=tweakpar;
					mframe.UpdateComboIns();
*/
				}
				else 
				{		
					int min_v=1;
					int max_v=1;
					char name[64],title[128];
					memset(name,0,64);

					_pMachine->GetParamRange(tweakpar,min_v,max_v);
					_pMachine->GetParamName(tweakpar,name);
					std::sprintf
						(
						title, "Param:'%.2x:%s' (Range from %d to %d)\0",
						tweakpar,
						name,
						min_v,
						max_v
						);

					CNewVal dlg(machine()._macIndex,tweakpar,_pMachine->GetParamValue(tweakpar),min_v,max_v,title);
					if ( dlg.DoModal() == IDOK)
					{
						PsycleGlobal::inputHandler().AddMacViewUndo();
						_pMachine->SetParameter(tweakpar,(int)dlg.m_Value);
						parentFrame->Automate(tweakpar,(int)dlg.m_Value,false, min_v);
					}
					Invalidate(false);
				}
			}
			CWnd::OnRButtonUp(nFlags, point);
		}

		int CNativeView::ConvertXYtoParam(int x, int y)
		{
			int realheight = uiSetting->dialheight+1;
			int realwidth = colwidth+1;
			if (y/realheight >= parspercol ) return -1; //this if for VST's that use the native gui.
			return (y/realheight) + ((x/realwidth)*parspercol);
		}

		bool CNativeView::GetViewSize(CRect& rect)
		{     
			int realheight = uiSetting->dialheight+1;
			int realwidth = colwidth+1;
			rect.left= rect.top = 0;
			rect.right = ncol * realwidth;
			rect.bottom = parspercol * realheight;
			return true;
		}

		void CNativeView::SelectMachine(Machine* pMachine)
		{
			_pMachine = pMachine;
			numParameters = _pMachine->GetNumParams();
			ncol = _pMachine->GetNumCols();
			if ( ncol == 0 )
			{
				ncol = 1;
				while ( (numParameters/ncol)*uiSetting->dialheight > ncol*colwidth ) ncol++;
			}
			parspercol = numParameters/ncol;
			if ( parspercol*ncol < numParameters) parspercol++; // check if all the parameters are visible.
			if (pMachine->_type == MACH_LUA) {
			   blink.clear();
			   for (int i=1; i<numParameters; ++i) {
				  blink.push_back(0);
			   }
			}
		}

    void CNativeView::ComputeLeds(int tweakpar, std::vector<int>& on, int &maxf, int &koffset, int &amp_v, int &x_knob, int &y_knob) {
      int realheight = uiSetting->dialheight+1;
			int realwidth = colwidth+1;
			int min_v, max_v = 0;						  
			_pMachine->GetParamRange(tweakpar,min_v,max_v);
			amp_v = max_v - min_v;
      on.clear(); for (int i=0; i < amp_v; ++i) on.push_back(0);
      float rel_v = _pMachine->GetParamValue(tweakpar) - min_v;				  
			x_knob = (tweakpar / parspercol)*realwidth;
			y_knob = (tweakpar % parspercol)*realheight;										
      char buffer[64];		
			_pMachine->GetParamValue(tweakpar, buffer);
			std::string v(buffer);
			int const K_XSIZE2=uiSetting->dialwidth;      
			bool isnumber=false, hasdescr = false;			
		  if (v[0]=='*' || v[0]=='~' || v[0]=='b') {						
        hasdescr = true;        
        int i = 0;
        for (std::string::iterator it = v.begin(); it!=v.end() && i < on.size();++it, ++i) {				
          if (*it=='*') on[i] = 1; else
			    if (*it=='b') on[i] = 2;
        }        
      } else {        
        on[rel_v] = 1;         
        if (v[0]!='K' && v[0]!='S' && v[0]!='M') {
					 isnumber = true;
        }
      }				  
      koffset = 0;
      maxf = 5;
      if (!isnumber) {
			  // check for alignment suffix
				int start = (v.size()>amp_v && hasdescr) ? amp_v : 0;
				for (int k = start; k < v.size(); ++k) {
				  if (v[k]=='K') koffset = K_XSIZE2; else
				  if (v[k]=='S') maxf = 10; else
				  if (v[k]=='M') maxf = 7;
				}
			}
    }
      
	}   // namespace
}   // namespace
