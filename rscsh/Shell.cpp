#include "Shell.h"

#include <iterator>
#include <algorithm>

#include <scb/ByteStream.h>

std::unordered_map<std::wstring, void (Shell::*)(std::vector<std::wstring> const &)> const Shell::command_map_{
    { L"exit",       &Shell::exit },
    { L"readers",    &Shell::readers },
    { L"connect",    &Shell::connect },
    { L"disconnect", &Shell::disconnect },
    { L"dump",       &Shell::dump },
    { L"parse",      &Shell::parse },

    { L"select",     &Shell::select },
};

Shell::Shell(std::wostringstream *execution_yield)
    : execution_yield_(execution_yield)
    , last_rapdu_(0)
{}

std::wostringstream* Shell::set_execution_yield(std::wostringstream *execution_yield) {
    auto old = execution_yield_;
    execution_yield_ = execution_yield;
    return old;
}

void Shell::create_context(DWORD dwScope) {
    rscContext_ = std::unique_ptr<rsc::Context>(new rsc::Context(dwScope));
    rscReaders_.reset();
    rscCard_.reset();
}

void Shell::create_readers(LPCTSTR mszGroups) {
    rscReaders_ = std::make_unique<rsc::Readers>(*rscContext_, mszGroups);
    rscCard_.reset();
}

void Shell::create_card(LPCTSTR szReader) {
    rscCard_ = std::make_unique<rsc::Card>(*rscContext_, szReader);
}

void Shell::execute(LPCTSTR command) {
    std::wistringstream cmd_stream(command);
    std::vector<std::wstring> argv{
        std::istream_iterator<std::wstring, wchar_t>(cmd_stream),
        std::istream_iterator<std::wstring, wchar_t>()
    };
    if (argv.empty())
        return;
    for (auto &arg : argv)
        std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
    if (auto cmd = command_map_.find(argv[0]); cmd != command_map_.end()) {
        (this->*cmd->second)(argv);
    } else if (has_card()) {
        execute(rsc::cAPDU(scb::Bytes(command)));
    }
}

void Shell::execute(rsc::cAPDU const &capdu) {
    if (!has_card())
        throw std::runtime_error("cannot execute cAPDU, no card present");

    last_rapdu_ = rscCard_->raw_transmit(capdu);
    if (execution_yield_) {
        *execution_yield_ << "< ";
        capdu.buffer().print(*execution_yield_, L" ");
        *execution_yield_ << "\r\n> ";
        last_rapdu_.buffer().print(*execution_yield_, L" ");
        *execution_yield_ << "\r\n";
    }
    if (last_rapdu_.SW().response_bytes_still_available()) {
        execute(rsc::cAPDU::GET_RESPONSE(last_rapdu_.SW().response_bytes_still_available()));
    }
}

void Shell::exit(std::vector<std::wstring> const&) {
    PostQuitMessage(0);
}

void Shell::readers(std::vector<std::wstring> const&) {
    if (!has_context())
        create_context(SCARD_SCOPE_USER);

    if (!has_readers()) {
        create_readers(SCARD_DEFAULT_READERS);
    } else {
        readers().fetch();
    }

    *execution_yield_ << "Readers:\r\n";
    for (size_t i = 0; i < readers().list().size(); i++) {
        auto const &reader = readers().list()[i];
        *execution_yield_ << "    " << i + 1 << ". " << reader << "\r\n";
    }
    *execution_yield_ << "\r\n";
}

void Shell::connect(std::vector<std::wstring> const &argv) {
    if (argv.size() != 2) {
        *execution_yield_ << "connect <reader id/name>\r\n";
        return;
    }

    auto &reader = argv[1];
    auto reader_id = _wtoi(reader.c_str());

    if (reader_id == 0) {
        create_card(reader.c_str());
    } else {
        reader_id--; // input is 1-based
        if (reader_id < 0 || reader_id >= readers().list().size())
            throw std::runtime_error("invalid reader id");
        create_card(readers().list()[reader_id].c_str());
    }
    card().connect();

    *execution_yield_ << "ATR: ";
    card().atr().print(*execution_yield_, L" ");
    *execution_yield_ << "\r\n";
}

void Shell::disconnect(std::vector<std::wstring> const&) {
    if (has_card()) {
        card().disconnect();
        rscCard_.reset();
    }
}

