#pragma once

#include <Windows.h>

class Application {
public:
    Application(HINSTANCE hInstance);
    ~Application();

    int run();

    INT_PTR CALLBACK dialog_proc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    void create_main_dialog();
    void initialize_fonts();
    void initialize_main_dialog();
    void update_main_dialog_layout();

    HINSTANCE hInstance_;

    HFONT hOutputFont_;
    HFONT hInputFont_;

    HWND hMainDialog_;
    HWND hOutput_;
    HWND hInput_;
};
