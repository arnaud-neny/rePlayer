#include <psycle/host/detail/project.private.hpp>
#include "EnvelopeEditor.hpp"
#include <psycle/host/Player.hpp>
#include <psycle/host/Song.hpp>
namespace psycle { namespace host {


//////////////////////////////////////////////////////////////////////////////
// CEnvelopeEditor -----------------------------------------------------------
//////////////////////////////////////////////////////////////////////////////
CEnvelopeEditor::CEnvelopeEditor()
: m_pEnvelope(NULL)
, m_bInitialized(false)
, m_bnegative(false)
, m_bCanSustain(true)
, m_bCanLoop(true)
, m_bPointEditing(false)
, m_envelopeIdx(0)
, m_EditPoint(0)
, m_millisPerPoint(0)
, m_ticksPerBeat(24)
{
	_line_pen.CreatePen(PS_SOLID,0,RGB(0,0,255));
	_gridpen.CreatePen(PS_SOLID,0,RGB(192,192,255));
	_gridpen1.CreatePen(PS_SOLID,0,RGB(255,192,192));
	_point_brush.CreateSolidBrush(RGB(0,0,255));
}
CEnvelopeEditor::~CEnvelopeEditor(){
	_line_pen.DeleteObject();
	_gridpen.DeleteObject();
	_gridpen1.DeleteObject();
	_point_brush.DeleteObject();
}
			
/*
BEGIN_MESSAGE_MAP(CEnvelopeEditor, CStatic)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID__ENV_ADDNEWPOINT, OnPopAddPoint)
	ON_COMMAND(ID__ENV_SETSUSTAINBEGIN, OnPopSustainStart)
	ON_COMMAND(ID__ENV_SETSUSTAINEND, OnPopSustainEnd)
	ON_COMMAND(ID__ENV_SETLOOPSTART, OnPopLoopStart)
	ON_COMMAND(ID__ENV_SETLOOPEND, OnPopLoopEnd)
	ON_COMMAND(ID__ENV_REMOVEPOINT, OnPopRemovePoint)
	ON_COMMAND(ID__ENV_REMOVESUSTAIN, OnPopRemoveSustain)
	ON_COMMAND(ID__ENV_REMOVELOOP, OnPopRemoveLoop)
	ON_COMMAND(ID__ENV_REMOVEENVELOPE, OnPopRemoveEnvelope)
END_MESSAGE_MAP()
*/
			
			
void CEnvelopeEditor::Initialize(XMInstrument::Envelope & pEnvelope, int tpb, int millis)
{
	m_pEnvelope = &pEnvelope;
			
	CRect _rect;
	GetClientRect(&_rect);
	m_WindowHeight = _rect.Height();
	m_WindowWidth = _rect.Width();
			
	m_EditPoint = m_pEnvelope->NumOfPoints();
	if (pEnvelope.Mode() == XMInstrument::Envelope::Mode::MILIS) {
		m_millisPerPoint=millis; m_ticksPerBeat=0; 
	}
	else {
		m_millisPerPoint=0; m_ticksPerBeat=tpb; 
	}
	FitZoom();

	m_bInitialized = true;
	Invalidate();
}
void CEnvelopeEditor::ConvertToADSR(bool allowgain) {
	XMInstrument::Envelope &env = envelope();
	env.SetAdsr(true, allowgain);
	m_EditPoint = env.NumOfPoints();
	FitZoom();
}
void CEnvelopeEditor::TimeFormulaTicks(int ticksperbeat) {
	if (m_millisPerPoint > 0 ) {
		m_pEnvelope->Mode(XMInstrument::Envelope::Mode::TICK, Global::player().bpm, ticksperbeat, m_millisPerPoint);
	}
	m_millisPerPoint=0; m_ticksPerBeat=ticksperbeat; 
	FitZoom();
}
void CEnvelopeEditor::TimeFormulaMillis(int millisperpoint) { 
	if (m_ticksPerBeat>0) {
		m_pEnvelope->Mode(XMInstrument::Envelope::Mode::MILIS, Global::player().bpm, m_ticksPerBeat,millisperpoint);
	}
	m_ticksPerBeat=0; m_millisPerPoint=millisperpoint;
	FitZoom();
}

void CEnvelopeEditor::FitZoom()
{
	if (m_ticksPerBeat>0) {
		m_Zoom = 8.0f;
	}
	else {
		m_Zoom = 0.4f*m_millisPerPoint;
	}
	const int _points =  m_pEnvelope->NumOfPoints();

	if (_points > 0 )
	{
		while (m_Zoom * m_pEnvelope->GetTime(_points-1) > m_WindowWidth)
		{
			m_Zoom= m_Zoom/2.0f;
		}
	}
	//could do SetCursorPoint() if wanting to zoom while moving.
}
void CEnvelopeEditor::AttackTime(int time, bool rezoom)
{
	if (time <= 0) time=1; //prevent point swapping.
	int prevAttack = m_pEnvelope->GetTime(1);
	int diff = time-prevAttack;
	if (time > prevAttack) {
		//The order matters to prevent the algorithm to change the point indexes
		m_pEnvelope->SetTime(3, m_pEnvelope->GetTime(3)+diff);
		m_pEnvelope->SetTime(2, m_pEnvelope->GetTime(2)+diff);
		m_pEnvelope->SetTime(1, m_pEnvelope->GetTime(1)+diff);
	}
	else {
		//The order matters to prevent the algorithm to change the point indexes
		m_pEnvelope->SetTime(1, m_pEnvelope->GetTime(1)+diff);
		m_pEnvelope->SetTime(2, m_pEnvelope->GetTime(2)+diff);
		m_pEnvelope->SetTime(3, m_pEnvelope->GetTime(3)+diff);
	}
	if (rezoom)	FitZoom();
}
void CEnvelopeEditor::DecayTime(int time, bool rezoom)
{
	int timestart = m_pEnvelope->GetTime(1);
	if (time-timestart <= 0) time=timestart+1; //prevent point swapping.
	int prevDecay = m_pEnvelope->GetTime(2);
	int diff = time-prevDecay;
	if (time > prevDecay) {
		//The order matters to prevent the algorithm to change the point indexes
		m_pEnvelope->SetTime(3, m_pEnvelope->GetTime(3)+diff);
		m_pEnvelope->SetTime(2, m_pEnvelope->GetTime(2)+diff);
	}
	else {
		//The order matters to prevent the algorithm to change the point indexes
		m_pEnvelope->SetTime(2, m_pEnvelope->GetTime(2)+diff);
		m_pEnvelope->SetTime(3, m_pEnvelope->GetTime(3)+diff);
	}
	if (rezoom)	FitZoom();
}
void CEnvelopeEditor::ReleaseTime(int time, bool rezoom)
{
	int timestart = m_pEnvelope->GetTime(2);
	if (time-timestart <= 0) time=timestart+1; //prevent point swapping.
	m_pEnvelope->SetTime(3, time);
	if (rezoom)	FitZoom();
}



void CEnvelopeEditor::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
{
	if(m_bInitialized){
		if (m_pEnvelope && lpDrawItemStruct->itemAction == ODA_DRAWENTIRE)
		{
			CDC dc;
			dc.Attach(lpDrawItemStruct->hDC);
			CPen *oldpen= dc.SelectObject(&_gridpen);
			
			CRect _rect;
			GetClientRect(&_rect);

			dc.FillSolidRect(&_rect,RGB(255,255,255));
			dc.SetBkMode(TRANSPARENT);
			
			// ***** Background lines *****
			float _stepy = ((float)(m_WindowHeight)) / 100.0f * 10.0f;
			int pos=0;
			for(float i = 0; i <= (float)m_WindowHeight; i += _stepy, pos++)
			{
				if (pos==5 && m_bnegative) {
					dc.SelectObject(&_gridpen1);
					dc.MoveTo(0,i);
					dc.LineTo(m_WindowWidth,i);
					dc.SelectObject(&_gridpen);
				}
				else {
					dc.MoveTo(0,i);
					dc.LineTo(m_WindowWidth,i);
				}
			}
			
			const int _points =  m_pEnvelope->NumOfPoints();

			int _mod = 0.0f;
			float tenthsec;
			if (m_ticksPerBeat > 0 ) {
				tenthsec = m_Zoom*(m_ticksPerBeat/600.0)*Global::player().bpm;
			}
			else {
				tenthsec = m_Zoom*100/m_millisPerPoint;
			}
			int _sec = 0;

			for(float i = 0; i < m_WindowWidth; i+=tenthsec)
			{
				if (_mod == 5 )
				{
					dc.MoveTo(i,0);
					dc.LineTo(i,m_WindowHeight);
				} else if(_mod == 10 ) {
					_mod = 0;
					dc.SelectObject(&_gridpen1);
					dc.MoveTo(i,0);
					dc.LineTo(i,m_WindowHeight);
					dc.SelectObject(&_gridpen);
					// *****  *****
					_sec += 1;
					CString string;
					string.Format("%ds",_sec);
					dc.TextOut(i,m_WindowHeight-20,string);
				} else {
					dc.MoveTo(i,m_WindowHeight-5);
					dc.LineTo(i,m_WindowHeight);
				}
				_mod++;
			}
			
			// Sustain Point *****  *****

			if(m_pEnvelope->SustainBegin() != XMInstrument::Envelope::INVALID)
			{
				const int _pt_start_x = (m_Zoom * (float)m_pEnvelope->GetTime(m_pEnvelope->SustainBegin()));
				const int _pt_end_x = (m_Zoom * (float)m_pEnvelope->GetTime(m_pEnvelope->SustainEnd()));

				CPen _edit_line_pen(PS_DOT,0,RGB(64,192,128));

				dc.SelectObject(&_edit_line_pen);
				dc.MoveTo(_pt_start_x,0);
				dc.LineTo(_pt_start_x,m_WindowHeight);
				dc.MoveTo(_pt_end_x,0);
				dc.LineTo(_pt_end_x,m_WindowHeight);
			}
			
			// Loop Start *****  ***** Loop End *****
			
			if(m_pEnvelope->LoopStart() != XMInstrument::Envelope::INVALID && 
				m_pEnvelope->LoopEnd() != XMInstrument::Envelope::INVALID)
			{
				const int _pt_loop_start_x = m_Zoom * (float)m_pEnvelope->GetTime(m_pEnvelope->LoopStart());
				const int _pt_loop_end_x = m_Zoom * (float)m_pEnvelope->GetTime(m_pEnvelope->LoopEnd());
				
				// Envelope Point *****  ***** Sustain Label *****  *****
				CPen _loop_pen(PS_SOLID,0,RGB(64,192,128));

				/*				This would be to show the loop range, but without alpha blending, this is not nice.
				CBrush  _loop_brush;
				_loop_brush.CreateSolidBrush(RGB(64, 0, 128));
				CRect rect(_pt_loop_start_x,0,_pt_loop_end_x - _pt_loop_start_x,m_WindowHeight);
				dc.FillRect(&rect,&_loop_brush);
			
				dc.TextOut(((_pt_loop_end_x - _pt_loop_start_x) / 2 + _pt_loop_start_x - 20),(m_WindowHeight / 2),"Loop");
				_loop_brush.DeleteObject();
				*/
				dc.SelectObject(&_loop_pen);
				dc.MoveTo(_pt_loop_start_x,0);
				dc.LineTo(_pt_loop_start_x,m_WindowHeight);
				dc.MoveTo(_pt_loop_end_x,0);
				dc.LineTo(_pt_loop_end_x,m_WindowHeight);

			}


			// ***** Draw Envelope line and points *****
			CPoint _pt_start;
			if ( _points > 0 ) 
			{
				_pt_start.x=0;
				_pt_start.y = (m_bnegative) 
					? (int)((float)m_WindowHeight * (1.0f - (1.0f+m_pEnvelope->GetValue(0))*0.5f))
					: (int)((float)m_WindowHeight * (1.0f - m_pEnvelope->GetValue(0)));
			}
			for(int i = 1;i < _points ;i++)
			{
				CPoint _pt_end;
				_pt_end.x = (int)(m_Zoom * (float)m_pEnvelope->GetTime(i)); 
				_pt_end.y = (m_bnegative) 
					? (int)((float)m_WindowHeight * (1.0f - (1.0f+m_pEnvelope->GetValue(i))*0.5f))
					: (int)((float)m_WindowHeight * (1.0f - m_pEnvelope->GetValue(i)));
				dc.SelectObject(&_line_pen);
				dc.MoveTo(_pt_start);
				dc.LineTo(_pt_end);
				_pt_start = _pt_end;
			}

			for(unsigned int i = 0;i < _points ;i++)
			{
				CPoint _pt;
				_pt.x = (int)(m_Zoom * (float)m_pEnvelope->GetTime(i));
				_pt.y = (m_bnegative) 
					? (int)((float)m_WindowHeight * (1.0f - (1.0f+m_pEnvelope->GetValue(i))*0.5f))
					: (int)((float)m_WindowHeight * (1.0f - m_pEnvelope->GetValue(i)));
				CRect rect(_pt.x - (POINT_SIZE / 2),_pt.y - (POINT_SIZE / 2),_pt.x + (POINT_SIZE / 2),_pt.y + (POINT_SIZE / 2));
				if ( m_EditPoint == i )
				{
					CBrush _edit_brush;
					_edit_brush.CreateSolidBrush(RGB(0,192,192));
					dc.FillRect(&rect,&_edit_brush);
					_edit_brush.DeleteObject();
				}
				else dc.FillRect(&rect,&_point_brush);
			}

	
			dc.SelectObject(oldpen);
			dc.Detach();
		}
	}
}

void CEnvelopeEditor::OnLButtonDown( UINT nFlags, CPoint point )
{
	SetFocus();
			
	if(!m_bPointEditing){
		const int _points =  m_pEnvelope->NumOfPoints();

		int _edit_point = GetEnvelopePointIndexAtPoint(point.x,point.y);
		bool freeform = !m_pEnvelope->IsAdsr();
		if(_edit_point != _points && (freeform || _edit_point > 0)) {
			m_bCanMoveUpDown=(freeform || _edit_point == 2);
			m_bPointEditing = true;
			SetCapture();
			m_EditPoint = _edit_point;
		}
		else {
			m_EditPoint = _points;
			Invalidate();
		}
	}
}
void CEnvelopeEditor::OnLButtonUp( UINT nFlags, CPoint point )
{
	if(m_bPointEditing){
		ReleaseCapture();
		m_bPointEditing =  false;
				
		if (point.x > m_WindowWidth ) m_Zoom = m_Zoom /2.0f;
		else if ( m_pEnvelope->GetTime(m_pEnvelope->NumOfPoints()-1)*m_Zoom < m_WindowWidth/2 && m_Zoom < 8.0f) m_Zoom = m_Zoom *2.0f;

		this->GetParent()->SendMessage(PSYC_ENVELOPE_CHANGED, m_envelopeIdx);
		Invalidate();
	}
}
void CEnvelopeEditor::OnMouseMove( UINT nFlags, CPoint point )
{
	if(m_bPointEditing)
	{
		if(point.y > m_WindowHeight)
		{
			point.y = m_WindowHeight;
		}

		if(point.y < 0)
		{
			point.y = 0;
		}

		if( point.x < 0)
		{
			point.x = 0;
		}
		if ( point.x > m_WindowWidth)
		{
		//what to do? unzoom... but what about the mouse?
		}
		if ( m_EditPoint == 0 )
		{
			point.x=0;
		}
		m_EditPointX = point.x;
		if (m_bCanMoveUpDown) { m_EditPointY = point.y; }
		else {
			if (m_bnegative) {
				m_EditPointY = (1 - m_pEnvelope->GetValue(m_EditPoint))*(float)m_WindowHeight/2.0f;
			}
			else {
				m_EditPointY = (1 - m_pEnvelope->GetValue(m_EditPoint))*(float)m_WindowHeight;
			}
		}
		if (m_pEnvelope->IsAdsr()) {
			float val;
			if (m_bnegative) {
				val = (1.0f - 2.0f*m_EditPointY / (float)m_WindowHeight);
			}
			else {
				val = (1.0f - (float)m_EditPointY / (float)m_WindowHeight);
			}
			switch(m_EditPoint) {
				case 1: AttackTime((int)((float)m_EditPointX / m_Zoom), false); break;
				case 2: m_pEnvelope->SetValue(2, std::max(m_pEnvelope->GetValue(0),std::min(val,m_pEnvelope->GetValue(1))));
					DecayTime((int)((float)m_EditPointX / m_Zoom), false);
					break;
				case 3: ReleaseTime((int)((float)m_EditPointX / m_Zoom), false); break;
				default:break;
			}
		}
		else {
			if (m_bnegative) {
				m_EditPoint = m_pEnvelope->SetTimeAndValue(m_EditPoint,(int)((float)m_EditPointX / m_Zoom),
					(1.0f - 2.0f*m_EditPointY / (float)m_WindowHeight));
			}
			else {
				m_EditPoint = m_pEnvelope->SetTimeAndValue(m_EditPoint,(int)((float)m_EditPointX / m_Zoom),
					(1.0f - (float)m_EditPointY / (float)m_WindowHeight));
			}
		}
		Invalidate();
	}
}
			
void CEnvelopeEditor::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	if (m_pEnvelope->IsAdsr()) {
		return;
	}
	CPoint tmp;
	tmp = point;
	ScreenToClient(&tmp);

