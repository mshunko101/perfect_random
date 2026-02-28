
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
	CEdit m_serie_count;
	CComboBox m_serie_type;
	CComboBox m_serie_format;
	CEdit m_serie_min;
	CEdit m_serie_max;
	CComboBox m_serie_datatype;
	CEdit m_serie_filename;
	CProgressCtrl m_progress_bar;
	CComboBox m_randomizer;
	afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
};
