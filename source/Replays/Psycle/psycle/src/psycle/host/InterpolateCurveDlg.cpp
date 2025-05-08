///\file
///\brief implementation file for psycle::host::CInterpolateCurve.
#include <psycle/host/detail/project.private.hpp>
#include "InterpolateCurveDlg.hpp"
#include "DPI.hpp"

namespace psycle { namespace host {

		CInterpolateCurve::CInterpolateCurve(int startsel, int endsel,int _linesperbeat,CWnd* pParent)
			: CDialog(CInterpolateCurve::IDD, pParent)
			, startIndex(startsel)
			, numLines(endsel-startsel+1)
			, linesperbeat(_linesperbeat)
			, selectedGPoint(0)
			, bDragging(false)
			, y_range(65535)
		{
			kf = new keyframesstruct[numLines];
			kfresult = new int[numLines];
			//set default keyframe values
			kf[0].value = 32768;
			kf[0].curvetype = 1;
			kftwk = -1;
			for (int i = 1;i < numLines-1;i++)
			{
				kf[i].value = -1;
				kf[i].curvetype = 1;
			}
			kf[numLines-1].value = 32768;
			kf[numLines-1].curvetype = 1;

		}

		CInterpolateCurve::~CInterpolateCurve()
		{
			delete[] kf;
			delete[] kfresult;
		}

		void CInterpolateCurve::DoDataExchange(CDataExchange* pDX)
		{
			CDialog::DoDataExchange(pDX);
			DDX_Control(pDX, IDC_POS, m_Pos);
			DDX_Control(pDX, IDC_VAL, m_Value);
			DDX_Control(pDX, IDC_INTER_MIN, m_Min);
			DDX_Control(pDX, IDC_INTER_MAX, m_Max);
			DDX_Control(pDX, IDC_CURVE_TYPE, m_CurveType);
			DDX_Control(pDX, IDC_COMBOTWK, m_combotwk);
			DDX_Control(pDX, IDC_CHECKTWK, m_checktwk);
		}

/*
		BEGIN_MESSAGE_MAP(CInterpolateCurve, CDialog)
			ON_BN_CLICKED(IDOK, OnOk)
			ON_WM_PAINT()
			ON_WM_LBUTTONDOWN()
			ON_WM_LBUTTONUP()
			ON_WM_MOUSEMOVE()
			ON_WM_RBUTTONDOWN()
			ON_CBN_SELENDOK(IDC_CURVE_TYPE, OnSelendokCurveType)
			ON_EN_KILLFOCUS(IDC_POS, OnEnKillfocusPos)
			ON_EN_KILLFOCUS(IDC_VAL, OnEnKillfocusVal)
			ON_EN_KILLFOCUS(IDC_INTER_MIN, OnEnKillfocusMin)
			ON_EN_KILLFOCUS(IDC_INTER_MAX, OnEnKillfocusMax)
			ON_BN_CLICKED(IDC_CHECKTWK, OnBnClickedChecktwk)
		END_MESSAGE_MAP()
*/
		
		BOOL CInterpolateCurve::OnInitDialog()
		{
			CDialog::OnInitDialog();

			//set combo box items
			m_CurveType.AddString ("Linear");
			m_CurveType.AddString ("Hermite Curve");
			m_CurveType.AddString ("All to Linear");
			m_CurveType.AddString ("All to Hermite");
			m_CurveType.SetCurSel (1);
			//determine scaling factor
			GetClientRect(&grapharea);
			AdjustRectToView(grapharea);
			xscale = (grapharea.right - grapharea.left) / float(numLines-1);
			xoffset = (float(grapharea.right - grapharea.left) - (xscale*(numLines-1)))/2.0f;
			yscale = (grapharea.bottom - grapharea.top) / float(y_range); ///total height divided by max param value.
			grapharea.left+=xoffset;grapharea.right-=xoffset;

			SetPosText(0);
			SetValText(kf[0].value);
			SetMinText(min_val);
			SetMaxText(max_val);
			if (kftwk != -1)
			{
				m_checktwk.SetCheck(1);
				m_combotwk.EnableWindow(true);
				m_combotwk.SetCurSel(kftwk);
			}
			
			return true;			
		}
		void CInterpolateCurve::OnBnClickedChecktwk()
		{
			if ( m_checktwk.GetCheck() == 0)
			{
				m_combotwk.EnableWindow(false);
			}
			else m_combotwk.EnableWindow(true);
		}

