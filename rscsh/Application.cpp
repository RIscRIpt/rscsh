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

char const *Application::APP_NAME = "rscsh v0.1";

int const Application::MIN_FONT_SIZE = 9;
int const Application::MAX_FONT_SIZE = 36;

Application::Application(HINSTANCE hInstance)
    : hInstance_(hInstance)
    , hOutputFont_(NULL)
    , hInputFont_(NULL)
    , hMainDialog_(NULL)
    , hOutput_(NULL)
    , hInput_(NULL)
    , origInputProc_(NULL)
    , fontSize_(MIN_FONT_SIZE)
    , shell_(&shell_log_)
{
    create_main_dialog();

    logf("%s\r\n", APP_NAME);

    initialize_shell();
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

void Application::initialize_shell() {
    try {
        shell_.create_context(SCARD_SCOPE_USER);
        shell_.create_readers(SCARD_DEFAULT_READERS);
        if (shell_.readers().list().size() == 0) {
            log("No smart card readers are present in the system.\r\n");
        } else if (shell_.readers().list().size() == 1) {
            log("Only one reader present in the system.\r\n");
            logf(L"Choosing reader \"%s\".\r\n", shell_.readers().list().front().c_str());
            shell_.create_card(shell_.readers().list().front().c_str());
        } else {
            log("Multiple readers present in the system:\r\n");
            for (auto const &reader : shell_.readers().list()) {
                logf(L"    - \"%s\"\r\n", reader);
            }
            log("\r\n");
        }
    } catch (std::system_error const &e) {
        if (e.code().value() == SCARD_E_NO_READERS_AVAILABLE) {
            log("No smart card readers are present in the system.\r\n");
        } else {
            log("Failed to initialize shell.\r\n");
            logf("Error: %s\r\n", e.what());
        }
    }
    if (shell_.has_card()) {
        try {
            shell_.card().connect();
            shell_log_ << "ATR: ";
            shell_.card().atr().print(shell_log_, L" ");
            shell_log_ << "\r\n";
            log_shell();
        } catch (std::system_error const &e) {
            if (e.code().value() == SCARD_W_REMOVED_CARD) {
                log("No card present in the reader.\r\n");
            } else {
                log("Unexpected error occured while trying to connect to the card.\r\n");
                logf("Error: %s\r\n", e.what());
            }
        }
    }
}

bool Application::input_proc_char(WPARAM wParam, LPARAM lParam) {
    switch (wParam) {
        case VK_RETURN:
            parse_input();
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

    auto currentStyle = GetWindowLongPtr(hInput_, GWL_STYLE);
    if (fontSize_ == MIN_FONT_SIZE) {
        SetWindowLongPtr(hInput_, GWL_STYLE, currentStyle | ES_UPPERCASE);
    } else {
        SetWindowLongPtr(hInput_, GWL_STYLE, currentStyle & ~ES_UPPERCASE);
    }
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
    GetWindowText(hInput_, buffer.data(), buffer.size());
    SetWindowText(hInput_, L"");
    try {
        shell_.execute(buffer.data());
        log_shell();
    } catch (std::system_error const &e) {
        logf("Error: %s\r\n", e.what());
        switch (e.code().value()) {
        }
    } catch (std::exception const &e) {
        logf("Error: %s\r\n", e.what());
    }
}
