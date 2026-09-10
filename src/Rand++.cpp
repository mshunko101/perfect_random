// Rand++.cpp — WinAPI version (no MFC) + CLI mode
#include "pch.h"
#include "../perfect_random.hpp"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <fstream>
#include <iomanip>
#include <thread>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <functional>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")

// ── Custom messages ─────────────────────────────────────────────
#define WM_UPDATE_PROGRESS   (WM_APP + 1)
#define WM_TASK_COMPLETE     (WM_APP + 2)
#define WM_TASK_ERROR        (WM_APP + 3)

// ── Globals ─────────────────────────────────────────────────────
static HINSTANCE g_hInstance = nullptr;

enum class NumberSize { Byte = 8, Word = 16, DWord = 32 };

// ── Thread parameters ───────────────────────────────────────────
struct GenParams {
    HWND hDlg = nullptr;
    unsigned long long serie_count = 10000000;
    unsigned long long serie_min = 0;
    unsigned long long serie_max = 4294967295ULL;
    NumberSize number_size = NumberSize::DWord;
    bool output_double = false;
    bool binary_format = true;
    std::wstring filename = L"output.bin";
    double period = 42.0;       // Период вращения для RotationCalculator
};

// ── Helpers ─────────────────────────────────────────────────────
static std::wstring GetExecutableDirectory() {
    wchar_t szPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, szPath, MAX_PATH);
    std::wstring path(szPath);
    size_t pos = path.find_last_of(L'\\');
    return (pos != std::wstring::npos) ? path.substr(0, pos) : path;
}

static std::wstring LoadStr(UINT uID) {
    wchar_t buf[1024];
    int len = LoadStringW(g_hInstance, uID, buf, 1024);
    return std::wstring(buf, len);
}

static std::wstring SizeToStr(NumberSize s) {
    switch (s) {
    case NumberSize::Byte:  return L"byte";
    case NumberSize::Word:  return L"word";
    default:                return L"dword";
    }
}


// ── Shared generation core ──────────────────────────────────────
static bool GenerateCore(const GenParams& p, std::wstring& errMsg,
    std::function<void(size_t)> progressFn = nullptr) {
    if (p.serie_count == 0) { errMsg = L"count";  return false; }
    if (p.serie_min >= p.serie_max) { errMsg = L"minmax"; return false; }

    RNG rng(static_cast<unsigned int>(time(nullptr)), p.period);

    // Сколько байт тянем из генератора за одно число
    size_t pull_size;
    if (p.output_double) {
        pull_size = 8;
    }
    else {
        switch (p.number_size) {
        case NumberSize::Byte:  pull_size = 1; break;
        case NumberSize::Word:  pull_size = 2; break;
        default:                pull_size = 4; break;
        }
    }

    const size_t updateInterval = (p.serie_count >= 100)
        ? p.serie_count / 100 : 1;

    // --- Binary mode ---
    if (p.binary_format) {
        std::ofstream file(p.filename, std::ios::binary);
        if (!file.is_open()) { errMsg = L"file"; return false; }

        for (size_t i = 0; i < p.serie_count; ++i) {
            double r = rng.generate();
            r = p.serie_min + r * (p.serie_max - p.serie_min);

            if (p.output_double) {
                file.write(reinterpret_cast<const char*>(&r), sizeof(r));
            }
            else {
                switch (p.number_size) {
                case NumberSize::Byte: {
                    uint8_t v = static_cast<uint8_t>(r);
                    file.write(reinterpret_cast<const char*>(&v), sizeof(v));
                    break;
                }
                case NumberSize::Word: {
                    uint16_t v = static_cast<uint16_t>(r);
                    file.write(reinterpret_cast<const char*>(&v), sizeof(v));
                    break;
                }
                default: {
                    uint32_t v = static_cast<uint32_t>(r);
                    file.write(reinterpret_cast<const char*>(&v), sizeof(v));
                    break;
                }
                }
            }
            if (progressFn && (i + 1) % updateInterval == 0)
                progressFn(i + 1);
        }
    }
    // --- Text mode ---
    else {
        std::ofstream file(p.filename);
        if (!file.is_open()) { errMsg = L"file"; return false; }

        if (p.output_double)
            file << std::fixed << std::setprecision(15);

        for (size_t i = 0; i < p.serie_count; ++i) {
            double r = rng.generate();
            r = p.serie_min + r * (p.serie_max - p.serie_min);

            if (p.output_double) {
                file << r;
            }
            else {
                uint32_t value = static_cast<uint32_t>(r);
                switch (p.number_size) {
                case NumberSize::Byte:  file << static_cast<unsigned>(value & 0xFF);   break;
                case NumberSize::Word:  file << static_cast<unsigned>(value & 0xFFFF);  break;
                default:                file << value;                                   break;
                }
            }
            if (i < p.serie_count - 1) file << "\n";
            if (progressFn && (i + 1) % updateInterval == 0)
                progressFn(i + 1);
        }
    }
    return true;
}


