#pragma once

#include <rsc/Readers.h>
#include <rsc/Context.h>
#include <rsc/Card.h>

#include <memory>
#include <sstream>
#include <unordered_map>

class Shell {
public:
    Shell(std::wostringstream *execution_yield = nullptr);
    Shell(Shell const &other) = delete;
    Shell& operator=(Shell const &other) = delete;

    std::wostringstream* set_execution_yield(std::wostringstream *execution_yield);

    void create_context(DWORD dwScope);
    void create_readers(LPCTSTR mszGroups);
    void create_card(LPCTSTR szReader);

    void execute(LPCTSTR command);
    void execute(rsc::cAPDU const &capdu);

    inline bool has_context() const noexcept { return rscContext_ != nullptr; }
    inline bool has_readers() const noexcept { return rscReaders_ != nullptr; }
    inline bool has_card() const noexcept { return rscCard_ != nullptr; }

    inline rsc::Context& context() { return *rscContext_; }
    inline rsc::Readers& readers() { return *rscReaders_; }
    inline rsc::Card& card() { return *rscCard_; }

private:
    void exit(std::vector<std::wstring> const&);
    void readers(std::vector<std::wstring> const&);
    void connect(std::vector<std::wstring> const &argv);
    void dump(std::vector<std::wstring> const &argv);
    void parse(std::vector<std::wstring> const &argv);

    void parse(rsc::TLVList const &tlvList, size_t parse_depth = 0) const;
    void parse_atr(scb::Bytes const &atr) const;
    void parse_atr_yield_interface_bytes(unsigned char byte, unsigned i) const;

    std::unique_ptr<rsc::Context> rscContext_ = nullptr;
    std::unique_ptr<rsc::Readers> rscReaders_ = nullptr;
    std::unique_ptr<rsc::Card> rscCard_ = nullptr;

    std::wostringstream *execution_yield_ = nullptr;

    rsc::rAPDU last_rapdu_;

    static const std::unordered_map<std::wstring, void (Shell::*)(std::vector<std::wstring> const &)> command_map_;
};

