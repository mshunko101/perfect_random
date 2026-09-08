// Rand++.cpp — WinAPI version (no MFC) + CLI mode
#pragma once
#define APPLICATION
#include "pch.h"
#include "../randpp.h"
#include <stdio.h>

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <fstream>
#include <iomanip>
#include <thread>
#include <cstdint>
#include <ctime>
#include <sstream>

#include "resource.h"
#include <iostream>

#pragma comment(lib, "comctl32.lib")

// ── Custom messages ─────────────────────────────────────────────
#ifndef WM_UPDATE_PROGRESS
#define WM_UPDATE_PROGRESS   (WM_APP + 1)
#endif
#ifndef WM_TASK_COMPLETE
#define WM_TASK_COMPLETE     (WM_APP + 2)
#endif
#ifndef WM_TASK_ERROR
#define WM_TASK_ERROR        (WM_APP + 3)
#endif

// ── Globals ─────────────────────────────────────────────────────
static HINSTANCE g_hInstance = nullptr;

enum class NumberSize { Byte = 8, Word = 16, DWord = 32 };

// ── Thread parameters ───────────────────────────────────────────
struct ThreadParams {
    HWND hDlg;
    unsigned long long serie_count;
    bool output_double;
    bool binary_format;
    unsigned long long serie_min;
    unsigned long long serie_max;
    NumberSize number_size;
    std::wstring filename;
    size_t N;          // шагов каскада между пермутациями
};

// ── Helpers ──────────────────────────────────────────────────────
std::wstring GetExecutableDirectory() {
    wchar_t szPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, szPath, MAX_PATH);
    std::wstring path(szPath);
    size_t pos = path.find_last_of(L'\\');
    if (pos != std::wstring::npos)
        path = path.substr(0, pos);
    return path;
}

std::wstring LoadStr(UINT uID) {
    wchar_t buf[1024];
    int len = LoadStringW(g_hInstance, uID, buf, 1024);
    return std::wstring(buf, len);
}

// ── Core generation logic (shared by GUI and CLI) ───────────────
bool GenerateFile(const ThreadParams& p, std::wstring& errMsg) {
    if (p.serie_count == 0) {
        errMsg = L"count";
        return false;
    }
    if (p.serie_min >= p.serie_max) {
        errMsg = L"minmax";
        return false;
    }

    size_t N = p.N;
    if (N < 1) N = 1;

    CascadePRNG rng(static_cast<unsigned int>(time(nullptr)), N);

    if (p.binary_format) {
        std::ofstream file(p.filename, std::ios::binary);
        if (!file.is_open()) {
            errMsg = L"file";
            return false;
        }

        if (p.output_double) {
            for (size_t i = 0; i < p.serie_count; ++i) {
                double r = rng.generate();
                r = p.serie_min + r * (p.serie_max - p.serie_min);
                file.write(reinterpret_cast<const char*>(&r), sizeof(r));
            }
        }
        else {
            for (size_t i = 0; i < p.serie_count; ++i) {
                double r = rng.generate();
                r = p.serie_min + r * (p.serie_max - p.serie_min);
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
                case NumberSize::DWord: {
                    uint32_t v = static_cast<uint32_t>(r);
                    file.write(reinterpret_cast<const char*>(&v), sizeof(v));
                    break;
                }
                }
            }
        }
    }
    else {
        std::ofstream file(p.filename);
        if (!file.is_open()) {
            errMsg = L"file";
            return false;
        }

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
                case NumberSize::Byte:  file << static_cast<uint8_t>(value);  break;
                case NumberSize::Word:  file << static_cast<uint16_t>(value); break;
                case NumberSize::DWord: file << value; break;
                }
            }
            if (i < p.serie_count - 1)
                file << "\n";
        }
    }
    return true;
}

