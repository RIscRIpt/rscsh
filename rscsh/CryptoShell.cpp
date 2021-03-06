#include "CryptoShell.h"

#include <scb/Bytes.h>
#include <scc/Hash.h>
#include <scc/RSA.h>
#include <scc/DES.h>
#include <scc/AES.h>

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
        execution_yield_ << "Unknown crypto shell command\r\n";
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
        execution_yield_ << "crypto sha [1/224/256/384/512] <hex/ascii/unicode> {buffer}\r\n";
        return;
    }

    scb::Bytes buffer, result;
    int version = std::stoi(argv[2]);

    if (argv[3] == L"hex") {
        buffer = to_bytes(scb::Bytes::Hex, argv.begin() + 4, argv.end());
    } else if (argv[3] == L"ascii") {
        buffer = to_bytes(scb::Bytes::ASCII, argv.begin() + 4, argv.end());
    } else if (argv[3] == L"unicode") {
        buffer = to_bytes(scb::Bytes::Unicode, argv.begin() + 4, argv.end());
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

    result.print(execution_yield_, L"");
    execution_yield_ << "\r\n";
}

void CryptoShell::rsa(std::vector<std::wstring> const &argv) {
    if (argv.size() < 5) {
    usage:
        execution_yield_ << "crypto rsa <modulus> <exponent> <hex/ascii/unicode> {buffer}\r\n";
        return;
    }

    scb::Bytes buffer, result;
    auto modulus = to_bytes(scb::Bytes::Hex, argv.begin() + 2, argv.begin() + 3);
    auto exponent = to_bytes(scb::Bytes::Hex, argv.begin() + 3, argv.begin() + 4);

    if (argv[4] == L"hex") {
        buffer = to_bytes(scb::Bytes::Hex, argv.begin() + 5, argv.end());
    } else if (argv[4] == L"ascii") {
        buffer = to_bytes(scb::Bytes::ASCII, argv.begin() + 5, argv.end());
    } else if (argv[4] == L"unicode") {
        buffer = to_bytes(scb::Bytes::Unicode, argv.begin() + 5, argv.end());
    } else {
        goto usage;
    }

    scc::RSA rsa(modulus, exponent);
    result = rsa.transorm(buffer);

    result.print(execution_yield_, L"");
    execution_yield_ << "\r\n";
}

void CryptoShell::rsa_keygen(std::vector<std::wstring> const &argv) {
    if (argv.size() < 4) {
        execution_yield_ << "crypto rsa-keygen <bits> <public exponent>\r\n";
        return;
    }

    unsigned bits = std::stoi(argv[2]);
    auto exponent = to_bytes(scb::Bytes::Hex, argv.begin() + 3, argv.end());

    scc::RSA rsa(bits, exponent);

    execution_yield_ << "Modulus: ";
    rsa.get_modulus().print(execution_yield_, L"");
    execution_yield_ << "\r\nPublic Exponent: ";
    rsa.get_public_exponent().print(execution_yield_, L"");
    execution_yield_ << "\r\nPrivate Exponent: ";
    rsa.get_private_exponent().print(execution_yield_, L"");
    execution_yield_ << "\r\n";
}