		// Assign array of values to the curve dialog.
		void CInterpolateCurve::AssignInitialValues(int* values,int commandtype, int minval, int maxval)
		{
			kftwk = commandtype;
			min_val = minval;
			max_val = maxval;
			y_range = maxval-minval;
			for (int i(0); i< numLines; ++i)
			{
				if (values[i] != -1)
				{
					kf[i].value=values[i];
				}
				//This ensures the default values are within range
				if (kf[i].value != -1 && kf[i].value < minval) kf[i].value=minval;
				else if(kf[i].value>maxval)kf[i].value=maxval;
			}
		}

		void CInterpolateCurve::OnOk()
		{
			FillReturnValues();
			if ( m_checktwk.GetCheck() > 0) kftwk = m_combotwk.GetCurSel();
			else kftwk = -1;

			for (int i=0;i< numLines;i++) kfresult[i]=kf[i].value;
			CDialog::OnOK();		
		}

		//These two methods wouldn't be needed if this graphic was implemented with a custom control.
		//(See sampulse UI)
		void CInterpolateCurve::AdjustPointToView(CPoint&point)
		{
			CDPI& g_metrics = PsycleGlobal::dpiSetting();

			point.y-=g_metrics.ScaleY(20); point.x-=g_metrics.ScaleX(10)+xoffset;
		}
		void CInterpolateCurve::AdjustRectToView(RECT&rect)
		{
			CDPI& g_metrics = PsycleGlobal::dpiSetting();
			rect.top += g_metrics.ScaleY(20); rect.left += g_metrics.ScaleX(10); rect.right -= g_metrics.ScaleX(10); rect.bottom -= g_metrics.ScaleY(100);
		}
		int CInterpolateCurve::GetPointFromX(LONG x)
		{
			int pos = -1;

			//determine x position
			float remainder = ((x * 1000) % (int(xscale * 1000)))/1000;  //remainder to reasonable accuracy
			if (remainder < (3 + int(xscale / 2 + 0.5)))
			{
				pos = int(x / xscale); //round down to neareset position on left of point
			}
			///\todo: what's the meaning of this "else if"?
			else if (remainder > ( - int(xscale / 2 + 0.5) - 6))
			{
				pos = int(x / xscale)+1;// round up to nearest position on right of point
			}
			return pos;
		}

		RECT CInterpolateCurve::GetGPointRect(int i)
		{
			RECT pointrect;
			int x = grapharea.left + int(xscale * i);
			int y = grapharea.top + int(yscale * (max_val - kf[i].value));
			pointrect.left = x - 3; pointrect.right = x + 3;
			pointrect.top = y - 3;  pointrect.bottom = y + 3;
			return pointrect;
		}

