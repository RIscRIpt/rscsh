#include "Application.h"

#include "resource.h"

#include <vector>
#include <system_error>
#include <cwchar>

static INT_PTR CALLBACK main_dialog_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Application *app;
    if (uMsg == WM_INITDIALOG) {
        SetWindowLongPtr(hwndDlg, DWLP_USER, lParam);
        app = reinterpret_cast<Application*>(lParam);
    } else {
        app = reinterpret_cast<Application*>(GetWindowLongPtr(hwndDlg, DWLP_USER));
    }
    return app->main_dialog_proc(hwndDlg, uMsg, wParam, lParam);
}

static INT_PTR CALLBACK input_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    return app->input_proc(hwnd, uMsg, wParam, lParam);
}


Application::Application(HINSTANCE hInstance)
    : hInstance_(hInstance)
    , hOutputFont_(NULL)
    , hInputFont_(NULL)
    , hMainDialog_(NULL)
    , hOutput_(NULL)
    , hInput_(NULL)
    , origInputProc_(NULL)
    , fontSize_(9)
{
    create_main_dialog();

    log(L"rscsh v0.1\r\n");
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

INT_PTR Application::main_dialog_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_SIZE:
        case WM_SIZING:
            update_main_dialog_layout();
            return TRUE;

        case WM_MOUSEWHEEL:
            if (LOWORD(wParam) & MK_CONTROL) {
                change_font_size(static_cast<SHORT>(HIWORD(wParam)) >= 0 ? +1 : -1);
                return TRUE;
            }
            break;

        case WM_INITDIALOG:
            hMainDialog_ = hwndDlg;
            initialize_main_dialog();
            return TRUE;

        case WM_CLOSE:
            DestroyWindow(hMainDialog_);
            return TRUE;

        case WM_DESTROY:
            PostQuitMessage(0);
            return TRUE;
    }
    return FALSE;
}

INT_PTR Application::input_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CHAR:
            if (input_proc_char(wParam, lParam))
                return FALSE;
    }
    return CallWindowProc(origInputProc_, hwnd, uMsg, wParam, lParam);
}

void Application::create_main_dialog() {
    if (CreateDialogParam(hInstance_, MAKEINTRESOURCE(IDD_MAINDIALOG), NULL, ::main_dialog_proc, (LPARAM)this) == NULL)
        throw std::system_error(GetLastError(), std::system_category());
}

void Application::initialize_main_dialog() {
    hInput_ = GetDlgItem(hMainDialog_, IDC_INPUT);
    hOutput_ = GetDlgItem(hMainDialog_, IDC_OUTPUT);

    SetWindowLongPtr(hInput_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    origInputProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hInput_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(::input_proc)));

    change_font_size(0);
}

void Application::update_main_dialog_layout() {
    const int inputHeight = fontSize_ + 8;
    RECT rcDialog;

    GetClientRect(hMainDialog_, &rcDialog);

    SetWindowPos(hOutput_, NULL, 0, 0, rcDialog.right - rcDialog.left, rcDialog.bottom - rcDialog.top - inputHeight, SWP_NOZORDER);
    SetWindowPos(hInput_, NULL, 0, rcDialog.bottom - rcDialog.top - inputHeight, rcDialog.right - rcDialog.left, inputHeight, SWP_NOZORDER);
}

bool Application::input_proc_char(WPARAM wParam, LPARAM lParam) {
    switch (wParam) {
        case VK_RETURN:
            return true;
    }
    return false;
}

HFONT Application::create_font(int size) {
    return CreateFont(
        size,
        0,
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

}

void Application::change_font_size(int delta) {
    fontSize_ += delta;
    if (fontSize_ < 9)
        fontSize_ = 9;
    else if (fontSize_ > 36)
        fontSize_ = 36;

    if (hOutputFont_)
        DeleteObject(hOutputFont_);
    if (hInputFont_)
        DeleteObject(hInputFont_);

    hInputFont_ = create_font(fontSize_);
    hOutputFont_ = create_font(fontSize_);

    SendMessage(hInput_, WM_SETFONT, reinterpret_cast<WPARAM>(hInputFont_), TRUE);
    SendMessage(hOutput_, WM_SETFONT, reinterpret_cast<WPARAM>(hOutputFont_), TRUE);

    update_main_dialog_layout();
}

void Application::log(char const *message) {
    DWORD selStart, selEnd;

    SendMessage(hOutput_, EM_GETSEL, reinterpret_cast<WPARAM>(&selStart), reinterpret_cast<WPARAM>(&selEnd));

    int length = GetWindowTextLength(hOutput_);
    SendMessage(hOutput_, EM_SETSEL, length, length);
    SendMessageA(hOutput_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(message));
    SendMessage(hOutput_, EM_SETSEL, selStart, selEnd);
}

void Application::log(wchar_t const *message) {
    DWORD selStart, selEnd;

    SendMessage(hOutput_, EM_GETSEL, reinterpret_cast<WPARAM>(&selStart), reinterpret_cast<WPARAM>(&selEnd));

    int length = GetWindowTextLength(hOutput_);
    SendMessage(hOutput_, EM_SETSEL, length, length);
    SendMessageW(hOutput_, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(message));
    SendMessage(hOutput_, EM_SETSEL, selStart, selEnd);
}

void Application::logf(char const *fmt, ...) {
    va_list vl;
    std::vector<char> buffer(4096);

    va_start(vl, fmt);
    vsprintf(buffer.data(), fmt, vl);
    va_end(vl);

    log(buffer.data());
}

void Application::logf(wchar_t const *fmt, ...) {
    va_list vl;
    std::vector<wchar_t> buffer(2048);

    va_start(vl, fmt);
    vswprintf(buffer.data(), fmt, vl);
    va_end(vl);

    log(buffer.data());
}
