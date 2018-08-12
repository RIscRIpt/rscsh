#include "CryptoShell.h"

#include <scb/Bytes.h>
#include <scc/Hash.h>
#include <scc/RSA.h>

std::unordered_map<std::wstring, void (CryptoShell::*)(std::vector<std::wstring> const &)> const CryptoShell::command_map_{
#define X(name, func, _) { name, &CryptoShell::func },
#include "CryptoShell_commands.h"
#undef X
};

std::unordered_map<std::wstring, std::wstring> const CryptoShell::help_map_{
#define X(name, _, desc) { name, desc },
#include "CryptoShell_commands.h"
#undef X
};

void CryptoShell::help(std::wstring const &prefix) {
    for (auto const& [cmd, help] : help_map_) {
        execution_yield_ << "\r\n" << prefix << ' ' << cmd << ' ' << help << "\r\n";
    }
    execution_yield_ << "\r\n";
}

void CryptoShell::execute(std::vector<std::wstring> const &argv) {
    if (argv.size() < 2) {
        help(L"crypto");
        return;
    }

    if (auto cmd = command_map_.find(argv[1]); cmd != command_map_.end()) {
        (this->*cmd->second)(argv);
    } else {
        execution_yield_ << "Unknown crypto command\r\n";
    }
}

scb::Bytes CryptoShell::to_bytes(scb::Bytes::StringAs as, std::vector<std::wstring>::const_iterator begin, std::vector<std::wstring>::const_iterator end) {
    scb::Bytes result;
    for (auto i = begin; i != end; ++i) {
        result += scb::Bytes(i->c_str(), as);
    }
    return result;
}

void CryptoShell::sha(std::vector<std::wstring> const &argv) {
    if (argv.size() < 4) {
    usage:
        execution_yield_ << "crypto sha [1/224/256/384/512] <hex/ascii> {buffer}\r\n";
        return;
    }

    scb::Bytes buffer, result;
    int version = std::stoi(argv[2]);

    if (argv[3] == L"hex") {
        buffer = to_bytes(scb::Bytes::Hex, argv.begin() + 4, argv.end());
    } else if (argv[3] == L"ascii") {
        buffer = to_bytes(scb::Bytes::Raw, argv.begin() + 4, argv.end());
    } else {
        goto usage;
    }

    switch (version) {
        case 1:
            result = scc::SHA1(buffer);
            break;
        case 224:
            result = scc::SHA224(buffer);
            break;
        case 256:
            result = scc::SHA256(buffer);
            break;
        case 384:
            result = scc::SHA384(buffer);
            break;
        case 512:
            result = scc::SHA512(buffer);
            break;
        default:
            goto usage;
    }

    execution_yield_
        << argv[0] << ' '
        << argv[1] << ' '
        << argv[2] << ' '
        << "hex ";
    buffer.print(execution_yield_, L" ");
    execution_yield_ << "\r\n";
    result.print(execution_yield_, L"");
    execution_yield_ << "\r\n";
}

void CryptoShell::rsa(std::vector<std::wstring> const &argv) {
    if (argv.size() < 5) {
    usage:
        execution_yield_ << "crypto rsa <modulus> <exponent> <hex/ascii> {buffer}\r\n";
        return;
    }

    scb::Bytes buffer, result;
    auto modulus = to_bytes(scb::Bytes::Hex, argv.begin() + 2, argv.begin() + 3);
    auto exponent = to_bytes(scb::Bytes::Hex, argv.begin() + 3, argv.begin() + 4);

    if (argv[4] == L"hex") {
        buffer = to_bytes(scb::Bytes::Hex, argv.begin() + 5, argv.end());
    } else if (argv[4] == L"ascii") {
        buffer = to_bytes(scb::Bytes::Raw, argv.begin() + 5, argv.end());
    } else {
        goto usage;
    }

    scc::RSA rsa(modulus, exponent);
    result = rsa.transorm(buffer);

    execution_yield_
        << argv[0] << ' '
        << argv[1] << ' ';
    modulus.print(execution_yield_, L"");
    execution_yield_ << ' ';
    exponent.print(execution_yield_, L"");
    execution_yield_ << " hex ";
    buffer.print(execution_yield_, L" ");
    execution_yield_ << "\r\n";
    result.print(execution_yield_, L"");
    execution_yield_ << "\r\n";
}