	CMenu menu;
	VERIFY(menu.LoadMenu(IDR_MENU_ENV_EDIT));
	int _edit_point = GetEnvelopePointIndexAtPoint(tmp.x,tmp.y);
	m_EditPointX = tmp.x;
	m_EditPointY = tmp.y;
	m_EditPoint = _edit_point;

	CMenu* pPopup = menu.GetSubMenu(0);
	assert(pPopup);

	// The ON_UPDATE_COMMAND_UI message handlers only work with CFrameWnd's, and CStatic is a CWnd.
	// This is documented to be the case, by Microsoft.
	if ( m_EditPoint == m_pEnvelope->NumOfPoints() || m_pEnvelope->SustainBegin() == m_EditPoint
		|| !m_bCanSustain) menu.EnableMenuItem(ID__ENV_SETSUSTAINBEGIN,MF_GRAYED);

	if ( m_EditPoint == m_pEnvelope->NumOfPoints() || m_pEnvelope->SustainEnd() == m_EditPoint
		|| !m_bCanSustain) menu.EnableMenuItem(ID__ENV_SETSUSTAINEND,MF_GRAYED);
			
	if ( m_EditPoint == m_pEnvelope->NumOfPoints()  || m_pEnvelope->LoopStart() == m_EditPoint
		|| !m_bCanLoop) menu.EnableMenuItem(ID__ENV_SETLOOPSTART,MF_GRAYED);

