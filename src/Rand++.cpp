
// Rand++.cpp: определяет поведение классов для приложения.
//

#include "pch.h"
#include "framework.h"
#include "Rand++.h"


// Rand++.cpp — WinAPI version (no MFC)
#define APPLICATION
#include "randpp.h"

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <fstream>
#include <iomanip>
#include <thread>
#include <cstdint>
#include <ctime>

#include "resource.h"

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
    double time_period;
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

// ── Worker thread ────────────────────────────────────────────────
void GenerateThread(ThreadParams* params) {
    HWND hDlg = params->hDlg;

    auto postProgress = [&](size_t processed) {
        if (IsWindow(hDlg))
            PostMessageW(hDlg, WM_UPDATE_PROGRESS,
                static_cast<WPARAM>(processed * 100 / params->serie_count), 0);
        };

    auto postError = [&](UINT stringID) {
        if (!IsWindow(hDlg)) return;
        std::wstring* pMsg = new std::wstring(LoadStr(stringID));
        PostMessageW(hDlg, WM_TASK_ERROR, 0, reinterpret_cast<LPARAM>(pMsg));
        };

    try {
        // ── Validation ──
        if (params->serie_count == 0)
            throw std::runtime_error("count");
        if (params->serie_min >= params->serie_max)
            throw std::runtime_error("minmax");

        // ── Initialize RNG ──
        RNG rng_perfect_var(static_cast<unsigned int>(time(nullptr)),
            params->time_period);
        RNGAbstract* rng_perfect = &rng_perfect_var;
        size_t period = rng_perfect->get_period();

        // Update period display
        if (IsWindow(hDlg))
            SetDlgItemTextW(hDlg, IDC_STATIC_PERIOD, std::to_wstring(period).c_str());

        size_t processed = 0;
        const size_t updateInterval = params->serie_count / 100;

        // ── Binary output ──
        if (params->binary_format) {
            std::ofstream file(params->filename.c_str(), std::ios::binary);
            if (!file.is_open())
                throw std::runtime_error("file");

            if (params->output_double) {
                for (size_t i = 0; i < params->serie_count; ++i) {
                    double r = rng_perfect->generate(static_cast<size_t>(params->number_size));
                    r = params->serie_min + r * (params->serie_max - params->serie_min);
                    file.write(reinterpret_cast<const char*>(&r), sizeof(r));
                    processed++;
                    if (updateInterval > 0 && processed % updateInterval == 0)
                        postProgress(processed);
                }
            }
            else {
                for (size_t i = 0; i < params->serie_count; ++i) {
                    double r = rng_perfect->generate(static_cast<size_t>(params->number_size));
                    r = params->serie_min + r * (params->serie_max - params->serie_min);
                    switch (params->number_size) {
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
                    processed++;
                    if (updateInterval > 0 && processed % updateInterval == 0)
                        postProgress(processed);
                }
            }
            file.close();
        }
        // ── Text output ──
        else {
            std::ofstream file(params->filename.c_str());
            if (!file.is_open())
                throw std::runtime_error("file");

            if (params->output_double)
                file << std::fixed << std::setprecision(15);

            for (size_t i = 0; i < params->serie_count; ++i) {
                double r = rng_perfect->generate(static_cast<size_t>(params->number_size));
                r = params->serie_min + r * (params->serie_max - params->serie_min);

                if (params->output_double) {
                    file << r;
                }
                else {
                    uint32_t value = static_cast<uint32_t>(r);
                    switch (params->number_size) {
                    case NumberSize::Byte:  file << static_cast<uint8_t>(value);  break;
                    case NumberSize::Word:  file << static_cast<uint16_t>(value); break;
                    case NumberSize::DWord: file << value; break;
                    }
                }
                processed++;
                if (updateInterval > 0 && processed % updateInterval == 0)
                    postProgress(processed);
                if (i < params->serie_count - 1)
                    file << "\n";
            }
            file.close();
        }
    }
    catch (const std::runtime_error& e) {
        std::string what(e.what());
        if (what == "count")  postError(IDS_MIN_SERIE_COUNT);
        else if (what == "minmax") postError(IDS_MIN_MAX_CONDITION);
        else if (what == "file")   postError(IDS_FILE_ERROR);
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

        // ── Init ──
    case WM_INITDIALOG: {
        hIcon = LoadIconW(g_hInstance, MAKEINTRESOURCEW(IDR_MAINFRAME));
        if (hIcon) {
            SendMessageW(hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
            SendMessageW(hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
        }

        // System menu — "About"
        HMENU hSysMenu = GetSystemMenu(hDlg, FALSE);
        if (hSysMenu) {
            AppendMenuW(hSysMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hSysMenu, MF_STRING, IDM_ABOUTBOX, LoadStr(IDS_ABOUTBOX).c_str());
        }

        // Combo boxes
        const wchar_t* counts[] = {
            L"10", L"100", L"1000", L"10000",
            L"134217728", L"268435456", L"536870912", L"1668467902"
        };
        for (auto s : counts)
            SendDlgItemMessageW(hDlg, IDC_SERIES_COUNT, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));

        const wchar_t* maxes[] = { L"256", L"65536", L"4294967296" };
        for (auto s : maxes)
            SendDlgItemMessageW(hDlg, IDC_SERIE_MAX, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));

        SendDlgItemMessageW(hDlg, IDC_SERIES_PERIOD, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"73.8"));

        SetDlgItemTextW(hDlg, IDC_SERIE_MAX, L"0");
        SetDlgItemTextW(hDlg, IDC_SERIE_MIN, L"0");
        return TRUE;
    }

                      // ── System menu ──
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == IDM_ABOUTBOX) {
            DialogBoxW(g_hInstance, MAKEINTRESOURCEW(IDD_ABOUTBOX), hDlg, AboutDlgProc);
            return TRUE;
        }
        break;

        // ── F1 help ──
    case WM_HELP:
        DialogBoxW(g_hInstance, MAKEINTRESOURCEW(IDD_ABOUTBOX), hDlg, AboutDlgProc);
        return TRUE;

        // ── Commands ──
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
            p->time_period = _wtof(buf);

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

        // ── Custom messages ──
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

                      // ── Icon painting (minimized) ──
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
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    g_hInstance = hInstance;

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    DialogBoxW(hInstance, MAKEINTRESOURCEW(IDD_RAND_DIALOG), nullptr, RandDlgProc);
    return 0;
}
