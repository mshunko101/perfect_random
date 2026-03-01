
// Rand++Dlg.cpp: файл реализации
//

#include "pch.h"
#include "framework.h"
#include "Rand++.h"
#include "Rand++Dlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
CString GetExecutableDirectory()
{
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, MAX_PATH);

    CString strPath(szPath);
    int pos = strPath.ReverseFind('\\');
    if (pos != -1)
    {
        strPath = strPath.Left(pos);
    }
    return strPath;
}
// Объявление функции потока
UINT MyComplexThread(LPVOID pParam);

// Диалоговое окно CAboutDlg используется для описания сведений о приложении

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Данные диалогового окна
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // поддержка DDX/DDV

// Реализация
protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedSerieGenerationStart();
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// Диалоговое окно CRandDlg



CRandDlg::CRandDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_RAND_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CRandDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_SERIES_COUNT, m_serie_count);
    DDX_Control(pDX, IDC_TYPE_INTEGER, m_type_integer);
    DDX_Control(pDX, IDC_TYPE_DOUBLE, m_type_double);
    DDX_Control(pDX, IDC_TYPE_TEXT, m_type_text);
    DDX_Control(pDX, IDC_TYPE_BINARY, m_type_binary);
    DDX_Control(pDX, IDC_SERIE_MIN, m_serie_min);
    DDX_Control(pDX, IDC_SERIE_MAX, m_serie_max);
    DDX_Control(pDX, IDC_TYPE_BYTE, m_type_byte);
    DDX_Control(pDX, IDC_TYPE_WORD, m_type_word);
    DDX_Control(pDX, IDC_TYPE_DWORD, m_type_dword);
    DDX_Control(pDX, IDC_SERIE_FILENAME, m_serie_filename);
    DDX_Control(pDX, IDC_TYPE_STD, m_type_std);
    DDX_Control(pDX, IDC_TYPE_MSHUNKO, m_type_mshunko);
    DDX_Control(pDX, IDC_PROGRESS_BAR, m_progress_bar);
}

BEGIN_MESSAGE_MAP(CRandDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(ID_SERIE_GENERATION_START, &CRandDlg::OnBnClickedSerieGenerationStart)
    ON_MESSAGE(WM_UPDATE_PROGRESS, &CRandDlg::OnUpdateProgress)
    ON_MESSAGE(WM_TASK_COMPLETE, &CRandDlg::OnTaskComplete)
    ON_MESSAGE(WM_TASK_ERROR, &CRandDlg::OnTaskError)
    ON_WM_HELPINFO()
    ON_BN_CLICKED(IDC_TYPE_DOUBLE, &CRandDlg::OnBnClickedTypeDouble)
    ON_BN_CLICKED(IDC_TYPE_INTEGER, &CRandDlg::OnBnClickedTypeInteger)
END_MESSAGE_MAP()


// Обработчики сообщений CRandDlg

BOOL CRandDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Добавление пункта "О программе..." в системное меню.

	// IDM_ABOUTBOX должен быть в пределах системной команды.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}
    m_serie_count.AddString(_T("10"));
    m_serie_count.AddString(_T("100"));
    m_serie_count.AddString(_T("1000"));
    m_serie_count.AddString(_T("10000"));
    m_serie_count.AddString(_T("100000"));
    m_serie_count.AddString(_T("1000000"));
    m_serie_count.AddString(_T("10000000"));
    m_serie_max.AddString(_T("256"));
    m_serie_max.AddString(_T("4294967296"));
    m_serie_max.SetWindowTextW(_T("0"));
    m_serie_min.SetWindowTextW(_T("0"));
	// Задает значок для этого диалогового окна.  Среда делает это автоматически,
	//  если главное окно приложения не является диалоговым
	SetIcon(m_hIcon, TRUE);			// Крупный значок
	SetIcon(m_hIcon, FALSE);		// Мелкий значок
   
	return TRUE;  // возврат значения TRUE, если фокус не передан элементу управления
}

LRESULT CRandDlg::OnUpdateProgress(WPARAM wParam, LPARAM)
{
    int progress = static_cast<int>(wParam);
    m_progress_bar.SetPos(progress);
    return 0;
}

LRESULT CRandDlg::OnTaskComplete(WPARAM, LPARAM)
{
    // Сбрасываем прогресс‑бар
    m_progress_bar.SetPos(0);
    return 0;
}

LRESULT CRandDlg::OnTaskError(WPARAM, LPARAM lParam)
{
    CString* pError = (CString*)lParam;
    MessageBox(*pError, L"Ошибка", MB_ICONERROR);
    // Сбрасываем прогресс‑бар
    m_progress_bar.SetPos(0);
    delete pError;
    return 0;
}

void CRandDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// При добавлении кнопки свертывания в диалоговое окно нужно воспользоваться приведенным ниже кодом,
//  чтобы нарисовать значок.  Для приложений MFC, использующих модель документов или представлений,
//  это автоматически выполняется рабочей областью.

void CRandDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // контекст устройства для рисования

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Выравнивание значка по центру клиентского прямоугольника
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Нарисуйте значок
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// Система вызывает эту функцию для получения отображения курсора при перемещении
//  свернутого окна.
HCURSOR CRandDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CRandDlg::OnBnClickedSerieGenerationStart()
{
    UpdateData();
	// Пытаемся создать поток
	CWinThread* pWorker = AfxBeginThread(MyComplexThread, this,
		THREAD_PRIORITY_BELOW_NORMAL, 0, CREATE_SUSPENDED);

	if (!pWorker)
	{
		MessageBox(_T("Ошибка создания потока!"));
		return;
	}

	// Запускаем приостановленный поток
	pWorker->ResumeThread();

	// Сохраняем указатель, если нужно будет управлять потоком позже
	m_pBackgroundThread = pWorker;
}

UINT MyComplexThread(LPVOID pParam)
{
    CRandDlg* pDlg = (CRandDlg*)pParam;
    CString buffer;
    pDlg->m_serie_count.GetWindowTextW(buffer);
    // Извлекаем данные из элементов управления
    unsigned long serie_count = _ttoi64(buffer);

    CString serie_format = pDlg->m_type_text.GetCheck() > 0 ? _T("txt") : _T("bin");

    CString serie_type = pDlg->m_type_integer.GetCheck() > 0 ? _T("int") : _T("double");

    pDlg->m_serie_min.GetWindowTextW(buffer);
    size_t serie_min = _tstoi64(buffer);

    pDlg->m_serie_max.GetWindowText(buffer);
    size_t serie_max = _tstoi64(buffer);

    CString serie_datatype;
    if(pDlg->m_type_byte.GetCheck())
    {
        serie_datatype = _T("byte");
    }
    if (pDlg->m_type_word.GetCheck())
    {
        serie_datatype = _T("word");
    }
    if (pDlg->m_type_dword.GetCheck())
    {
        serie_datatype = _T("dword");
    }

    CString serie_filename_tmp;
    pDlg->m_serie_filename.GetWindowTextW(serie_filename_tmp);

    CString randomizer;
    if (pDlg->m_type_std.GetCheck())
    {
        randomizer = _T("std");
    }
    if (pDlg->m_type_mshunko.GetCheck())
    {
        randomizer = _T("mshunko");
    }


    CString executableDir = GetExecutableDirectory();

    // Формируем полный путь к файлу
    CString fullPath = executableDir + _T("\\") + serie_filename_tmp;

    // Преобразуем CString в std::string для работы с STL
    std::string filename = CT2A(fullPath).m_psz;

    try
    {
        size_t processed = 0;
        const size_t updateInterval = serie_count / 100;  // обновлять каждые 1 %

        // Валидация входных данных
        if (serie_count <= 0)
            throw std::invalid_argument("Количество чисел должно быть больше 0");

        if (serie_min >= serie_max)
            throw std::invalid_argument("min должен быть меньше max");

        // Парсинг типа данных
        bool output_double = false;
        if (serie_type == L"double")
            output_double = true;
        else if (serie_type != L"int")
            throw std::invalid_argument("Тип должен быть 'double' или 'int'");

        // Парсинг формата вывода
        bool binary_format = false;
        if (serie_format == L"bin")
            binary_format = true;
        else if (serie_format != L"txt")
            throw std::invalid_argument("Формат должен быть 'txt' или 'bin'");

        // Парсинг размера числа
        enum class NumberSize { Byte, Word, DWord } number_size;
        if (serie_datatype == L"byte")
            number_size = NumberSize::Byte;
        else if (serie_datatype == L"word")
            number_size = NumberSize::Word;
        else if (serie_datatype == L"dword")
            number_size = NumberSize::DWord;
        else
            throw std::invalid_argument("Размер должен быть 'byte', 'word' или 'dword'");

        // Преобразуем CString в std::string для работы с STL 

        // Инициализация генератора случайных чисел

        StdGeneralRNG rg;
        RNG rng_perfect_var(static_cast<unsigned int>(time(nullptr)));
        RNGAbstract* rng_perfect = &rng_perfect_var;
        if (randomizer == _T("std"))
        {
            rng_perfect = &rg;
        }
        
        if (binary_format)
        {
            // Открываем файл в бинарном режиме
            std::ofstream file(filename, std::ios::binary);
            if (!file.is_open())
                throw std::runtime_error("Не удалось открыть файл для записи");

            if (output_double)
            {
                // Запись double значений в заданном диапазоне
                for (size_t i = 0; i < serie_count; ++i)
                {
                    double random_double = rng_perfect->generate();
                    random_double = serie_min + random_double * (serie_max - serie_min);
                    file.write(reinterpret_cast<const char*>(&random_double), sizeof(random_double));

                    processed++;
                    if (updateInterval > 0 && processed % updateInterval == 0)
                    {
                        int progress = static_cast<int>((processed * 100) / serie_count);
                        pDlg->PostMessage(WM_UPDATE_PROGRESS, progress, 0);
                    }
                }
            }
            else
            {
                // Запись целых чисел разного размера
                for (size_t i = 0; i < serie_count; ++i)
                {
                    double random_double = rng_perfect->generate();
                    random_double = serie_min + random_double * (serie_max - serie_min);

                    switch (number_size)
                    {
                    case NumberSize::Byte:
                    {
                        uint8_t random_byte = static_cast<uint8_t>(random_double);
                        file.write(reinterpret_cast<const char*>(&random_byte), sizeof(random_byte));
                        break;
                    }
                    case NumberSize::Word:
                    {
                        uint16_t random_word = static_cast<uint16_t>(random_double);
                        file.write(reinterpret_cast<const char*>(&random_word), sizeof(random_word));
                        break;
                    }
                    case NumberSize::DWord:
                    {
                        uint32_t random_dword = static_cast<uint32_t>(random_double);
                        file.write(reinterpret_cast<const char*>(&random_dword), sizeof(random_dword));
                        break;
                    }
                    }
                    processed++;
                    if (updateInterval > 0 && processed % updateInterval == 0)
                    {
                        int progress = static_cast<int>((processed * 100) / serie_count);
                        pDlg->PostMessage(WM_UPDATE_PROGRESS, progress, 0);
                    }
                }
            }
            file.close();
        }
        else
        {
            // Открываем файл в текстовом режиме
            std::ofstream file(filename);
            if (!file.is_open())
                throw std::runtime_error("Не удалось открыть файл для записи");

            // Устанавливаем точность для double
            if (output_double)
                file << std::fixed << std::setprecision(15);

            for (size_t i = 0; i < serie_count; ++i)
            {
                double random_double = rng_perfect->generate();
                random_double = serie_min + random_double * (serie_max - serie_min);

                if (output_double)
                {
                    file << random_double;
                }
                else
                {
                    uint32_t value = static_cast<uint32_t>(random_double);
                    switch (number_size)
                    {
                    case NumberSize::Byte: file << static_cast<uint8_t>(value); break;
                    case NumberSize::Word: file << static_cast<uint16_t>(value); break;
                    case NumberSize::DWord: file << value; break;
                    }
                }
                processed++;
                if (updateInterval > 0 && processed % updateInterval == 0)
                {
                    int progress = static_cast<int>((processed * 100) / serie_count);
                    pDlg->PostMessage(WM_UPDATE_PROGRESS, progress, 0);
                }
                // Добавляем разделитель (пробел) для всех чисел, кроме последнего
                if (i < serie_count - 1)
                    file << "\n";
            }
            file.close();
        }
    }
    catch (const std::exception& e)
    {
        // Отправляем ошибку в UI
        CString errorMsg;
        errorMsg.Format(L"Ошибка: %s", CA2W(e.what()).m_psz);
        AfxMessageBox(errorMsg);
        AfxEndThread(1);
        return 1;
    }
    pDlg->PostMessage(WM_TASK_COMPLETE, 0, 0);
    AfxEndThread(0);
    return 0;
}


BOOL CRandDlg::OnHelpInfo(HELPINFO* pHelpInfo)
{
    CAboutDlg dlg;
    dlg.DoModal();
    return TRUE;
}

void CRandDlg::OnBnClickedTypeDouble()
{
    m_type_byte.EnableWindow(false);
    m_type_word.EnableWindow(false);
    m_type_dword.EnableWindow(true);
    m_type_dword.SetCheck(1);
}

void CRandDlg::OnBnClickedTypeInteger()
{
    m_type_byte.EnableWindow(true);
    m_type_word.EnableWindow(true);
    m_type_dword.EnableWindow(true);
}
