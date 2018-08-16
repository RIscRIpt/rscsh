#pragma once

#include "Shell.h"
#include "CardShell.h"
#include "CryptoShell.h"

#include <functional>

class MainShell : public Shell {
public:
    using FunctionEnd = std::function<void()>;

    MainShell(std::wostringstream &execution_yield, FunctionEnd const &end);
    MainShell(MainShell const &other) = delete;

    using Shell::operator=;

    void execute(LPCTSTR args);
    void execute(std::vector<std::wstring> const argv);

    inline CardShell& card_shell() noexcept { return cardShell_; }
    inline CryptoShell& crypto_shell() noexcept { return cryptoShell_; }

private:
    void help(std::vector<std::wstring> const&);
    void exit(std::vector<std::wstring> const&);
    void version(std::vector<std::wstring> const&);

    FunctionEnd end_;

    CardShell cardShell_;
    CryptoShell cryptoShell_;

    static const std::unordered_map<std::wstring, void (MainShell::*)(std::vector<std::wstring> const &)> command_map_;
    static const std::unordered_map<std::wstring, std::wstring> help_map_;
};
