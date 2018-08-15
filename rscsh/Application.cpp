#include "Application.h"

#include "resource.h"

#include <vector>
#include <system_error>
#include <cwchar>

#include "version.ver"

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

wchar_t const *Application::APP_NAME = L"rscsh";
wchar_t const *Application::APP_VERSION = L"" VERSION;

int const Application::MIN_FONT_SIZE = 12;
int const Application::DEF_FONT_SIZE = 14;
int const Application::MAX_FONT_SIZE = 36;

Application::Application(HINSTANCE hInstance)
    : hInstance_(hInstance)
    , hOutputFont_(NULL)
    , hInputFont_(NULL)
    , hMainDialog_(NULL)
    , hOutput_(NULL)
    , hInput_(NULL)
    , origInputProc_(NULL)
    , fontSize_(DEF_FONT_SIZE)
    , input_ctrl_pressed_(false)
    , rscEventListener_()
    , shell_(shell_log_, std::bind(&Application::shell_done, this))
{
    create_main_dialog();

    logf(L"%s %s\r\n", APP_NAME, APP_VERSION);
    set_title(L"Disconnected");

    shell_.cardShell().set_context(rscEventListener_.context());
    rscEventListener_.listen_new_readers(true);

    using namespace std::placeholders;
    rscEventListener_.start(SCARD_STATE_EMPTY | SCARD_STATE_PRESENT, std::bind(&Application::rsc_event, this, _1, _2, _3));
}

Application::~Application() {
    if (hOutputFont_)
        DeleteObject(hOutputFont_);
    if (hInputFont_)
        DeleteObject(hInputFont_);
    rscEventListener_.stop();
}

int Application::run() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) != 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
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
            break;
        case WM_KEYDOWN:
            if (input_proc_keydown(wParam, lParam))
                return FALSE;
            break;
        case WM_KEYUP:
            if (input_proc_keyup(wParam, lParam))
                return FALSE;
            break;
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

    if ((hIcon_ = reinterpret_cast<HICON>(LoadImage(hInstance_, MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0))) == NULL)
        throw std::system_error(GetLastError(), std::generic_category());

    SendMessage(hMainDialog_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon_));
}

void Application::update_main_dialog_layout() {
    const int inputHeight = fontSize_ + 8;
    RECT rcDialog;

    GetClientRect(hMainDialog_, &rcDialog);

    SetWindowPos(hOutput_, NULL, 0, 0, rcDialog.right - rcDialog.left, rcDialog.bottom - rcDialog.top - inputHeight, SWP_NOZORDER);
    SetWindowPos(hInput_, NULL, 0, rcDialog.bottom - rcDialog.top - inputHeight, rcDialog.right - rcDialog.left, inputHeight, SWP_NOZORDER);
}

void Application::set_title(std::wstring const &title) {
    std::wstring fullTitle = std::wstring(APP_NAME) + L" | " + title;
    SetWindowText(hMainDialog_, fullTitle.c_str());
}

void Application::shell_done() {
}

void Application::rsc_event(DWORD event, rsc::Context const &context, std::wstring const &reader) {
    try {
        if (event & SCARD_STATE_PRESENT) {
            logf(L"Smart card was connected to the reader \"%s\"\r\n", reader.c_str());
            if (!shell_.cardShell().has_card()) {
                logf(L"Connecting ...\r\n");
                Sleep(1000);
                shell_.cardShell().create_card_and_connect(reader.c_str());
                shell_.cardShell().print_connection_info();
                log_shell();
                set_title(L"Connected to " + reader);
            } else {
                logf(L"Not connecting because other connection outstanding.\r\n");
            }
        } else if (event & SCARD_STATE_EMPTY) {
            logf(L"Smart card was disconnected from the reader \"%s\"\r\n", reader.c_str());
            if (shell_.cardShell().has_card()) {
                if (shell_.cardShell().card().belongs_to(reader)) {
                    shell_.cardShell().reset_card();
                    set_title(L"Disconnected");
                }
            }
        }
    } catch (std::exception const &e) {
        logf("Error: %s\r\n", e.what());
    }
}

bool Application::input_proc_char(WPARAM wParam, LPARAM lParam) {
    switch (wParam) {
        case VK_RETURN:
            parse_input();
            return true;
        case VK_LBUTTON:
            return true;
    }
    return false;
}

bool Application::input_proc_keydown(WPARAM wParam, LPARAM lParam) {
    switch (wParam) {
        case VK_CONTROL:
            input_ctrl_pressed_ = true;
            return true;
        case 'A':
            if (input_ctrl_pressed_) {
                SendMessage(hInput_, EM_SETSEL, 0, -1);
                return true;
            }
            break;
    }
    return false;
}

bool Application::input_proc_keyup(WPARAM wParam, LPARAM lParam) {
    switch (wParam) {
        case VK_CONTROL:
            input_ctrl_pressed_ = false;
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
        CLEARTYPE_NATURAL_QUALITY,
        CF_FIXEDPITCHONLY,
        TEXT("Consolas")
    );

}

void Application::change_font_size(int delta) {
    fontSize_ += delta;
    if (fontSize_ < MIN_FONT_SIZE)
        fontSize_ = MIN_FONT_SIZE;
    else if (fontSize_ > MAX_FONT_SIZE)
        fontSize_ = MAX_FONT_SIZE;

    if (hOutputFont_)
        DeleteObject(hOutputFont_);
    if (hInputFont_)
        DeleteObject(hInputFont_);

    hInputFont_ = create_font(fontSize_);
    hOutputFont_ = create_font(fontSize_);

    SendMessage(hInput_, WM_SETFONT, reinterpret_cast<WPARAM>(hInputFont_), TRUE);
    SendMessage(hOutput_, WM_SETFONT, reinterpret_cast<WPARAM>(hOutputFont_), TRUE);

    update_main_dialog_layout();

    /*
    auto currentStyle = GetWindowLongPtr(hInput_, GWL_STYLE);
    if (fontSize_ == MIN_FONT_SIZE) {
        SetWindowLongPtr(hInput_, GWL_STYLE, currentStyle | ES_UPPERCASE);
    } else {
        SetWindowLongPtr(hInput_, GWL_STYLE, currentStyle & ~ES_UPPERCASE);
    }
    */
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
    vsprintf_s(buffer.data(), buffer.size(), fmt, vl);
    va_end(vl);

    log(buffer.data());
}

void Application::logf(wchar_t const *fmt, ...) {
    va_list vl;
    std::vector<wchar_t> buffer(2048);

    va_start(vl, fmt);
    vswprintf(buffer.data(), buffer.size(), fmt, vl);
    va_end(vl);

    log(buffer.data());
}

void Application::log_shell() {
    log(shell_log_.str().c_str());
    clear_shell_log();
}

void Application::clear_shell_log() {
    shell_log_.str(std::wstring());
    shell_log_.clear();
}

void Application::parse_input() {
    std::vector<wchar_t> buffer(2048);
    GetWindowText(hInput_, buffer.data(), static_cast<int>(buffer.size()));
    SetWindowText(hInput_, L"");
    shell_execute(buffer.data());
}

void Application::shell_execute(LPCTSTR command) {
    try {
        shell_.execute(command);
        log_shell();
    } catch (std::system_error const &e) {
        logf("Error: %s\r\n", e.what());
    } catch (std::exception const &e) {
        logf("Error: %s\r\n", e.what());
    }
}