void Shell::dump(std::vector<std::wstring> const &argv) {
    if (argv.size() == 1) {
        last_rapdu_.buffer().dump(*execution_yield_);
        *execution_yield_ << "\r\n";
    } else if (argv.size() > 1) {
        scb::Bytes bytes;
        for (size_t i = 1; i < argv.size(); i++) {
            auto const &arg = argv[i];
            bytes += arg;
        }
        bytes.dump(*execution_yield_);
        *execution_yield_ << "\r\n";
    }
}

void Shell::parse(std::vector<std::wstring> const &argv) {
    if (argv.size() == 1) {
        parse(last_rapdu_.tlv_list());
    } else if (argv.size() > 1 && argv[1] == L"atr") {
        if (argv.size() > 2) {
            scb::Bytes bytes;
            for (size_t i = 2; i < argv.size(); i++) {
                auto const &arg = argv[i];
                bytes += arg;
            }
            parse_atr(bytes);
        } else if (has_card()) {
            parse_atr(card().atr());
        } else {
            *execution_yield_ << "No card and no ATR provided\r\n";
        }
    } else {
        scb::Bytes bytes;
        for (size_t i = 1; i < argv.size(); i++) {
            auto const &arg = argv[i];
            bytes += arg;
        }
        parse(bytes);
    }
}

void Shell::select(std::vector<std::wstring> const &argv) {
    scb::Bytes name;
    bool first;
    scb::Bytes::StringAs stringAs;

    if (argv.size() < 4) {
    usage:
        *execution_yield_ << "usage: SELECT <first/next> <ascii/hex> <name>\r\n";
        return;
    }

    if (argv[1] == L"first") {
        first = true;
    } else if (argv[1] == L"next") {
        first = false;
    } else {
        goto usage;
    }

    if (argv[2] == L"ascii") {
        stringAs = scb::Bytes::Raw;
    } else if (argv[2] == L"hex") {
        stringAs = scb::Bytes::Hex;
    } else {
        goto usage;
    }

    for (size_t i = 3; i < argv.size(); i++) {
        name += scb::Bytes(argv[i], stringAs);
    }

    *execution_yield_ << "SELECT " << argv[1] << " hex ";
    name.print(*execution_yield_);
    *execution_yield_ << "\r\n";

    execute(rsc::cAPDU::SELECT(name, true, first));
}

void Shell::parse(rsc::TLVList const &tlvList, size_t parse_depth) const {
    std::wstring prefix;
    for (size_t i = 0; i < parse_depth; i++) {
        prefix += L"| ";
    }
    for (auto const &tlv : tlvList) {
        *execution_yield_
            << prefix
            << "* ";
        tlv.tag().bytes().print(*execution_yield_);
        *execution_yield_
            << " (" << tlv.length() << ") " << tlv.tag().name()
            << "\r\n"
            << prefix
            << "|\\\r\n";

        if (tlv.tag().is_constructed()) {
            parse(tlv.value_as_tlv_list(), parse_depth + 1);
        } else {
            for (size_t i = 0; i < tlv.value().size(); i += 16) {
                size_t length = 16;
                if (i + length >= tlv.value().size())
                    length = tlv.value().size() - i;
                *execution_yield_ << prefix << "| > ";
                tlv.value().bytes(i, length).print(*execution_yield_);
                *execution_yield_ << "\r\n";
            }
        }

        *execution_yield_ << prefix << "|/\r\n";
    }
}