		float CInterpolateCurve::HermiteCurveInterpolate(int kf0, int kf1, int kf2, int kf3, int curposition, int maxposition, float tangmult, bool interpolation)
		{
			if ( interpolation == true )
			{
				float s = (float)curposition/(float)maxposition;
				float pws3 = pow(s, 3);
				float pws2 = pow(s, 2);
				float pws23 = 3 * pws2;
				float h1 = (2 * pws3) - pws23 + 1;
				float h2 = (-2 * pws3) + pws23;
				float h3 = pws3 - (2 * pws2) + s;
				float h4 = pws3 - pws2;

				float t1 = tangmult * (kf2-kf0);
				float t2 = tangmult * (kf3-kf1);

				return (h1 * kf1 + h2 * kf2 + h3 * t1 + h4 * t2);
			}
			else
			{
				return kf1;
			}
		}
		void CInterpolateCurve::SetPosText(int i)
		{
			char val[5];
			sprintf (val, "%d", i+startIndex);
			m_Pos.SetWindowText (val);
		}
		void CInterpolateCurve::SetValText(int i)
		{
			char val[5];
			sprintf (val, "%04X", i);
			m_Value.SetWindowText (val);
		}
		void CInterpolateCurve::SetMinText(int i)
		{
			char val[5];
			sprintf (val, "%04X", i);
			m_Min.SetWindowText (val);
		}
		void CInterpolateCurve::SetMaxText(int i)
		{
			char val[5];
			sprintf (val, "%04X", i);
			m_Max.SetWindowText (val);
		}
		void CInterpolateCurve::OnPaint()
		{
			CPaintDC dc(this); // device context for painting
			CDPI& g_metrics = PsycleGlobal::dpiSetting();

			RECT tmprect = grapharea;
			//set an extra border.
			tmprect.left -= xoffset; tmprect.right += xoffset;
			tmprect.top -= 10; tmprect.bottom += 10;
			dc.FillSolidRect (&tmprect, 0x00FFFFFF);
			dc.Rectangle (&tmprect);
			HGDIOBJ hFont = GetStockObject( DEFAULT_GUI_FONT );
			HGDIOBJ oldFont = dc.SelectObject(hFont);

			//draw gridlines
			//horizontal
			///\todo: adapt to range.
			float range_16 = yscale * y_range /16.f;
			for (int j = 0; j < 17; j++)  //line every 0x1000
			{
				dc.FillSolidRect (grapharea.left, grapharea.top + int(j *range_16), grapharea.right - grapharea.left, 1, 0x00DDDDDD);
			}
			//vertical
			int fontheight = g_metrics.ScaleY(12);
			for (int h = 0; h < numLines;h++)
			{
				if ( (startIndex+h) % linesperbeat == 0)
				{
					CString bla; bla.Format("%d",startIndex+h);
					dc.TextOut (int(h * xscale) + grapharea.left,grapharea.bottom-fontheight, bla);
					dc.FillSolidRect (int(h * xscale) + grapharea.left,grapharea.top, 1,grapharea.bottom - grapharea.top, 0x00DD0000);
				}
				else
					dc.FillSolidRect (int(h * xscale) + grapharea.left,grapharea.top, 1,grapharea.bottom - grapharea.top, 0x00DDDDDD);
			}

			// Draw points and lines between points
			int pos0=0, pos1=0, pos2=0, pos3=0;

			GetNextkfvalue(pos1);
			if ( pos1 >= numLines ) return;

			int x = grapharea.left + int(xscale * pos1);
			int y = grapharea.top + int(yscale * (max_val - kf[pos1].value));

			dc.MoveTo (x, y);
			RECT pointrect = GetGPointRect(pos1);
			dc.FillSolidRect (&pointrect, 0);

			pos0=pos1;
			pos2=pos1;
			GetNextkfvalue(++pos2);
			if ( pos2 < numLines )
			{
				pos3=pos2;
				GetNextkfvalue(++pos3);
				while (pos3 < numLines)
				{
					switch (kf[pos1].curvetype)
					{
					case 0:
						x = grapharea.left + int(xscale * pos2);
						y = grapharea.top + int(yscale * (max_val - kf[pos2].value));
						dc.LineTo(x,y);
						break;
					case 1:
						int distance = grapharea.left + int(xscale * pos2) - x;
						for (int i=1; i < distance; i++)
						{
							int curveval=HermiteCurveInterpolate(kf[pos0].value,kf[pos1].value,kf[pos2].value,kf[pos3].value,i,distance, 0, true);
							dc.LineTo(x+i,grapharea.top + int(yscale * (max_val - curveval)));
						}
						x = grapharea.left + int(xscale * pos2);
						break;
					}
					pointrect = GetGPointRect(pos2);
					dc.FillSolidRect (&pointrect, 0);

					pos0=pos1;
					pos1=pos2;
					pos2=pos3;
					GetNextkfvalue(++pos3);
				}

				pos3=pos2;
				switch (kf[pos1].curvetype)
				{
				case 0:
					x = grapharea.left + int(xscale * pos2);
					y = grapharea.top + int(yscale * (max_val - kf[pos2].value));
					dc.LineTo(x,y);
					break;
				case 1:
					int distance = grapharea.left + int(xscale * pos2) - x;
					for (int i=1; i < distance; i++)
					{
						int curveval=HermiteCurveInterpolate(kf[pos0].value,kf[pos1].value,kf[pos2].value,kf[pos3].value,i,distance, 0, true);
						dc.LineTo(x+i,grapharea.top + int(yscale * (max_val - curveval)));
					}
					break;
				}
				pointrect = GetGPointRect(pos2);
				dc.FillSolidRect (&pointrect, 0);
			}

			//draw selected item
			if (selectedGPoint > -1)
			{
				RECT highlightrect = GetGPointRect(selectedGPoint);
				dc.FillSolidRect (&highlightrect, 0x000000FF);
			}
			dc.SelectObject(oldFont);
		}

		void CInterpolateCurve::OnLButtonDown(UINT nFlags, CPoint point)
		{
			AdjustPointToView(point);
			if ((point.x >= -3) && (point.x <= 3+(grapharea.right-grapharea.left)) && (point.y >= 0 ) && (point.y <= (grapharea.bottom-grapharea.top) ))
			{			
				int pos = GetPointFromX(point.x);
				if (pos >= 0)
				{	
                    kf[pos].value = max_val - int((point.y)/yscale);
					selectedGPoint = pos;
					
					SetPosText(pos);
					SetValText(kf[pos].value);

					switch (kf[pos].curvetype)
					{
					case 0: 
						m_CurveType.SetCurSel (0); break;
					case 1:
						m_CurveType.SetCurSel (1); break;
					}

					RECT temp;  temp.left = grapharea.left - 4; temp.right = grapharea.right + 4; 
					temp.bottom = grapharea.bottom + 4; temp.top = grapharea.top - 4;
					InvalidateRect(&temp, 0);

				}
			}
			bDragging=true;
			SetCapture();
			CDialog::OnLButtonDown(nFlags, point);
		}

