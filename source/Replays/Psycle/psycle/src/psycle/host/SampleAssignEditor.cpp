#include <psycle/host/detail/project.private.hpp>
#include "SampleAssignEditor.hpp"
#include "PsycleConfig.hpp"
#include "InstrIndividualMap.hpp"
#include "InstrumentGenDlg.hpp"
#include <psycle/host/XMInstrument.hpp>

namespace psycle { namespace host {

const int CSampleAssignEditor::c_NaturalKeysPerOctave = 7;
const int CSampleAssignEditor::c_SharpKeysPerOctave = 5;
const int CSampleAssignEditor::c_KeysPerOctave = 12;

const int CSampleAssignEditor::c_SharpKey_Xpos[]= {15,44,92,121,150};
const CSampleAssignEditor::TNoteKey CSampleAssignEditor::c_NoteAssign[]=
	{CSampleAssignEditor::NaturalKey,CSampleAssignEditor::SharpKey,CSampleAssignEditor::NaturalKey,CSampleAssignEditor::SharpKey,CSampleAssignEditor::NaturalKey,
	CSampleAssignEditor::NaturalKey,CSampleAssignEditor::SharpKey,CSampleAssignEditor::NaturalKey,CSampleAssignEditor::SharpKey,CSampleAssignEditor::NaturalKey,CSampleAssignEditor::SharpKey,CSampleAssignEditor::NaturalKey};
const int CSampleAssignEditor::c_noteAssignindex[] = {0,0,1,1,2,3,2,4,3,5,4,6};

const CString CSampleAssignEditor::c_Key_name[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const CString CSampleAssignEditor::c_NaturalKey_name[] = {"C","D","E","F","G","A","B"};
const int CSampleAssignEditor::c_NaturalKey_index[] = {0,2,4,5,7,9,11};
const int CSampleAssignEditor::c_SharpKey_index[] = {1,3,6,8,10};

//////////////////////////////////////////////////////////////////////////////
// SampleAssignEditor ------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////////


/*
BEGIN_MESSAGE_MAP(CSampleAssignEditor, CStatic)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()
	ON_WM_HSCROLL()
END_MESSAGE_MAP()
*/
/*
	ON_WM_CONTEXTMENU()
*/


CSampleAssignEditor::CSampleAssignEditor()
: m_bInitialized(false)
, m_tracking(false)
, m_Octave(4)
, bmpDC(NULL)
{
	m_NaturalKey.LoadBitmap(IDB_KEYS_NORMAL);
	m_SharpKey.LoadBitmap(IDB_KEYS_SHARP);
	m_BackKey.LoadBitmap(IDB_KEYS_BACK);

	BITMAP _bmp, _bmp2;
	m_NaturalKey.GetBitmap(&_bmp);
	m_SharpKey.GetBitmap(&_bmp2);
	m_naturalkey_width = _bmp.bmWidth;
	m_naturalkey_height = _bmp.bmHeight;
	m_sharpkey_width = _bmp2.bmWidth;
	m_sharpkey_height = _bmp2.bmHeight;

	m_octave_width = m_naturalkey_width * c_NaturalKeysPerOctave;
}
CSampleAssignEditor::~CSampleAssignEditor()
{
	m_NaturalKey.DeleteObject();
	m_SharpKey.DeleteObject();
	m_BackKey.DeleteObject();
	if ( bmpDC != NULL )
	{
		bmpDC->DeleteObject();
		delete bmpDC; bmpDC = 0;
	}
}

void CSampleAssignEditor::Initialize(XMInstrument & pInstrument)
{
	m_pInst = &pInstrument;
	m_bInitialized=true;
	Invalidate();
}
	
void CSampleAssignEditor::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
{
	if(m_bInitialized){
		if (lpDrawItemStruct->itemAction == ODA_DRAWENTIRE)
		{
			const PsycleConfig::PatternView & patView = PsycleGlobal::conf().patView();
			char **notetable = (patView.showA440) ? patView.notes_tab_a440 : patView.notes_tab_a220;
			CDC dc, memDC, keyDC;
			CRect _rect;
			GetClientRect(&_rect);
			dc.Attach(lpDrawItemStruct->hDC);
			memDC.CreateCompatibleDC(&dc);
			CFont* font = dc.GetCurrentFont();
			CFont* oldmemfont = memDC.SelectObject(font);
			keyDC.CreateCompatibleDC(&dc);
			if ( bmpDC == NULL ) // buffer creation
			{
				bmpDC = new CBitmap();
				bmpDC->CreateCompatibleBitmap(&dc,_rect.Width(),_rect.Height());
			}

			//Draw top header image.
			CBitmap* oldMemBmp = memDC.SelectObject(bmpDC);
			CBitmap* oldKeyBmp = keyDC.SelectObject(&m_BackKey);
			memDC.FillSolidRect(&_rect,RGB(0,0,0));
			memDC.SetBkMode(TRANSPARENT);
			memDC.SetTextColor(RGB(0xCC,0xCC,0xCC));
			const int head_width = 26;
			const int head_height = 20;
			int text_xOffset = 5;
			int text_yOffset = 4;
			CString _tmp_str;
			int _index = 0,_octave = m_Octave;
			for(int i = 0;i < _rect.Width() && _octave<10;i+=head_width)
			{
				_tmp_str.Format("%s%d",c_NaturalKey_name[_index],patView.showA440?_octave-1:_octave);
				memDC.BitBlt(i,0, 	head_width,head_height, 	&keyDC, 0,0,	SRCCOPY);
				if (m_FocusKeyRect.left>=i && m_FocusKeyRect.left<i+head_width && m_FocusKeyRect.left!=m_FocusKeyRect.right){
					memDC.SetTextColor(RGB(0x55,0xAA,0xFF));
					memDC.TextOut(i+text_xOffset,text_yOffset,_tmp_str);
					memDC.SetTextColor(RGB(0xCC,0xCC,0xCC));
				}
				else { memDC.TextOut(i+text_xOffset,text_yOffset,_tmp_str); }
				
				_index++;
				if(_index == c_NaturalKeysPerOctave){
					_index = 0;
					_octave++;
				}
			}

			//Draw natural keys
			keyDC.SelectObject(&m_NaturalKey);
			memDC.SetBkMode(TRANSPARENT);
			//memDC.SetTextColor(RGB(160,128,48));
			memDC.SetTextColor(RGB(0x00,0x6C,0xD8));
			memDC.SetBkColor(RGB(255,255,255));
			text_xOffset=3;
			text_yOffset=head_height+m_sharpkey_height;
			_index = 0,_octave = m_Octave;
			for(int i = 0; i < _rect.Width() && _octave<10;i+=m_naturalkey_width )
			{
				XMInstrument::NotePair noteToSample = m_pInst->NoteToSample(_octave*c_KeysPerOctave+c_NaturalKey_index[_index]);
				memDC.BitBlt(i,head_height,   m_naturalkey_width,m_naturalkey_height,   &keyDC, 0,0,	SRCCOPY);
				if (i == m_FocusKeyRect.left){
					TXT(&memDC,notetable[noteToSample.first],i+text_xOffset,text_yOffset,m_naturalkey_width-4,12);
				} else{ memDC.TextOut(i+text_xOffset,text_yOffset,notetable[noteToSample.first]); }

				int _sample = noteToSample.second;
				if ( _sample == 255 ) _tmp_str="--";
				else _tmp_str.Format("%02X",_sample);
				if (i == m_FocusKeyRect.left){
					TXT(&memDC,_tmp_str,i+text_xOffset,text_yOffset+12,m_naturalkey_width-4,12);
				} else { memDC.TextOut(i+text_xOffset,text_yOffset+12,_tmp_str); }

				_index++;
				if(_index == c_NaturalKeysPerOctave){
					_index = 0;
					_octave++;
				}
			}

			//Draw sharp keys
			keyDC.SelectObject(&m_SharpKey);
			//text_xOffset-=c_SharpKey_Xpos[0];
			memDC.SetTextColor(RGB(0x55,0xAA,0xFF));
			text_xOffset=3;
			text_yOffset=head_height+4;
			int octavebmpoffset = 0;
			_index = 0,_octave = m_Octave;
			for(int i = c_SharpKey_Xpos[0];i < _rect.Width() && _octave<10;i = octavebmpoffset + c_SharpKey_Xpos[_index])
			{
				XMInstrument::NotePair noteToSample = m_pInst->NoteToSample(_octave*c_KeysPerOctave+c_SharpKey_index[_index]);

				memDC.BitBlt(i,head_height, m_sharpkey_width,m_sharpkey_height, 	&keyDC,		0,0,	SRCCOPY);
				if (i == m_FocusKeyRect.left){
					TXT(&memDC,notetable[noteToSample.first],i+text_xOffset,text_yOffset,m_naturalkey_width-4,12);
				} else { memDC.TextOut(i+text_xOffset,text_yOffset,notetable[noteToSample.first]); }
				
				int _sample=noteToSample.second;
				if ( _sample == 255 ) _tmp_str="--";
				else  _tmp_str.Format("%02X",_sample);
				if (i == m_FocusKeyRect.left){
					TXT(&memDC,_tmp_str,i+text_xOffset,text_yOffset+12,m_naturalkey_width-4,12);
				} else { memDC.TextOut(i+text_xOffset,text_yOffset+12,_tmp_str); }

				_index++;
				if(_index == c_SharpKeysPerOctave){
					_index = 0;
					_octave++;
					octavebmpoffset += m_octave_width;
				}
			}

			//render and cleanup
			dc.BitBlt(0,0,	_rect.Width(),_rect.Height(),	&memDC,	0,0,SRCCOPY);
			keyDC.SelectObject(oldKeyBmp);
			memDC.SelectObject(oldMemBmp);
			memDC.SelectObject(oldmemfont);
			keyDC.DeleteDC();
			memDC.DeleteDC();
			dc.Detach();
		}
	}
}

int CSampleAssignEditor::GetKeyIndexAtPoint(const int x,const int y,CRect& keyRect)
{
	const int head_height = 20;

	if ( y < head_height || y > head_height+m_naturalkey_height ) return notecommands::empty;

	//Get the X position in natural key notes.
	int visualoctave = (x/m_octave_width);
	int notenatural = (x/m_naturalkey_width);
	int indexnote = c_NaturalKey_index[notenatural%c_NaturalKeysPerOctave];

	keyRect.top = head_height;
	keyRect.bottom = head_height+m_naturalkey_height;
	keyRect.left = notenatural*m_naturalkey_width;
	keyRect.right = keyRect.left+m_naturalkey_width;

	if ( y < head_height+m_sharpkey_height ) 
	{
		//If the code reaches here, we have to check if it is a sharp key or a natural one.

		//Check previous sharp note
		if (indexnote > 0 && c_NoteAssign[indexnote-1]==SharpKey)
		{
			const int note = c_noteAssignindex[indexnote-1];
			const int _xpos = c_SharpKey_Xpos[note] + visualoctave * m_octave_width;
			if(x >= _xpos && x <= (_xpos + m_sharpkey_width))
			{
				keyRect.bottom = m_sharpkey_height;
				keyRect.left = _xpos;
				keyRect.right = _xpos + m_sharpkey_width;
				return (m_Octave + visualoctave)*c_KeysPerOctave + indexnote-1;
			}
		}
		//Check next sharp note
		if ( indexnote+1<c_KeysPerOctave && c_NoteAssign[indexnote+1]==SharpKey)
		{
			const int note = c_noteAssignindex[indexnote+1];
			const int _xpos = c_SharpKey_Xpos[note] + visualoctave * m_octave_width;
			if(x >= _xpos && x <= (_xpos + m_sharpkey_width))
			{
				keyRect.bottom = m_sharpkey_height;
				keyRect.left = _xpos;
				keyRect.right = _xpos + m_sharpkey_width;
				return (m_Octave + visualoctave)*c_KeysPerOctave + indexnote+1;
			}
		}
	}
	//Not a valid sharp note. Return the already found note.
	return (m_Octave + visualoctave)*c_KeysPerOctave + indexnote;

}
void CSampleAssignEditor::OnMouseMove( UINT nFlags, CPoint point )
{
	int tmp = m_FocusKeyIndex;
	m_FocusKeyIndex=GetKeyIndexAtPoint(point.x,point.y,m_FocusKeyRect);
	if  ( tmp != m_FocusKeyIndex) {
		Invalidate();
	}
	if (!m_tracking){
		TRACKMOUSEEVENT tme;
		tme.cbSize=sizeof(TRACKMOUSEEVENT);
		tme.dwFlags=TME_LEAVE;
		tme.hwndTrack=GetSafeHwnd();
		tme.dwHoverTime=HOVER_DEFAULT;
		/*BOOL result =*/TrackMouseEvent(&tme);
		m_tracking=true;
	}

}
void CSampleAssignEditor::OnLButtonDown( UINT nFlags, CPoint point )
{
}
void CSampleAssignEditor::OnLButtonUp( UINT nFlags, CPoint point )
{
	if (notecommands::empty != m_FocusKeyIndex) {
		CInstrIndividualMap indDlg;
		indDlg.editPos = m_FocusKeyIndex;
		indDlg.xins = m_pInst;
		indDlg.DoModal();
		Invalidate();
		CInstrumentGenDlg* win = dynamic_cast<CInstrumentGenDlg*>(GetParent());
		win->ValidateEnabled();
	}
}
void CSampleAssignEditor::OnMouseLeave() {
	m_FocusKeyIndex = -1;
	m_FocusKeyRect.left =-1;
	m_tracking=false;
	Invalidate();
}

}}