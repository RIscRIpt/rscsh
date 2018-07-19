#pragma once

#include <Windows.h>

class Application {
public:
    Application(HINSTANCE hInstance);
    ~Application();

    int run();

    INT_PTR CALLBACK main_dialog_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    INT_PTR CALLBACK input_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    void create_main_dialog();
    void initialize_main_dialog();
    void update_main_dialog_layout();

    bool input_proc_char(WPARAM wParam, LPARAM lParam);

    HFONT create_font(int size);
    void change_font_size(int delta);

    HINSTANCE hInstance_;

    HFONT hOutputFont_;
    HFONT hInputFont_;

    HWND hMainDialog_;
    HWND hOutput_;
    HWND hInput_;

    WNDPROC origInputProc_;

    int fontSize_;
};