void Shell::parse_atr(scb::Bytes const &atr) const {
    std::ios::fmtflags flags(execution_yield_->flags());
    scb::ByteStream bs(atr);

    *execution_yield_ << std::uppercase << std::setfill(L'0');

    auto TS = bs.next_u8();
    switch (TS) {
        case 0x3B:
            *execution_yield_ << "TS  = 3B | Direct convention\r\n";
            break;
        case 0x3F:
            *execution_yield_ << "TS  = 3F | Inverse convention\r\n";
            break;
        default:
            *execution_yield_ << "TS  = " << std::hex << std::setw(2) << TS << " | Unexpected value\r\n";
    }

    auto T0 = bs.next_u8();
    *execution_yield_ << "T0  = " << std::hex << std::setw(2) << T0 << " | ";
    parse_atr_yield_interface_bytes(T0, 1);
    *execution_yield_ << "present, and " << std::dec << (T0 & 0x0F) << " historical bytes\r\n";

    // TA1
    if (T0 & (1 << 4)) {
        auto TA1 = bs.next_u8();
        *execution_yield_ << "TA1 = " << std::hex << std::setw(2) << TA1 << " | ";
        unsigned D = 0;
        switch (TA1 & 0x0F) {
            case 1: D = 1;  break; 
            case 2: D = 2;  break; 
            case 3: D = 4;  break; 
            case 4: D = 8;  break; 
            case 5: D = 16; break; 
            case 6: D = 32; break; 
            case 7: D = 64; break; 
            case 8: D = 12; break; 
            case 9: D = 20; break; 
        }
        if (D)
            *execution_yield_ << "D = " << std::dec << D << ", ";
        else
            *execution_yield_ << "D = unknown, ";

        unsigned F = 0;
        float fmax = 0;
        switch (TA1 >> 4) {
            case 1:  F = 372;  fmax = 5;   break; 
            case 2:  F = 558;  fmax = 6;   break; 
            case 3:  F = 744;  fmax = 8;   break; 
            case 4:  F = 1116; fmax = 12;  break; 
            case 5:  F = 1488; fmax = 16;  break; 
            case 6:  F = 1860; fmax = 20;  break; 
            case 9:  F = 512;  fmax = 5;   break; 
            case 10: F = 768;  fmax = 7.5; break; 
            case 11: F = 1024; fmax = 10;  break; 
            case 12: F = 1536; fmax = 15;  break; 
            case 13: F = 2048; fmax = 20;  break; 
        }
        if (F)
            *execution_yield_ << "F = " << std::dec << F << " and fmax = " << fmax << ", ";
        else
            *execution_yield_ << "F = Internal and fmax = 9600, ";

        float etu;
        if (F)
            etu = 1.0f / static_cast<float>(D) * F / fmax;
        else
            etu = 1.0f / static_cast<float>(D) * 1.0f / 9600.0f;

        *execution_yield_ << "etu = " << etu << "\r\n";
    }

    // TB1
    if (T0 & (1 << 5)) {
        auto TB1 = bs.next_u8();
        *execution_yield_ << "TB1 = " << std::hex << std::setw(2) << TB1 << " | ";

        unsigned I = 0;
        switch ((TB1 >> 5) & 0x03) {
            case 0: I = 25; break;
            case 1: I = 50; break;
            case 2: I = 100; break;
        }

        unsigned PI1 = (TB1 & 0x1F);

        *execution_yield_ << "I = " << std::dec << I << " and PI1 = " << PI1 << "\r\n";
    }

    // TC1
    if (T0 & (1 << 6)) {
        auto TC1 = bs.next_u8();
        *execution_yield_
            << "TC1 = " << std::hex << std::setw(2) << TC1 << " | "
            << "N = " << std::dec << TC1 << "\r\n";
    }

    // TD1
    if (T0 & (1 << 7)) {
        auto TD1 = bs.next_u8();
        *execution_yield_ << "TD1 = " << std::hex << std::setw(2) << TD1 << " | ";

        unsigned T = TD1 & 0x0F;

        parse_atr_yield_interface_bytes(TD1, 2);
        *execution_yield_ << "present, and protocol is T=" << T << "\r\n";

        unsigned i = 2;
        for (auto TDi = TD1; ; i++) {
            if (TDi & (1 << 4)) *execution_yield_ << "TA" << i << " = " << std::hex << std::setw(2) << bs.next_u8() << "\r\n";
            if (TDi & (1 << 5)) *execution_yield_ << "TB" << i << " = " << std::hex << std::setw(2) << bs.next_u8() << "\r\n";
            if (TDi & (1 << 6)) *execution_yield_ << "TC" << i << " = " << std::hex << std::setw(2) << bs.next_u8() << "\r\n";
            unsigned char next_TDi = 0;
            if (TDi & (1 << 7)) {
                next_TDi = bs.next_u8();
                *execution_yield_ << "TD" << i << " = " << std::hex << std::setw(2) << next_TDi << " | ";
                parse_atr_yield_interface_bytes(next_TDi, i + 1);
                *execution_yield_ << "present\r\n";
            }
            if (!(TDi & 0xF0))
                break;
            TDi = next_TDi;
        }
    } else {
        *execution_yield_ << "TD1 is not present, protocol is T=0\r\n";
    }

    execution_yield_->flags(flags);
}

void Shell::parse_atr_yield_interface_bytes(unsigned char byte, unsigned i) const {
    if (byte & (1 << 4)) *execution_yield_ << "TA" << i << ' ';
    if (byte & (1 << 5)) *execution_yield_ << "TB" << i << ' ';
    if (byte & (1 << 6)) *execution_yield_ << "TC" << i << ' ';
    if (byte & (1 << 7)) *execution_yield_ << "TD" << i << ' ';
}
