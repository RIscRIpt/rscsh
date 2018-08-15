#include "MainShell.h"

#include <sstream>
#include <iterator>
#include <algorithm>
#include <thread>

#include "version.ver"

std::unordered_map<std::wstring, void (MainShell::*)(std::vector<std::wstring> const &)> const MainShell::command_map_{
#define X(name, func, _) { name, &MainShell::func },
#include "MainShell_commands.h"
#undef X
};

std::unordered_map<std::wstring, std::wstring> const MainShell::help_map_{
#define X(name, _, desc) { name, desc },
#include "MainShell_commands.h"
#undef X
};

MainShell::MainShell(std::wostringstream &execution_yield, FunctionEnd const &end)
    : Shell(execution_yield)
    , end_(end)
    , cardShell_(execution_yield)
    , cryptoShell_(execution_yield)
{}

void MainShell::execute(LPCTSTR args) {
    std::wistringstream cmd_stream(args);
    std::vector<std::wstring> argv{
        std::istream_iterator<std::wstring, wchar_t>(cmd_stream),
        std::istream_iterator<std::wstring, wchar_t>()
    };

    for (auto &arg : argv)
        std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);

    execute(argv);
}

void MainShell::execute(std::vector<std::wstring> const argv) {
    if (argv.empty()) {
        end_();
        return;
    }

    if (auto cmd = command_map_.find(argv[0]); cmd != command_map_.end()) {
        (this->*cmd->second)(argv);
    } else if (argv[0] == L"crypto") {
        cryptoShell_.execute(argv);
    } else {
        cardShell_.execute(argv);
    }

    end_();
}

void MainShell::help(std::vector<std::wstring> const&) {
    execution_yield_ << "Main Shell Help:\r\n";
    for (auto const& [cmd, help] : help_map_) {
        execution_yield_ << "\r\n" << cmd << ' ' << help << "\r\n";
    }
    execution_yield_ << "\r\n";

    execution_yield_ << "Card Shell Help:\r\n";
    cardShell_.help(L"");

    execution_yield_ << "Cryto Shell Help:\r\n";
    cryptoShell_.help(L"crypto");
}

void MainShell::exit(std::vector<std::wstring> const&) {
    PostQuitMessage(0);
}

void MainShell::version(std::vector<std::wstring> const&) {
    execution_yield_ << VERSION << "\r\n";
}
