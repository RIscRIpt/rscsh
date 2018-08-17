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
int const Application::MAX_FONT_SIZE = 72;

Application::Application(HINSTANCE hInstance)
    : hInstance_(hInstance)
    , hFont_(NULL)
    , hMainDialog_(NULL)
    , hOutput_(NULL)
    , hInput_(NULL)
    , hSymbols_(NULL)
    , hIcon_(NULL)
    , origInputProc_(NULL)
    , fontSize_(DEF_FONT_SIZE)
    , input_ctrl_pressed_(false)
    , selectedInput_(inputHistory_.end())
    , shell_(shell_log_, std::bind(&Application::shell_done, this))
{
    create_main_dialog();

    logf(L"%s %s\r\n", APP_NAME, APP_VERSION);
    set_title(L"Disconnected");
    set_symbols(0, 0);

    shell_.card_shell().set_context(rscEventListener_.context());
    rscEventListener_.listen_new_readers(true);

    using namespace std::placeholders;
    rscEventListener_.start(SCARD_STATE_EMPTY | SCARD_STATE_PRESENT, std::bind(&Application::rsc_event, this, _1, _2, _3));
    shell_.card_shell().set_on_connection_changed_callback(std::bind(&Application::card_shell_connection_changed, this, _1));
}

Application::~Application() {
    if (hFont_)
        DeleteObject(hFont_);
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
        case WM_GETTEXTLENGTH:
            input_proc_text_changed();
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
    hSymbols_ = GetDlgItem(hMainDialog_, IDC_SYMBOLS);

    SetWindowLongPtr(hInput_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    origInputProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hInput_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(::input_proc)));

    change_font_size(0);

    if ((hIcon_ = reinterpret_cast<HICON>(LoadImage(hInstance_, MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0))) == NULL)
        throw std::system_error(GetLastError(), std::generic_category());

    SendMessage(hMainDialog_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon_));
}

void Application::update_main_dialog_layout() {
    const int inputHeight = fontSize_ + 8;
    const int symbolsWidth = inputHeight * 3;

    RECT rcDialog;

    GetClientRect(hMainDialog_, &rcDialog);

    SetWindowPos(hOutput_, NULL, 0, 0, rcDialog.right - rcDialog.left, rcDialog.bottom - rcDialog.top - inputHeight, SWP_NOZORDER);
    SetWindowPos(hInput_, NULL, 0, rcDialog.bottom - rcDialog.top - inputHeight, rcDialog.right - rcDialog.left - symbolsWidth, inputHeight, SWP_NOZORDER);
    SetWindowPos(hSymbols_, NULL, rcDialog.right - symbolsWidth, rcDialog.bottom - rcDialog.top - inputHeight, symbolsWidth, inputHeight, SWP_NOZORDER);
}

void Application::set_title(std::wstring const &title) {
    std::wstring fullTitle = std::wstring(APP_NAME) + L" | " + title;
    SetWindowText(hMainDialog_, fullTitle.c_str());
}

void Application::set_symbols(size_t total, size_t noSpace) {
    std::wstringstream ss;
    ss << total << '/' << noSpace;
    SetWindowText(hSymbols_, ss.str().c_str());
}

void Application::shell_done() {
}

void Application::rsc_event(DWORD event, rsc::Context const &context, std::wstring const &reader) {
    try {
        if (event & SCARD_STATE_PRESENT) {
            logf(L"Smart card was connected to the reader \"%s\"\r\n", reader.c_str());
            if (!shell_.card_shell().has_card()) {
                logf(L"Connecting ...\r\n");
                Sleep(1000);
                shell_.card_shell().create_card_and_connect(reader.c_str());
                shell_.card_shell().print_connection_info();
                log_shell();
            } else {
                logf(L"Not connecting because other connection outstanding.\r\n");
            }
        } else if (event & SCARD_STATE_EMPTY) {
            logf(L"Smart card was disconnected from the reader \"%s\"\r\n", reader.c_str());
            if (shell_.card_shell().has_card()) {
                if (shell_.card_shell().card().belongs_to(reader)) {
                    shell_.card_shell().reset_card();
                }
            }
        }
    } catch (std::exception const &e) {
        logf("Error: %s\r\n", e.what());
    }
}