// ── CLI mode ────────────────────────────────────────────────────
static int RunCLI(int argc, wchar_t* argv[]) {
    GenParams p;

    for (int i = 1; i < argc; i++) {
        std::wstring arg = argv[i];
        auto getNext = [&]() -> std::wstring {
            if (i + 1 < argc) return argv[++i];
            return L"";
            };

        if (arg == L"-n" || arg == L"--count")      p.serie_count = _wcstoui64(getNext().c_str(), nullptr, 10);
        else if (arg == L"-a" || arg == L"--min")    p.serie_min = _wcstoui64(getNext().c_str(), nullptr, 10);
        else if (arg == L"-b" || arg == L"--max")    p.serie_max = _wcstoui64(getNext().c_str(), nullptr, 10);
        else if (arg == L"-o" || arg == L"--output")  p.filename = getNext();
        else if (arg == L"-p" || arg == L"--period")  p.period = std::stod(getNext());
        else if (arg == L"-t" || arg == L"--type")   p.output_double = (getNext() == L"double" || getNext() == L"d");
        else if (arg == L"-f" || arg == L"--format") p.binary_format = (getNext() == L"bin" || getNext() == L"b");
        else if (arg == L"-s" || arg == L"--size") {
            std::wstring v = getNext();
            if (v == L"byte" || v == L"8")       p.number_size = NumberSize::Byte;
            else if (v == L"word" || v == L"16")  p.number_size = NumberSize::Word;
            else if (v == L"dword" || v == L"32") p.number_size = NumberSize::DWord;
            else                                  p.number_size = NumberSize::DWord;
        }
        else if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            std::wcout << L"Rand++ CLI mode (CascadePRNG)\n"
                << L"Usage: Rand++.exe [options]\n\n"
                << L"Options:\n"
                << L"  -n, --count N       Number of values (default: 10000000)\n"
                << L"  -a, --min N         Minimum value (default: 0)\n"
                << L"  -b, --max N         Maximum value (default: 4294967295)\n"
                << L"  -o, --output FILE   Output filename (default: output.bin)\n"
                << L"  -t, --type TYPE     double or int (default: int)\n"
                << L"  -f, --format FMT    bin or txt (default: bin)\n"
                << L"  -s, --size SIZE     byte, word, dword (default: dword)\n"
                << L"  -p, --period N      Rotation period in years (default: 42)\n"
                << L"  -h, --help          Show this help\n";
            return 0;
        }
    }

    // Resolve relative paths
    if (p.filename.find_first_of(L"\\/:") == std::wstring::npos)
        p.filename = GetExecutableDirectory() + L"\\" + p.filename;

    std::wcout << L"Generating " << p.serie_count << L" values"
        << L" (type=" << (p.output_double ? L"double" : L"int")
        << L", format=" << (p.binary_format ? L"bin" : L"txt")
        << L", size=" << SizeToStr(p.number_size)
        << L", period=" << p.period
        << L") -> " << p.filename << std::endl;

    std::wstring errMsg;
    if (GenerateCore(p, errMsg)) {
        std::wcout << L"Done: " << p.filename << std::endl;
        return 0;
    }

    if (errMsg == L"count")      std::wcerr << L"Error: count must be > 0" << std::endl;
    else if (errMsg == L"minmax") std::wcerr << L"Error: min must be < max" << std::endl;
    else if (errMsg == L"file")   std::wcerr << L"Error: cannot open file" << std::endl;
    else                          std::wcerr << L"Error: " << errMsg << std::endl;
    return 1;
}