	if ( m_EditPoint == m_pEnvelope->NumOfPoints() || m_pEnvelope->LoopEnd() == m_EditPoint
		|| !m_bCanLoop) menu.EnableMenuItem(ID__ENV_SETLOOPEND,MF_GRAYED);
	        
	if (m_EditPoint == m_pEnvelope->NumOfPoints()) menu.EnableMenuItem(ID__ENV_REMOVEPOINT,MF_GRAYED);
	        
	if(m_pEnvelope->SustainBegin() == XMInstrument::Envelope::INVALID) menu.EnableMenuItem(ID__ENV_REMOVESUSTAIN,MF_GRAYED);

	if(m_pEnvelope->LoopStart() == XMInstrument::Envelope::INVALID) menu.EnableMenuItem(ID__ENV_REMOVELOOP,MF_GRAYED);

	if ( m_pEnvelope->NumOfPoints() == 0 ) menu.EnableMenuItem(ID__ENV_REMOVEENVELOPE,MF_GRAYED);
	//

	pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
	menu.DestroyMenu();

	CWnd::OnContextMenu(pWnd,point);
}

			
void CEnvelopeEditor::OnPopAddPoint()
{
		
	int _new_point = (int)((float)m_EditPointX / m_Zoom);
	float _new_value = (1.0f - (float)m_EditPointY / (float)m_WindowHeight);

		
	if(_new_value > 1.0f)
	{
		_new_value = 1.0f;
	}

	if(_new_value < 0.0f)
	{
		_new_value = 0.0f;
	}

	if( _new_point < 0)
	{
		_new_point = 0;
	}

	CExclusiveLock lock(&Global::song().semaphore, 2, true);
	//\todo : Verify that we aren't trying to add an existing point!!!

	if ( m_pEnvelope->NumOfPoints() == 0 && _new_point != 0 ) m_EditPoint = m_pEnvelope->Insert(0,1.0f);
	m_EditPoint = m_pEnvelope->Insert(_new_point,_new_value);
	this->GetParent()->SendMessage(PSYC_ENVELOPE_CHANGED, m_envelopeIdx);
	Invalidate();
}
void CEnvelopeEditor::OnPopSustainStart()
{
	if ( m_EditPoint != m_pEnvelope->NumOfPoints())
	{
		CExclusiveLock lock(&Global::song().semaphore, 2, true);
		m_pEnvelope->SustainBegin(m_EditPoint);
		if (m_pEnvelope->SustainEnd()== XMInstrument::Envelope::INVALID ) m_pEnvelope->SustainEnd(m_EditPoint);
		else if (m_pEnvelope->SustainEnd() < m_EditPoint )m_pEnvelope->SustainEnd(m_EditPoint);
		this->GetParent()->SendMessage(PSYC_ENVELOPE_CHANGED, m_envelopeIdx);
		Invalidate();
	}
}
void CEnvelopeEditor::OnPopSustainEnd()
{
	if ( m_EditPoint != m_pEnvelope->NumOfPoints())
	{
		CExclusiveLock lock(&Global::song().semaphore, 2, true);
		if (m_pEnvelope->SustainBegin()== XMInstrument::Envelope::INVALID ) m_pEnvelope->SustainBegin(m_EditPoint);
		else if (m_pEnvelope->SustainBegin() > m_EditPoint )m_pEnvelope->SustainBegin(m_EditPoint);
		m_pEnvelope->SustainEnd(m_EditPoint);
		this->GetParent()->SendMessage(PSYC_ENVELOPE_CHANGED, m_envelopeIdx);
		Invalidate();
	}
}
void CEnvelopeEditor::OnPopLoopStart()
{
	if ( m_EditPoint != m_pEnvelope->NumOfPoints())
	{
		CExclusiveLock lock(&Global::song().semaphore, 2, true);
		m_pEnvelope->LoopStart(m_EditPoint);
		if (m_pEnvelope->LoopEnd()== XMInstrument::Envelope::INVALID ) m_pEnvelope->LoopEnd(m_EditPoint);
		else if (m_pEnvelope->LoopEnd() < m_EditPoint )m_pEnvelope->LoopEnd(m_EditPoint);
		this->GetParent()->SendMessage(PSYC_ENVELOPE_CHANGED, m_envelopeIdx);
		Invalidate();
	}
}
void CEnvelopeEditor::OnPopLoopEnd()
{
	if ( m_EditPoint != m_pEnvelope->NumOfPoints())
	{
		CExclusiveLock lock(&Global::song().semaphore, 2, true);
		if (m_pEnvelope->LoopStart()== XMInstrument::Envelope::INVALID ) m_pEnvelope->LoopStart(m_EditPoint);
		else if (m_pEnvelope->LoopStart() > m_EditPoint )m_pEnvelope->LoopStart(m_EditPoint);
		m_pEnvelope->LoopEnd(m_EditPoint);
		this->GetParent()->SendMessage(PSYC_ENVELOPE_CHANGED, m_envelopeIdx);
		Invalidate();
	}
}
void CEnvelopeEditor::OnPopRemovePoint()
{
	const int _points =  m_pEnvelope->NumOfPoints();
	if(_points == 0)
	{
		return;
	}

	CRect _rect;
	GetClientRect(&_rect);


	if(m_EditPoint != _points)
	{
		CExclusiveLock lock(&Global::song().semaphore, 2, true);
		m_pEnvelope->Delete(m_EditPoint);
		m_EditPoint = _points;
		Invalidate();
		this->GetParent()->SendMessage(PSYC_ENVELOPE_CHANGED, m_envelopeIdx);
	}
}
void CEnvelopeEditor::OnPopRemoveSustain()
{ 
	CExclusiveLock lock(&Global::song().semaphore, 2, true);
	m_pEnvelope->SustainBegin(XMInstrument::Envelope::INVALID);
	m_pEnvelope->SustainEnd(XMInstrument::Envelope::INVALID);
	this->GetParent()->SendMessage(PSYC_ENVELOPE_CHANGED, m_envelopeIdx);
	Invalidate();
}
void CEnvelopeEditor::OnPopRemoveLoop()
{ 
	CExclusiveLock lock(&Global::song().semaphore, 2, true);
	m_pEnvelope->LoopStart(XMInstrument::Envelope::INVALID);
	m_pEnvelope->LoopEnd(XMInstrument::Envelope::INVALID);
	this->GetParent()->SendMessage(PSYC_ENVELOPE_CHANGED, m_envelopeIdx);
	Invalidate();
}
void CEnvelopeEditor::OnPopRemoveEnvelope()
{
	CExclusiveLock lock(&Global::song().semaphore, 2, true);
	m_pEnvelope->Clear();
	this->GetParent()->SendMessage(PSYC_ENVELOPE_CHANGED, m_envelopeIdx);
	Invalidate();
}

}}