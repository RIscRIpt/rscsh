#include "Shell.h"

#include <iterator>
#include <algorithm>

const std::unordered_map<std::wstring, void (Shell::*)(std::vector<std::wstring> const &)> Shell::command_map_{
    { L"readers", &Shell::readers },
    { L"connect", &Shell::connect },
    { L"dump",    &Shell::dump },
    { L"parse",   &Shell::parse },
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
    rscReaders_ = nullptr;
    rscCard_ = nullptr;
}

void Shell::create_readers(LPCTSTR mszGroups) {
    rscReaders_ = std::make_unique<rsc::Readers>(*rscContext_, mszGroups);
    rscCard_ = nullptr;
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
    } else {
        scb::Bytes bytes;
        for (size_t i = 1; i < argv.size(); i++) {
            auto const &arg = argv[i];
            bytes += arg;
        }
        parse(bytes);
    }
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
                *execution_yield_ << prefix << "| ";
                tlv.value().bytes(i, length).print(*execution_yield_);
                *execution_yield_ << "\r\n";
            }
        }

        *execution_yield_ << prefix << "|/\r\n";
    }
}