void CryptoShell::des(std::vector<std::wstring> const &argv) {
    if (argv.size() < 5) {
    usage:
        execution_yield_ << "crypto des [decrypt / encrypt] [cbc <iv> / ecb] <key> <hex/ascii/unicode> {buffer}\r\n";
        return;
    }

    auto nextArg = argv.begin() + 2;

    scc::operation::Operation operation;
    auto const &szOperation = *nextArg++;
    if (szOperation == L"decrypt")
        operation = scc::operation::Decrypt;
    else if (szOperation == L"encrypt")
        operation = scc::operation::Encrypt;
    else
        goto usage;

    scb::Bytes iv;

    scc::mode::Mode mode;
    auto const &szMode = *nextArg++;
    if (szMode == L"cbc") {
        mode = scc::mode::CBC;
        iv = to_bytes(scb::Bytes::Hex, nextArg, nextArg + 1);
        ++nextArg;
    } else if (szMode == L"ecb") {
        mode = scc::mode::ECB;
    } else {
        goto usage;
    }

    auto key = to_bytes(scb::Bytes::Hex, nextArg, nextArg + 1);
    ++nextArg;

    scb::Bytes buffer;

    if (*nextArg == L"hex") {
        ++nextArg;
        buffer = to_bytes(scb::Bytes::Hex, nextArg, argv.end());
    } else if (*nextArg == L"ascii") {
        ++nextArg;
        buffer = to_bytes(scb::Bytes::ASCII, nextArg, argv.end());
    } else if (*nextArg == L"unicode") {
        ++nextArg;
        buffer = to_bytes(scb::Bytes::Unicode, nextArg, argv.end());
    } else {
        goto usage;
    }

    scb::Bytes result;
    scc::DES DES(key);
    if (DES.key.size() > 8) {
        result = DES.crypt3(buffer, operation, iv);
    } else {
        result = DES.crypt1(buffer, operation, iv);
    }

    result.print(execution_yield_, L"");
    execution_yield_ << "\r\n";
}

void CryptoShell::des_kcv(std::vector<std::wstring> const &argv) {
    if (argv.size() < 3) {
        execution_yield_ << "crypto des-kcv <key>\r\n";
        return;
    }

    scb::Bytes kcv;
    auto key = to_bytes(scb::Bytes::Hex, argv.begin() + 2, argv.end());

    scc::DES DES(key);
    if (DES.key.size() > 8) {
        kcv = DES.encrypt3_ecb(scb::Bytes(key.size()));
    } else {
        kcv = DES.encrypt1_ecb(scb::Bytes(key.size()));
    }

    kcv.left(3).print(execution_yield_, L"");
    execution_yield_ << "\r\n";
}

void CryptoShell::aes(std::vector<std::wstring> const &argv) {
    if (argv.size() < 5) {
    usage:
        execution_yield_ << "crypto aes [decrypt / encrypt] [cbc <iv> / ecb] <key> <hex/ascii/unicode> {buffer}\r\n";
        return;
    }

    auto nextArg = argv.begin() + 2;

    scc::operation::Operation operation;
    auto const &szOperation = *nextArg++;
    if (szOperation == L"decrypt")
        operation = scc::operation::Decrypt;
    else if (szOperation == L"encrypt")
        operation = scc::operation::Encrypt;
    else
        goto usage;

    scb::Bytes iv;

    scc::mode::Mode mode;
    auto const &szMode = *nextArg++;
    if (szMode == L"cbc") {
        mode = scc::mode::CBC;
        iv = to_bytes(scb::Bytes::Hex, nextArg, nextArg + 1);
        ++nextArg;
    } else if (szMode == L"ecb") {
        mode = scc::mode::ECB;
    } else {
        goto usage;
    }

    auto key = to_bytes(scb::Bytes::Hex, nextArg, nextArg + 1);
    ++nextArg;

    scb::Bytes buffer;

    if (*nextArg == L"hex") {
        ++nextArg;
        buffer = to_bytes(scb::Bytes::Hex, nextArg, argv.end());
    } else if (*nextArg == L"ascii") {
        ++nextArg;
        buffer = to_bytes(scb::Bytes::ASCII, nextArg, argv.end());
    } else if (*nextArg == L"unicode") {
        ++nextArg;
        buffer = to_bytes(scb::Bytes::Unicode, nextArg, argv.end());
    } else {
        goto usage;
    }

    scc::AES AES(key);
    auto result = AES.crypt(buffer, operation, iv);

    result.print(execution_yield_, L"");
    execution_yield_ << "\r\n";
}

void CryptoShell::aes_kcv(std::vector<std::wstring> const &argv) {
    if (argv.size() < 3) {
        execution_yield_ << "crypto aes-kcv <key>\r\n";
        return;
    }

    auto key = to_bytes(scb::Bytes::Hex, argv.begin() + 2, argv.end());

    scc::AES AES(key);
    auto kcv = AES.encrypt_ecb(scb::Bytes(key.size()));

    kcv.left(3).print(execution_yield_, L"");
    execution_yield_ << "\r\n";
}
