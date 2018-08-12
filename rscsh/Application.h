#pragma once

#include "MainShell.h"

#include <rsc/EventListener.h>

#include <Windows.h>

class Application {
public:
    Application(HINSTANCE hInstance);
    ~Application();

    int run();

    INT_PTR CALLBACK main_dialog_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    INT_PTR CALLBACK input_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void shell_done();

    static char const *APP_NAME;
    
    static int const MIN_FONT_SIZE;
    static int const DEF_FONT_SIZE;
    static int const MAX_FONT_SIZE;

private:
    void create_main_dialog();
    void initialize_main_dialog();
    void update_main_dialog_layout();

    void rsc_event(DWORD event, rsc::Context const &context, std::wstring const &reader);

    bool input_proc_char(WPARAM wParam, LPARAM lParam);
    bool input_proc_keydown(WPARAM wParam, LPARAM lParam);
    bool input_proc_keyup(WPARAM wParam, LPARAM lParam);

    HFONT create_font(int size);
    void change_font_size(int delta);

    void log(char const *message);
    void log(wchar_t const *message);

    void logf(char const *fmt, ...);
    void logf(wchar_t const *fmt, ...);

    void log_shell();
    void clear_shell_log();
    
    void parse_input();
    void shell_execute(LPCTSTR command);

    HINSTANCE hInstance_;

    HFONT hOutputFont_;
    HFONT hInputFont_;

    HWND hMainDialog_;
    HWND hOutput_;
    HWND hInput_;

    HICON hIcon_;

    WNDPROC origInputProc_;

    int fontSize_;

    bool input_ctrl_pressed_;

    rsc::EventListener rscEventListener_;
    MainShell shell_;
    std::wostringstream shell_log_;
};