void Application::card_shell_connection_changed(std::wstring const &reader) {
    if (!reader.empty()) {
        set_title(L"Connected to " + reader);
    } else {
        set_title(L"Disconnected");
    }
}

bool Application::input_proc_char(WPARAM wParam, LPARAM lParam) {
    switch (wParam) {
        case VK_RETURN:
            process_input();
            return true;
        case VK_LBUTTON:
            return true;
        case 127: // DEL
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
        case 'W':
        case VK_BACK:
            if (input_ctrl_pressed_) {
                erase_last_input_word();
                return true;
            }
            break;
        case VK_DOWN:
            select_input_history_entry(+1);
            return true;
        case VK_UP:
            select_input_history_entry(-1);
            return true;
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

void Application::input_proc_text_changed() {
    std::vector<wchar_t> buffer(2048);

    auto actualSize = GetWindowText(hInput_, buffer.data(), static_cast<int>(buffer.size()));
    buffer.resize(actualSize + 1);

    size_t total = buffer.size() - 1;
    size_t noSpace = total;
    if (auto it = std::find(buffer.rbegin(), buffer.rend(), ' '); it != buffer.rend()) {
        noSpace = buffer.end() - it.base() - 1;
    }

    set_symbols(total, noSpace);
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

    if (hFont_)
        DeleteObject(hFont_);

    hFont_ = create_font(fontSize_);

    SendMessage(hInput_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont_), TRUE);
    SendMessage(hOutput_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont_), TRUE);
    SendMessage(hSymbols_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont_), TRUE);

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

void Application::process_input() {
    std::vector<wchar_t> buffer(2048);

    auto actualLength = GetWindowText(hInput_, buffer.data(), static_cast<int>(buffer.size()));
    buffer.resize(actualLength + 1);

    SetWindowText(hInput_, L"");

    if (buffer.size() > 1 && (inputHistory_.empty() || inputHistory_.back() != buffer.data())) {
        inputHistory_.emplace_back(buffer.data());
        selectedInput_ = inputHistory_.end();
    }

    shell_execute(buffer.data());
}

void Application::shell_execute(LPCTSTR command) {
    try {
        shell_.execute(command);
        log_shell();
    } catch (std::system_error const &e) {
        log_shell();
        logf("Error: %s\r\n", e.what());
    } catch (std::exception const &e) {
        log_shell();
        logf("Error: %s\r\n", e.what());
    }
}

void Application::select_input_history_entry(int offset) {
    if (inputHistory_.empty())
        return;

    selectedInput_ += offset;

    while (selectedInput_ < inputHistory_.begin())
        selectedInput_ += inputHistory_.end() - inputHistory_.begin() + 1;
    while (selectedInput_ > inputHistory_.end())
        selectedInput_ -= inputHistory_.end() - inputHistory_.begin() + 1;

    if (selectedInput_ != inputHistory_.end()) {
        SetWindowText(hInput_, selectedInput_->c_str());
        SendMessage(hInput_, EM_SETSEL, selectedInput_->size(), selectedInput_->size());
    } else {
        SetWindowText(hInput_, L"");
    }
}

void Application::erase_last_input_word() {
    std::vector<wchar_t> buffer(2048);

    auto actualLength = GetWindowText(hInput_, buffer.data(), static_cast<int>(buffer.size()));
    if (actualLength == 0)
        return;

    buffer.resize(actualLength);

    std::vector<wchar_t>::reverse_iterator it = buffer.rbegin();

    // Skip trailing spaces
    while (it != buffer.rend() && *it == L' ')
        ++it;

    // Erase last word
    while (it != buffer.rend() && *it != L' ')
        ++it;

    --it; // Keep space

    *it = L'\0';
    SetWindowText(hInput_, buffer.data());
    DWORD end = it.base() - buffer.begin();
    SendMessage(hInput_, EM_SETSEL, end, end);
}
