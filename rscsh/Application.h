#pragma once

#include "Shell.h"

#include <Windows.h>

class Application {
public:
    Application(HINSTANCE hInstance);
    ~Application();

    int run();

    INT_PTR CALLBACK main_dialog_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    INT_PTR CALLBACK input_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static char const *APP_NAME;
    
    static int const MIN_FONT_SIZE;
    static int const MAX_FONT_SIZE;

private:
    void create_main_dialog();
    void initialize_main_dialog();
    void update_main_dialog_layout();

    void initialize_shell();

    bool input_proc_char(WPARAM wParam, LPARAM lParam);

    HFONT create_font(int size);
    void change_font_size(int delta);

    void log(char const *message);
    void log(wchar_t const *message);

    void logf(char const *fmt, ...);
    void logf(wchar_t const *fmt, ...);

    void log_shell();
    void clear_shell_log();
    
    void parse_input();

    HINSTANCE hInstance_;

    HFONT hOutputFont_;
    HFONT hInputFont_;

    HWND hMainDialog_;
    HWND hOutput_;
    HWND hInput_;

    WNDPROC origInputProc_;

    int fontSize_;

    Shell shell_;
    std::wostringstream shell_log_;
};
