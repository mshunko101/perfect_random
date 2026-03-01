
// Rand++Dlg.h: файл заголовка
//

#pragma once
#include "Randomizer.h"
#include "generator_std.h"
#define WM_UPDATE_PROGRESS (WM_USER + 100)
#define WM_TASK_COMPLETE (WM_USER + 101)
#define WM_TASK_ERROR (WM_USER + 102)


// Диалоговое окно CRandDlg
class CRandDlg : public CDialogEx
{
// Создание
public:
	CRandDlg(CWnd* pParent = nullptr);	// стандартный конструктор

// Данные диалогового окна
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_RAND_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// поддержка DDX/DDV

	CWinThread* m_pBackgroundThread;
// Реализация
protected:
	HICON m_hIcon;

	// Созданные функции схемы сообщений
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedSerieGenerationStart();
	afx_msg LRESULT OnUpdateProgress(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTaskComplete(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnTaskError(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()
public:
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
	CString m_serie_count;
	CButton m_type_integer;
	CButton m_type_double;
	CButton m_type_text;
	CButton m_type_binary;
	CEdit m_serie_min;
	CComboBox m_serie_max;
	CButton m_type_byte;
	CButton m_type_word;
	CButton m_type_dword;
	CEdit m_serie_filename;
	CButton m_type_std;
	CButton m_type_mshunko;
	CProgressCtrl m_progress_bar;
	afx_msg void OnBnClickedTypeDouble();
	afx_msg void OnBnClickedTypeInteger();
	afx_msg void OnCbnSelchangeCombo1();
};
