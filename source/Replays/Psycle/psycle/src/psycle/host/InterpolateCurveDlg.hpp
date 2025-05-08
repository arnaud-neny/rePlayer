///\file
///\brief interface file for psycle::host::InterpolateCurveDlg.
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
namespace psycle { namespace host {

	class CInterpolateCurve : public CDialog
	{
		public:
			CInterpolateCurve(int startsel, int endsel, int _linesperbeat, CWnd* pParent = 0);
			~CInterpolateCurve();
			
			void AssignInitialValues(int* values,int commandtype, int minval, int maxval);
			
		// Dialog Data
			enum { IDD = IDD_INTERPOLATE_CURVE };
			CButton m_checktwk;
			CComboBox m_combotwk;
			CEdit m_Pos;
			CEdit m_Value;
			CEdit m_Min;
			CEdit m_Max;
			CComboBox m_CurveType;
		// Overrides
		protected:
			virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
			virtual BOOL OnInitDialog();
		// Implementation
		protected:
			DECLARE_MESSAGE_MAP()
			afx_msg void OnOk();
			afx_msg void OnPaint();
			afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
			afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
			afx_msg void OnMouseMove(UINT nFlags, CPoint point);
			afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
			afx_msg void OnSelendokCurveType();
			afx_msg void OnEnKillfocusPos();
			afx_msg void OnEnKillfocusVal();
			afx_msg void OnEnKillfocusMin();
			afx_msg void OnEnKillfocusMax();
			afx_msg void OnBnClickedChecktwk();
		private:
			void AdjustPointToView(CPoint&point);
			void AdjustRectToView(RECT&rect);
			RECT GetGPointRect(int i);
			int GetPointFromX(LONG x);
			void GetNextkfvalue(int &startpos);
			void FillReturnValues();
			void SetPosText(int i);
			void SetValText(int i);
			void SetMinText(int i);
			void SetMaxText(int i);
			int GetPosValue();
			int GetValValue();
			int GetMinValue();
			int GetMaxValue();
			float HermiteCurveInterpolate(int kf0, int kf1, int kf2, int kf3, int curposition, int maxposition, float tangmult, bool interpolation);

			int startIndex;
			int numLines;
			int linesperbeat;
			int min_val;
			int max_val;
			int y_range;
			RECT grapharea;
			int	xoffset;
			float xscale;
			float yscale;
			int selectedGPoint;
			bool bDragging;
			typedef struct keyframesstruct {
				int value;
				int curvetype;
			};
			keyframesstruct *kf;
		public:
			int *kfresult;
			int kftwk;
	};

	}   // namespace
}   // namespace