// ── CLI mode ─────────────────────────────────────────────────────
int RunCLI(int argc, wchar_t* argv[]) {
    ThreadParams p;
    p.serie_count = 10000000;
    p.output_double = false;
    p.binary_format = true;
    p.serie_min = 0;
    p.serie_max = 4294967295ULL;
    p.number_size = NumberSize::DWord;
    p.N = 42;
    p.filename = L"output.bin";

    for (int i = 1; i < argc; i++) {
        std::wstring arg = argv[i];
        auto getNext = [&]() -> std::wstring {
            if (i + 1 < argc) return argv[++i];
            return L"";
            };

        if (arg == L"--count" || arg == L"-n") p.serie_count = _wcstoui64(getNext().c_str(), nullptr, 10);
        else if (arg == L"--min" || arg == L"-a") p.serie_min = _wcstoui64(getNext().c_str(), nullptr, 10);
        else if (arg == L"--max" || arg == L"-b") p.serie_max = _wcstoui64(getNext().c_str(), nullptr, 10);
        else if (arg == L"--output" || arg == L"-o") p.filename = getNext();
        else if (arg == L"--period" || arg == L"-p") p.N = static_cast<size_t>(_wtoi(getNext().c_str()));
        else if (arg == L"--type" || arg == L"-t") { std::wstring v = getNext(); p.output_double = (v == L"double" || v == L"d"); }
        else if (arg == L"--format" || arg == L"-f") { std::wstring v = getNext(); p.binary_format = (v == L"bin" || v == L"b"); }
        else if (arg == L"--size" || arg == L"-s") {
            std::wstring v = getNext();
            if (v == L"byte" || v == L"8")  p.number_size = NumberSize::Byte;
            else if (v == L"word" || v == L"16") p.number_size = NumberSize::Word;
            else                                   p.number_size = NumberSize::DWord;
        }
        else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            std::wcout << L"Rand++ CLI mode (Cascade PRNG)\n"
                << L"Usage: Rand++.exe [options]\n\n"
                << L"Options:\n"
                << L"  -n, --count N       Number of values (default: 1000000)\n"
                << L"  -a, --min N         Minimum value (default: 0)\n"
                << L"  -b, --max N         Maximum value (default: 4294967295)\n"
                << L"  -o, --output FILE   Output filename (default: output.bin)\n"
                << L"  -t, --type TYPE     int or double (default: int)\n"
                << L"  -f, --format FMT    txt or bin (default: bin)\n"
                << L"  -s, --size SIZE     byte, word, dword (default: dword)\n"
                << L"  -p, --period N      Cascade steps N (default: 5)\n"
                << L"  -h, --help          Show this help\n";
            return 0;
        }
    }

    // Resolve relative paths to executable directory
    if (p.filename.find_first_of(L"\\/:") == std::wstring::npos)
        p.filename = GetExecutableDirectory() + L"\\" + p.filename;

    std::wcout << L"Generating " << p.serie_count << L" values"
        << L" (type=" << (p.output_double ? L"double" : L"int")
        << L", format=" << (p.binary_format ? L"bin" : L"txt")
        << L", size=" << (p.number_size == NumberSize::Byte ? L"byte" :
            p.number_size == NumberSize::Word ? L"word" : L"dword")
        << L", N=" << p.N
        << L") -> " << p.filename << std::endl;

    std::wstring errMsg;
    if (GenerateFile(p, errMsg)) {
        std::wcout << L"Done: " << p.filename << std::endl;
        return 0;
    }

    if (errMsg == L"count")  std::wcerr << L"Error: count must be > 0" << std::endl;
    else if (errMsg == L"minmax")  std::wcerr << L"Error: min must be < max" << std::endl;
    else if (errMsg == L"file")   std::wcerr << L"Error: cannot open file" << std::endl;
    else                          std::wcerr << L"Error: " << errMsg << std::endl;
    return 1;
}

