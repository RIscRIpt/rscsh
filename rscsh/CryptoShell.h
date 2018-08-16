#pragma once

#include "Shell.h"

#include <vector>
#include <unordered_map>

#include <scb/Bytes.h>

class CryptoShell : public Shell {
public:
    using Shell::Shell;
    using Shell::operator=;

    void help(std::wstring const &prefix);
    
    void execute(std::vector<std::wstring> const &argv);

private:
    scb::Bytes to_bytes(scb::Bytes::StringAs as, std::vector<std::wstring>::const_iterator begin, std::vector<std::wstring>::const_iterator end);

    void sha(std::vector<std::wstring> const &argv);
    void rsa(std::vector<std::wstring> const &argv);
    void des(std::vector<std::wstring> const &argv);

    static const std::unordered_map<std::wstring, void (CryptoShell::*)(std::vector<std::wstring> const &)> command_map_;
    static const std::unordered_map<std::wstring, std::wstring> help_map_;
};
