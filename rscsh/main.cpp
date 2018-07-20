#include <Windows.h>

#include "Application.h"

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    try {
        return Application(hInstance).run();
    } catch (std::system_error const &e) {
        MessageBoxA(HWND_DESKTOP, e.what(), "System Error", MB_ICONERROR);
        return e.code().value();
    } catch (std::runtime_error const &e) {
        MessageBoxA(HWND_DESKTOP, e.what(), "Runtime Error", MB_ICONERROR);
        return -2;
    } catch (std::exception const &e) {
        MessageBoxA(HWND_DESKTOP, e.what(), "Error", MB_ICONERROR);
        return -1;
    }
}