// ── Worker thread (GUI mode) ─────────────────────────────────────
void GenerateThread(ThreadParams* params) {
    HWND hDlg = params->hDlg;

    auto postProgress = [&](size_t processed) {
        if (IsWindow(hDlg))
            PostMessageW(hDlg, WM_UPDATE_PROGRESS,
                static_cast<WPARAM>(processed * 100 / params->serie_count), 0);
        };

    std::wstring errMsg;
    try {
        if (params->serie_count == 0) {
            errMsg = L"count";
            throw errMsg;
        }
        if (params->serie_min >= params->serie_max) {
            errMsg = L"minmax";
            throw errMsg;
        }

        size_t N = params->N;
        if (N < 1) N = 1;

        CascadePRNG rng_perfect(
            static_cast<unsigned int>(time(nullptr)), N);

        // Период
        size_t period = rng_perfect.get_period();
        if (IsWindow(hDlg))
            SetDlgItemTextW(hDlg, IDC_STATIC_PERIOD,
                std::to_wstring(period).c_str());

        size_t processed = 0;
        const size_t updateInterval = params->serie_count / 100;

        if (params->binary_format) {
            std::ofstream file(params->filename, std::ios::binary);
            if (!file.is_open()) { errMsg = L"file"; throw errMsg; }

            if (params->output_double) {
                for (size_t i = 0; i < params->serie_count; ++i) {
                    double r = rng_perfect.generate();
                    r = params->serie_min +
                        r * (params->serie_max - params->serie_min);
                    file.write(reinterpret_cast<const char*>(&r),
                        sizeof(r));
                    if (++processed % updateInterval == 0)
                        postProgress(processed);
                }
            }
            else {
                for (size_t i = 0; i < params->serie_count; ++i) {
                    double r = rng_perfect.generate();
                    r = params->serie_min +
                        r * (params->serie_max - params->serie_min);
                    switch (params->number_size) {
                    case NumberSize::Byte: {
                        uint8_t v = static_cast<uint8_t>(r);
                        file.write(reinterpret_cast<const char*>(&v),
                            sizeof(v));
                        break;
                    }
                    case NumberSize::Word: {
                        uint16_t v = static_cast<uint16_t>(r);
                        file.write(reinterpret_cast<const char*>(&v),
                            sizeof(v));
                        break;
                    }
                    case NumberSize::DWord: {
                        uint32_t v = static_cast<uint32_t>(r);
                        file.write(reinterpret_cast<const char*>(&v),
                            sizeof(v));
                        break;
                    }
                    }
                    if (++processed % updateInterval == 0)
                        postProgress(processed);
                }
            }
        }
        else {
            std::ofstream file(params->filename);
            if (!file.is_open()) { errMsg = L"file"; throw errMsg; }
            if (params->output_double)
                file << std::fixed << std::setprecision(15);

            for (size_t i = 0; i < params->serie_count; ++i) {
                double r = rng_perfect.generate();
                r = params->serie_min +
                    r * (params->serie_max - params->serie_min);

                if (params->output_double) {
                    file << r;
                }
                else {
                    uint32_t value = static_cast<uint32_t>(r);
                    switch (params->number_size) {
                    case NumberSize::Byte:
                        file << static_cast<uint8_t>(value); break;
                    case NumberSize::Word:
                        file << static_cast<uint16_t>(value); break;
                    case NumberSize::DWord:
                        file << value; break;
                    }
                }
                if (i < params->serie_count - 1) file << "\n";
                if (++processed % updateInterval == 0)
                    postProgress(processed);
            }
        }
    }
    catch (const std::wstring& e) {
        if (IsWindow(hDlg)) {
            std::wstring* pMsg = new std::wstring(LoadStr(
                e == L"count" ? IDS_MIN_SERIE_COUNT :
                e == L"minmax" ? IDS_MIN_MAX_CONDITION :
                IDS_FILE_ERROR));
            PostMessageW(hDlg, WM_TASK_ERROR, 0,
                reinterpret_cast<LPARAM>(pMsg));
        }
        delete params;
        return;
    }

    if (IsWindow(hDlg))
        PostMessageW(hDlg, WM_TASK_COMPLETE, 0, 0);
    delete params;
}

// ── About dialog ─────────────────────────────────────────────────
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG:
        return TRUE;
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
INT_PTR CALLBACK RandDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
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

        SendDlgItemMessageW(hDlg, IDC_SERIES_PERIOD, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"5"));

        SetDlgItemTextW(hDlg, IDC_SERIE_MAX, L"0");
        SetDlgItemTextW(hDlg, IDC_SERIE_MIN, L"0");
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
            ThreadParams* p = new ThreadParams;
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
            size_t N = static_cast<size_t>(_wtoi(buf));
            if (N < 1) N = 1;
            p->N = N;

            p->output_double = IsDlgButtonChecked(hDlg, IDC_TYPE_DOUBLE) == BST_CHECKED;
            p->binary_format = IsDlgButtonChecked(hDlg, IDC_TYPE_BINARY) == BST_CHECKED;

            if (IsDlgButtonChecked(hDlg, IDC_TYPE_BYTE) == BST_CHECKED)
                p->number_size = NumberSize::Byte;
            else if (IsDlgButtonChecked(hDlg, IDC_TYPE_WORD) == BST_CHECKED)
                p->number_size = NumberSize::Word;
            else
                p->number_size = NumberSize::DWord;

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

// ── Entry point ──────────────────────────────────────────────────
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int) {
    g_hInstance = hInstance;

    // CLI mode: if command line has arguments, run without GUI
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argv && argc > 1) {
        int ret = RunCLI(argc, argv);
        LocalFree(argv);
        return ret;
    }
    if (argv) LocalFree(argv);

    // GUI mode
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    DialogBoxW(hInstance, MAKEINTRESOURCEW(IDD_RAND_DIALOG), nullptr, RandDlgProc);
    return 0;
}
