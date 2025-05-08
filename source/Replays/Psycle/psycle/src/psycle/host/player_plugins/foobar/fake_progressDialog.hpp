#include <psycle/host/detail/project.hpp>
#include <psycle/host/Global.hpp>
namespace psycle
{
	namespace host
	{
		class CProgressDialogCtrl
		{
		public:
			CProgressDialogCtrl(){};
			void SetPos(int i){};
			void SetRange(int i, int x){};
			void SetStep(int nStep){};
			void StepIt(){};
		};

		class CProgressDialog
		{
		public:
			CProgressDialog(CWnd* pParent = 0){};
			BOOL Create(){ return FALSE; };
			void SetWindowText(LPCTSTR lpszString){}
			void SendMessage(UINT /*msg*/){}
			BOOL ShowWindow(int nCmdShow){ return FALSE; };
			virtual void OnCancel(void){};

			CProgressDialogCtrl m_Progress;
		};
	}
}