		void CInterpolateCurve::OnLButtonUp(UINT nFlags, CPoint point)
		{
			AdjustPointToView(point);
			bDragging = false;
			ReleaseCapture();

			CDialog::OnLButtonUp(nFlags, point);
		}

		void CInterpolateCurve::OnMouseMove(UINT nFlags, CPoint point)
		{
			if (bDragging)
			{
				AdjustPointToView(point);
				if ((point.x >= -3) && (point.x <= 3+(grapharea.right-grapharea.left)) && (point.y >= 0 ) && (point.y <= (grapharea.bottom-grapharea.top) ))
				{			
					int pos = GetPointFromX(point.x);
					if (pos >= 0)
					{	
						if ( pos != selectedGPoint && selectedGPoint != 0 && selectedGPoint != numLines-1)
						{
							kf[pos].curvetype = kf[selectedGPoint].curvetype;
							kf[selectedGPoint].value = -1;
						}
						kf[pos].value = max_val - int((point.y)/yscale);
						selectedGPoint = pos;

						SetPosText(pos);
						SetValText(kf[pos].value);

						switch (kf[pos].curvetype)
						{
						case 0: 
							m_CurveType.SetCurSel (0); break;
						case 1:
							m_CurveType.SetCurSel (1); break;
						}

						RECT temp;  temp.left = grapharea.left - 4; temp.right = grapharea.right + 4; 
						temp.bottom = grapharea.bottom + 4; temp.top = grapharea.top - 4;
						InvalidateRect(&temp, 0);
					}
				}
			}
			CDialog::OnMouseMove(nFlags, point);
		}

		void CInterpolateCurve::OnRButtonDown(UINT nFlags, CPoint point)
		{
			AdjustPointToView(point);
			int pos = GetPointFromX(point.x);

			if (pos > 0 && pos < numLines-1)  // First and last point are not allowed to be removed.
				kf[pos].value = -1;
			else return;

			if (pos == selectedGPoint)
			{
				selectedGPoint = -1;
			}

			RECT temp;  temp.left = grapharea.left - 4; temp.right = grapharea.right + 4; 
			temp.bottom = grapharea.bottom + 4; temp.top = grapharea.top - 4;
			InvalidateRect(&temp, 0);
			CDialog::OnRButtonDown(nFlags, point);
		}	

		void CInterpolateCurve::OnSelendokCurveType()
		{
			if (selectedGPoint >=0)
			{
				switch (m_CurveType.GetCurSel ())
				{
				case 0:
					kf[selectedGPoint].curvetype = 0; break;
				case 1:
					kf[selectedGPoint].curvetype = 1; break;
				case 2:
					for (int i(0); i< numLines; i++)
					{
						kf[i].curvetype = 0;
					}
					m_CurveType.SetCurSel(0);
					break;
				case 3:
					for (int i(0); i< numLines; i++)
					{
						kf[i].curvetype = 1;
					}
					m_CurveType.SetCurSel(1);
					break;
				}
				Invalidate();
			}
		}

		void CInterpolateCurve::OnEnKillfocusPos()
		{
			CString text;
			m_Pos.GetWindowText(text);
			int pos=atoi(text)-startIndex;

			if ( pos >=0 && pos <numLines)
			{
				if ( kf[pos].value == -1)
				{
					int value=GetValValue();
					if ( value >= GetMinValue() && value <= GetMaxValue())
					{
						kf[pos].value = value;
					}
					else 
					{
						kf[pos].value= 0;
						SetValText(0);
					}
				}
				else
				{
					SetValText(kf[pos].value);
				}
				selectedGPoint = pos;
				RECT temp;  temp.left = grapharea.left - 4; temp.right = grapharea.right + 4; 
				temp.bottom = grapharea.bottom + 4; temp.top = grapharea.top - 4;
				InvalidateRect(&temp, 0);
			}
			else
			{
				SetPosText(selectedGPoint);
				SetValText(kf[selectedGPoint].value);
			}

		}

