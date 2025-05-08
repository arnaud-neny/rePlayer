#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"

namespace psycle { namespace host {
	
class XMInstrument;

class CSampleAssignEditor : public CStatic
{
		friend class XMSamplerUIInst;
	public:
		enum TNoteKey
		{
			NaturalKey=0,
			SharpKey
		};

		static const int c_NaturalKeysPerOctave;
		static const int c_SharpKeysPerOctave;
		static const int c_KeysPerOctave;
		static const int c_SharpKey_Xpos[];
		static const TNoteKey c_NoteAssign[];
		static const int c_noteAssignindex[];
		static const CString c_Key_name[];
		static const CString c_NaturalKey_name[];
		static const int c_NaturalKey_index[];
		static const int c_SharpKey_index[];
	public:
		CSampleAssignEditor();
		virtual ~CSampleAssignEditor();

		void Initialize(XMInstrument& pInstrument);
		inline int Octave() { return m_Octave; }
		inline void Octave(int octave) { m_Octave = octave; }

	protected:
		virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct );

	protected:
		DECLARE_MESSAGE_MAP()
		afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
		afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
		afx_msg void OnMouseMove( UINT nFlags, CPoint point );
		afx_msg void OnMouseLeave();

	protected:
		int GetKeyIndexAtPoint(const int x,const int y,CRect& keyRect);
		inline void TXT(CDC *devc,const CString& txt, int x,int y,int w,int h)
		{
			CRect Rect;
			Rect.left=x-1;
			Rect.top=y;
			Rect.right=x+w;
			Rect.bottom=y+h;
			devc->ExtTextOut(x,y,ETO_OPAQUE | ETO_CLIPPED ,Rect,txt,NULL);
		}
		XMInstrument *m_pInst;
		int m_naturalkey_width;
		int m_naturalkey_height;
		int m_sharpkey_width;
		int m_sharpkey_height;
		int m_octave_width;
		bool	m_bInitialized;
		bool    m_tracking;
		int		m_Octave;
		int m_FocusKeyIndex;///< 
		CRect m_FocusKeyRect;///<

		CBitmap m_NaturalKey;
		CBitmap m_SharpKey;
		CBitmap m_BackKey;
		CBitmap* bmpDC;

	};
}}