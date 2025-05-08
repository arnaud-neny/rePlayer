///\file
///\brief implementation file for psycle::host::CPlotterDlg.
#include <psycle/host/detail/project.private.hpp>
#include "PlotterDlg.hpp"
#include "InputHandler.hpp"
#include "Machine.hpp"
#include "DPI.hpp"
#include <psycle/helpers/dsp.hpp>
#include <cmath>
#include "lua.hpp"
#include "LuaArray.hpp"
#include "LuaInternals.hpp"
#include "Ui.hpp"


namespace psycle { namespace host {

	  
		/// master dialog
		CPlotterDlg::CPlotterDlg(CWnd* wndView) 
			: CDialog(CPlotterDlg::IDD,  wndView), data_(0), len_(0),
			  minpeak_(0), maxpeak_(1)
		{
			CDialog::Create(CPlotterDlg::IDD,  wndView);
			/*  // testcase
			data_ = &testdata[0];			
			data_[0]= 0.5;
			data_[1]= 0.1;
			data_[2]= 0.2;
			data_[3]= -0.5;
			data_[4]= -0.1;
			data_[5]= 0.01;
			len_ = 6;
			parse_peak();*/
		}

		CPlotterDlg::~CPlotterDlg() {
		}


		void CPlotterDlg::do_script() {
			lua_State* L = luaL_newstate();
            luaL_openlibs(L);
			std::string path = Global::configuration().GetAbsoluteLuaDir() + "/plotblit.lua";
			int status = luaL_loadfile(L, path.c_str());
			if (status) {
        const char* msg = lua_tostring(L, -1);
	      ui::alert(msg);
			  lua_close (L);
			  return;
            }
//            LuaArrayBind::register_module(L);             
//			LuaWaveOscBind::register_module(L);             
			status = lua_pcall(L, 0, LUA_MULTRET, 0);
			if (status) {
        const char* msg = lua_tostring(L, -1);
	      ui::alert(msg);
      }  else {
			  lua_getglobal(L, "work");
			  status = lua_pcall(L, 0, 1, 0);
			  if (status) {
          const char* msg = lua_tostring(L, -1);
	        ui::alert(msg);
        }  else {
			    PSArray* x = *(PSArray **)luaL_checkudata(L, -1, "array_meta");			
			    assert(x);
			    this->set_data(x->data(), x->len());
			  }
			}
			lua_close (L);
		}

		void CPlotterDlg::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
		}

/*
		BEGIN_MESSAGE_MAP(CPlotterDlg, CDialog)			
			ON_WM_PAINT()
	    ON_WM_ERASEBKGND()
			ON_WM_CLOSE()
		END_MESSAGE_MAP()
*/

		BOOL CPlotterDlg::OnInitDialog() 
		{
			CDialog::OnInitDialog();
			return TRUE;
		}

		void CPlotterDlg::OnCancel()
		{
			// DestroyWindow();			
		}

		void CPlotterDlg::OnClose()
		{
			CDialog::OnClose();	
			do_script();
			Invalidate(); 
			UpdateWindow();
			//DestroyWindow();
		}

		void CPlotterDlg::PostNcDestroy()
		{
			CDialog::PostNcDestroy();
			delete this;
		}

		void CPlotterDlg::OnPaint() 
		{
			CPaintDC dc(this); // device context for painting
			// Do not call CDialog::OnPaint() for painting messages
			CRect drawing_area;
			GetClientRect(&drawing_area);
			if (len_>0) {
				int xident = 20;			
				int yident = 20;
				int height = drawing_area.Height() - 2*yident;
				int width = drawing_area.Width() - xident;
				double range = maxpeak_-minpeak_;
				double step_height = height / (range);
			    double step_width = width / (double)len_;
				int start = floor(minpeak_)-1;
				int stop = maxpeak_;
				int fonth2 = 5;
				CPen * oldPen;
				oldPen = (CPen *) dc.SelectStockObject(WHITE_PEN);
				COLORREF backcolor = RGB(255,255,255);
				CBrush brush(backcolor); 
				int y0 = drawing_area.Height() - yident - step_height*(0-minpeak_);
				dc.MoveTo(xident, y0);
				dc.LineTo(xident+width, y0);
				dc.MoveTo(xident, yident);
				dc.LineTo(xident, yident+height);						
				for (int i=0; i < len_; i++) {
				  int x = xident+i*step_width;
				  int y = drawing_area.Height() - yident - step_height*(data_[i]-minpeak_);
				  dc.MoveTo(x, y0-2);
				  dc.LineTo(x, y0+2);
				  RECT r;
				  int size = 1;
				  r.left = x-size; r.top = y-size; r.right = x+size; r.bottom = y+size;
				  dc.FillRect(&r, &brush);
				}			
				for (double i = start; i <= stop; i+=0.5) {
				  CString str; 
				  str.Format(_T("%0.1f"), i); 
				  int y = drawing_area.Height() - yident - step_height*(i-minpeak_);
				  dc.TextOut(0, y-fonth2, str);
				  dc.MoveTo(15, y);
				  dc.LineTo(20, y);
				}
				dc.SelectObject(oldPen);
			}
		}
	
		BOOL CPlotterDlg::OnEraseBkgnd(CDC* pDC) 
		{
			BOOL res = CDialog::OnEraseBkgnd(pDC);		
			CRect drawing_area;
            GetClientRect(&drawing_area);
			COLORREF backcolor = RGB(0,0,0);
            CBrush brush_back_ground(backcolor); 
			pDC->FillRect(&drawing_area, &brush_back_ground);
			return res;
		}

		void CPlotterDlg::parse_peak() {
		    minpeak_ = 0; maxpeak_ = 0;
			for (int i = 0; i < len_; ++i) {
				if (i==0) minpeak_ = data_[0];
				minpeak_ = std::min(minpeak_, data_[i]);
				maxpeak_ = std::max(maxpeak_, data_[i]);
			}
		}
	}   // namespace
}   // namespace
