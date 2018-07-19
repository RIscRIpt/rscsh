#include "Application.h"

#include "resource.h"

#include <system_error>

static INT_PTR CALLBACK main_dialog_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Application *app;
    if (uMsg == WM_INITDIALOG) {
        SetWindowLongPtr(hwndDlg, DWLP_USER, lParam);
        app = reinterpret_cast<Application*>(lParam);
    } else {
        app = reinterpret_cast<Application*>(GetWindowLongPtr(hwndDlg, DWLP_USER));
    }
    return app->dialog_proc(hwndDlg, uMsg, wParam, lParam);
}


Application::Application(HINSTANCE hInstance)
    : hInstance_(hInstance)
    , hOutputFont_(NULL)
    , hInputFont_(NULL)
    , hMainDialog_(NULL)
    , hOutput_(NULL)
    , hInput_(NULL)
{
    create_main_dialog();
}

Application::~Application() {
    if (hOutputFont_)
        DeleteObject(hOutputFont_);
    if (hInputFont_)
        DeleteObject(hInputFont_);
}

int Application::run() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) != 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}

INT_PTR Application::dialog_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_SIZE:
        case WM_SIZING:
            update_main_dialog_layout();
            break;

        case WM_INITDIALOG:
            hMainDialog_ = hwndDlg;
            initialize_main_dialog();
            break;

        case WM_CLOSE:
            DestroyWindow(hMainDialog_);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return FALSE;
    }
    return TRUE;
}

void Application::create_main_dialog() {
    if (CreateDialogParam(hInstance_, MAKEINTRESOURCE(IDD_MAINDIALOG), NULL, main_dialog_proc, (LPARAM)this) == NULL)
        throw std::system_error(GetLastError(), std::system_category());
}

void Application::initialize_fonts() {
    HFONT font = CreateFont(
        9,
        6,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        CF_FIXEDPITCHONLY,
        TEXT("Terminal")
    );

    hInputFont_ = font;
    hOutputFont_ = font;
}

void Application::initialize_main_dialog() {
    hInput_ = GetDlgItem(hMainDialog_, IDC_INPUT);
    hOutput_ = GetDlgItem(hMainDialog_, IDC_OUTPUT);

    initialize_fonts();

    SendMessage(hInput_, WM_SETFONT, reinterpret_cast<WPARAM>(hInputFont_), TRUE);
    SendMessage(hOutput_, WM_SETFONT, reinterpret_cast<WPARAM>(hOutputFont_), TRUE);
}

void Application::update_main_dialog_layout() {
    const int inputHeight = 20;
    RECT rcDialog;

    GetClientRect(hMainDialog_, &rcDialog);

    SetWindowPos(hOutput_, NULL, 0, 0, rcDialog.right - rcDialog.left, rcDialog.bottom - rcDialog.top - inputHeight, SWP_NOZORDER);
    SetWindowPos(hInput_, NULL, 0, rcDialog.bottom - rcDialog.top - inputHeight, rcDialog.right - rcDialog.left, inputHeight, SWP_NOZORDER);
}