// ── Worker thread (GUI mode) ────────────────────────────────────
static void GenerateThread(GenParams* params) {
    HWND hDlg = params->hDlg;

    auto progressFn = [hDlg, params](size_t processed) {
        if (IsWindow(hDlg))
            PostMessageW(hDlg, WM_UPDATE_PROGRESS,
                static_cast<WPARAM>(processed * 100 / params->serie_count), 0);
        };

    std::wstring errMsg;
    bool ok = GenerateCore(*params, errMsg, progressFn);

    if (!ok) {
        if (IsWindow(hDlg)) {
            UINT id = (errMsg == L"count") ? IDS_MIN_SERIE_COUNT :
                (errMsg == L"minmax") ? IDS_MIN_MAX_CONDITION :
                IDS_FILE_ERROR;
            std::wstring* pMsg = new std::wstring(LoadStr(id));
            PostMessageW(hDlg, WM_TASK_ERROR, 0,
                reinterpret_cast<LPARAM>(pMsg));
        }
    }
    else {
        if (IsWindow(hDlg))
            PostMessageW(hDlg, WM_TASK_COMPLETE, 0, 0);
    }
    delete params;
}

// ── About dialog ─────────────────────────────────────────────────
static INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// ── Main dialog ──────────────────────────────────────────────────
static INT_PTR CALLBACK RandDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HICON hIcon = nullptr;

    switch (msg) {

    case WM_INITDIALOG: {
        hIcon = LoadIconW(g_hInstance, MAKEINTRESOURCEW(IDR_MAINFRAME));
        if (hIcon) {
            SendMessageW(hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
            SendMessageW(hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
        }

        HMENU hSysMenu = GetSystemMenu(hDlg, FALSE);
        if (hSysMenu) {
            AppendMenuW(hSysMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hSysMenu, MF_STRING, IDM_ABOUTBOX, LoadStr(IDS_ABOUTBOX).c_str());
        }

        const wchar_t* counts[] = {
            L"10", L"100", L"1000", L"10000",
            L"134217728", L"268435456", L"536870912", L"1668467902"
        };
        for (auto s : counts)
            SendDlgItemMessageW(hDlg, IDC_SERIES_COUNT, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));

        const wchar_t* maxes[] = { L"256", L"65536", L"4294967296" };
        for (auto s : maxes)
            SendDlgItemMessageW(hDlg, IDC_SERIE_MAX, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));

        const wchar_t* periods[] = {  L"73.8" };
        for (auto s : periods)
            SendDlgItemMessageW(hDlg, IDC_SERIES_PERIOD, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));

        SetDlgItemTextW(hDlg, IDC_SERIE_MAX, L"0");
        SetDlgItemTextW(hDlg, IDC_SERIE_MIN, L"0");
        SetDlgItemTextW(hDlg, IDC_SERIES_PERIOD, L"42");
        return TRUE;
    }

    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == IDM_ABOUTBOX) {
            DialogBoxW(g_hInstance, MAKEINTRESOURCEW(IDD_ABOUTBOX), hDlg, AboutDlgProc);
            return TRUE;
        }
        break;

    case WM_HELP:
        DialogBoxW(g_hInstance, MAKEINTRESOURCEW(IDD_ABOUTBOX), hDlg, AboutDlgProc);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {

        case ID_SERIE_GENERATION_START: {
            GenParams* p = new GenParams;
            p->hDlg = hDlg;

            wchar_t buf[256];

            GetDlgItemTextW(hDlg, IDC_SERIES_COUNT, buf, 256);
            p->serie_count = wcstoull(buf, nullptr, 10);

            GetDlgItemTextW(hDlg, IDC_SERIE_MIN, buf, 256);
            p->serie_min = _wtoi64(buf);

            GetDlgItemTextW(hDlg, IDC_SERIE_MAX, buf, 256);
            p->serie_max = _wtoi64(buf);

            GetDlgItemTextW(hDlg, IDC_SERIE_FILENAME, buf, 256);
            p->filename = GetExecutableDirectory() + L"\\" + buf;

            GetDlgItemTextW(hDlg, IDC_SERIES_PERIOD, buf, 256);
            p->period = _wtof(buf);
            if (p->period < 1.0) p->period = 42.0;

            p->output_double = IsDlgButtonChecked(hDlg, IDC_TYPE_DOUBLE) == BST_CHECKED;
            p->binary_format = IsDlgButtonChecked(hDlg, IDC_TYPE_BINARY) == BST_CHECKED;

            if (IsDlgButtonChecked(hDlg, IDC_TYPE_BYTE) == BST_CHECKED) p->number_size = NumberSize::Byte;
            else if (IsDlgButtonChecked(hDlg, IDC_TYPE_WORD) == BST_CHECKED) p->number_size = NumberSize::Word;
            else p->number_size = NumberSize::DWord;

            std::thread(GenerateThread, p).detach();
            return TRUE;
        }

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;

        case IDC_TYPE_DOUBLE:
            EnableWindow(GetDlgItem(hDlg, IDC_TYPE_BYTE), FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_TYPE_WORD), FALSE);
            EnableWindow(GetDlgItem(hDlg, IDC_TYPE_DWORD), TRUE);
            CheckRadioButton(hDlg, IDC_TYPE_BYTE, IDC_TYPE_DWORD, IDC_TYPE_DWORD);
            return TRUE;

        case IDC_TYPE_INTEGER:
            EnableWindow(GetDlgItem(hDlg, IDC_TYPE_BYTE), TRUE);
            EnableWindow(GetDlgItem(hDlg, IDC_TYPE_WORD), TRUE);
            EnableWindow(GetDlgItem(hDlg, IDC_TYPE_DWORD), TRUE);
            return TRUE;
        }
        break;

    case WM_UPDATE_PROGRESS:
        SendDlgItemMessageW(hDlg, IDC_PROGRESS_BAR, PBM_SETPOS, wParam, 0);
        return TRUE;

    case WM_TASK_COMPLETE:
        SendDlgItemMessageW(hDlg, IDC_PROGRESS_BAR, PBM_SETPOS, 0, 0);
        return TRUE;

    case WM_TASK_ERROR: {
        std::wstring* pErr = reinterpret_cast<std::wstring*>(lParam);
        if (pErr) {
            MessageBoxW(hDlg, pErr->c_str(), L"Ошибка", MB_ICONERROR);
            delete pErr;
        }
        SendDlgItemMessageW(hDlg, IDC_PROGRESS_BAR, PBM_SETPOS, 0, 0);
        return TRUE;
    }

    case WM_PAINT:
        if (IsIconic(hDlg)) {
            PAINTSTRUCT ps;
            HDC hDC = BeginPaint(hDlg, &ps);
            SendMessageW(hDlg, WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(hDC), 0);
            int cx = GetSystemMetrics(SM_CXICON);
            int cy = GetSystemMetrics(SM_CYICON);
            RECT rc; GetClientRect(hDlg, &rc);
            DrawIcon(hDC, (rc.right - cx + 1) / 2, (rc.bottom - cy + 1) / 2, hIcon);
            EndPaint(hDlg, &ps);
            return TRUE;
        }
        break;

    case WM_QUERYDRAGICON:
        return reinterpret_cast<INT_PTR>(hIcon);

    case WM_CLOSE:
        EndDialog(hDlg, 0);
        return TRUE;
    }

    return FALSE;
}

// ── Entry point ─────────────────────────────────────────────────
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    g_hInstance = hInstance;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argv && argc > 1) {
        int ret = RunCLI(argc, argv);
        LocalFree(argv);
        return ret;
    }
    if (argv) LocalFree(argv);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);

    DialogBoxW(hInstance, MAKEINTRESOURCEW(IDD_RAND_DIALOG), nullptr, RandDlgProc);
    return 0;
}
