#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "XMInstrument.hpp"

namespace psycle { namespace host {

class XMSampler;

class CEnvelopeEditor : public CStatic
	{
	public:
		// constant
		static const int POINT_SIZE = 6;///< Envelope Point size, in pixels.

		CEnvelopeEditor();
		virtual ~CEnvelopeEditor();

		void Initialize(XMInstrument::Envelope& pEnvelope,int tpb=24, int millis=1);
		virtual void DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct );

		XMInstrument::Envelope& envelope() { return *m_pEnvelope; }
		void ConvertToADSR(bool allowgain=false);
		bool negative() { return m_bnegative; }
		void negative(bool negative) { m_bnegative=negative; }
		bool canSustain() { return m_bCanSustain; }
		void canSustain(bool sustain) { m_bCanSustain=sustain; }
		bool canLoop() { return m_bCanLoop; }
		void canLoop(bool loop) { m_bCanLoop=loop; }
		void envelopeIdx(int idx) { m_envelopeIdx=idx; }
		int envelopeIdx() { return m_envelopeIdx; }
		int editPoint() { return m_EditPoint; }
		void TimeFormulaTicks(int ticksperbeat=24);
		void TimeFormulaMillis(int millisperpoint=1);

		// Only meaningful when freeform is false
		void AttackTime(int time, bool rezoom=true);
		int AttackTime() { return m_pEnvelope->GetTime(1)-m_pEnvelope->GetTime(0); }
		void DecayTime(int time, bool rezoom=true);
		int DecayTime() { return m_pEnvelope->GetTime(2)-m_pEnvelope->GetTime(1); }
		void SustainValue(XMInstrument::Envelope::ValueType value) { m_pEnvelope->SetValue(2,value); }
		XMInstrument::Envelope::ValueType SustainValue() { return m_pEnvelope->GetValue(2); }
		void ReleaseTime(int time, bool rezoom=true);
		int ReleaseTime() { return m_pEnvelope->GetTime(3)-m_pEnvelope->GetTime(2); }

	protected:
		DECLARE_MESSAGE_MAP()
		afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
		afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
		afx_msg void OnMouseMove( UINT nFlags, CPoint point );
		afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
		afx_msg void OnPopAddPoint();
		afx_msg void OnPopSustainStart();
		afx_msg void OnPopSustainEnd();
		afx_msg void OnPopLoopStart();
		afx_msg void OnPopLoopEnd();
		afx_msg void OnPopRemovePoint();
		afx_msg void OnPopRemoveSustain();
		afx_msg void OnPopRemoveLoop();
		afx_msg void OnPopRemoveEnvelope();

	protected:

		/**  */
		inline const int GetEnvelopePointIndexAtPoint(const int x,const int y)
		{
			unsigned int const _points = m_pEnvelope->NumOfPoints();
			for(unsigned int i = 0;i < _points ;i++)
			{
				CPoint _pt_env;
				_pt_env.y = (m_bnegative) 
					? static_cast<int>((float)m_WindowHeight * (1.0f - (1.0f+m_pEnvelope->GetValue(i))*0.5f))
					: static_cast<int>((float)m_WindowHeight * (1.0f - m_pEnvelope->GetValue(i)));
				_pt_env.x = static_cast<int>(m_Zoom * (float)m_pEnvelope->GetTime(i));

				if(((_pt_env.x - POINT_SIZE / 2) <= x) & ((_pt_env.x + POINT_SIZE / 2) >= x) &
					((_pt_env.y - POINT_SIZE / 2) <= y) & ((_pt_env.y + POINT_SIZE / 2) >= y))
				{
					return i;
				}
			}

			return _points; // return == _points -> Point not found.
		}
		void FitZoom();

		XMInstrument::Envelope* m_pEnvelope;
		bool m_bInitialized;
		bool m_bnegative;
		int m_envelopeIdx;
		float m_Zoom;///< Zoom
		int m_millisPerPoint;
		int m_ticksPerBeat;
		int m_WindowHeight;
		int m_WindowWidth;
		bool m_bCanSustain;
		bool m_bCanLoop;

		bool m_bPointEditing;///< EnvelopePoint 
		int m_EditPoint;///< ***** Envelope Point Index
		int m_EditPointX;///< Envelope Point
		int m_EditPointY;///< Envelope Point
		bool m_bCanMoveUpDown;

		CPen _line_pen;
		CPen _gridpen;
		CPen _gridpen1;
		CBrush _point_brush;
	};
}}