		void CInterpolateCurve::OnEnKillfocusVal()
		{
			if (selectedGPoint >=0)
			{
				int value=GetValValue();
				if ( value >= GetMinValue() && value <= GetMaxValue())
				{
					kf[selectedGPoint].value = value;
					RECT temp;  temp.left = grapharea.left - 4; temp.right = grapharea.right + 4; 
					temp.bottom = grapharea.bottom + 4; temp.top = grapharea.top - 4;
					InvalidateRect(&temp, 0);
				}
				else
				{
					SetValText(kf[selectedGPoint].value);
				}
			}
		}


		void CInterpolateCurve::OnEnKillfocusMin()
		{
			int min = GetMinValue();
			int max = GetMaxValue();
			if (min < 0 || min > max) min=0;
			y_range = max-min;
			min_val = min;
			//todo: guarantee not out of range values in kf
			yscale = (grapharea.bottom - grapharea.top) / float(y_range); ///total height divided by max param value.
			RECT temp;  temp.left = grapharea.left - 4; temp.right = grapharea.right + 4; 
			temp.bottom = grapharea.bottom + 4; temp.top = grapharea.top - 4;
			InvalidateRect(&temp, 0);
		}
		void CInterpolateCurve::OnEnKillfocusMax()
		{
			int min = GetMinValue();
			int max = GetMaxValue();
			if (max < min || max > 0xFFFF) max=0xFFFF;
			y_range = max-min;
			max_val = max;
			yscale = (grapharea.bottom - grapharea.top) / float(y_range); ///total height divided by max param value.
			//todo: guarantee not out of range values in kf
			RECT temp;  temp.left = grapharea.left - 4; temp.right = grapharea.right + 4; 
			temp.bottom = grapharea.bottom + 4; temp.top = grapharea.top - 4;
			InvalidateRect(&temp, 0);
		}

		void CInterpolateCurve::GetNextkfvalue(int &startpos)
		{
			for (; startpos <= numLines; startpos++)
			{
				if (kf[startpos].value >= 0)
				{
					return;
				}
			}
		}

		void CInterpolateCurve::FillReturnValues()
		{
			int pos0=0, pos1=0, pos2=0, pos3=0;

			GetNextkfvalue(pos1);
			if ( pos1 >= numLines ) return;

			pos0=pos1;
			pos2=pos1;
			GetNextkfvalue(++pos2);
			if ( pos2 >= numLines ) return;

			pos3=pos2;
			GetNextkfvalue(++pos3);
			while (pos3 < numLines)
			{
				switch (kf[pos1].curvetype)
				{
				case 0:
					for (int i=1; i < (pos2-pos1); i++)
					{
						kf[pos1+i].value=kf[pos1].value + (((kf[pos2].value-kf[pos1].value)*i)/(pos2-pos1));
					}
					break;
				case 1:
					for (int i=1; i < (pos2-pos1); i++)
					{
						kf[pos1+i].value=HermiteCurveInterpolate(kf[pos0].value,kf[pos1].value,kf[pos2].value,kf[pos3].value,i,pos2-pos1, 0, true);
					}
					break;
				}

				pos0=pos1;
				pos1=pos2;
				pos2=pos3;
				GetNextkfvalue(++pos3);
			}
			
			pos3=pos2;
			switch (kf[pos1].curvetype)
			{
			case 0:
				for (int i=1; i < (pos2-pos1); i++)
				{
					kf[pos1+i].value=kf[pos1].value + (((kf[pos2].value-kf[pos1].value)*i)/(pos2-pos1));
				}
				break;
			case 1:
				for (int i=1; i < (pos2-pos1); i++)
				{
					kf[pos1+i].value=HermiteCurveInterpolate(kf[pos0].value,kf[pos1].value,kf[pos2].value,kf[pos3].value,i,pos2-pos1, 0, true);
				}
				break;
			}
		}
		int CInterpolateCurve::GetPosValue()
		{
			CString text;
			m_Pos.GetWindowText(text);
			int pos=atoi(text)-startIndex;
			return pos;
		}
		int CInterpolateCurve::GetValValue()
		{
			CString text;
			m_Value.GetWindowText(text);
			std::string text2 = text;
			int value=0;
			helpers::hexstring_to_integer(text2,value);
			return value;
		}

		int CInterpolateCurve::GetMinValue()
		{
			CString text;
			m_Min.GetWindowText(text);
			std::string text2 = text;
			int value=0;
			helpers::hexstring_to_integer(text2,value);
			return value;
		}
		int CInterpolateCurve::GetMaxValue()
		{
			CString text;
			m_Max.GetWindowText(text);
			std::string text2 = text;
			int value=0;
			helpers::hexstring_to_integer(text2,value);
			return value;
		}

}   // namespace
}   // namespace
