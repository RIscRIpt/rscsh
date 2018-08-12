#pragma once

#include "Shell.h"

#include <rsc/Readers.h>
#include <rsc/Context.h>
#include <rsc/Card.h>

#include <memory>
#include <unordered_map>

class CardShell : public Shell {
public:
    using Shell::Shell;
    using Shell::operator=;

    void create_context();
    void create_readers();
    void create_card(LPCTSTR szReader);

    void help(std::wstring const &prefix);

    void execute(std::vector<std::wstring> const &argv);
    void execute(rsc::cAPDU const &capdu);

    void print_connection_info();

    inline bool has_context() const noexcept { return rscContext_ != nullptr; }
    inline bool has_readers() const noexcept { return rscReaders_ != nullptr; }
    inline bool has_card() const noexcept { return rscCard_ != nullptr; }

    inline rsc::Context& context() { return *rscContext_; }
    inline rsc::Readers& readers() { return *rscReaders_; }
    inline rsc::Card& card() { return *rscCard_; }

    inline void reset_context() { rscContext_.reset(); }
    inline void reset_readers() { rscReaders_.reset(); }
    inline void reset_card() { rscCard_.reset(); }

private:
    void readers(std::vector<std::wstring> const&);
    void connect(std::vector<std::wstring> const &argv);
    void disconnect(std::vector<std::wstring> const&);
    void reset(std::vector<std::wstring> const &argv);
    void dump(std::vector<std::wstring> const &argv);
    void parse(std::vector<std::wstring> const &argv);

    void select(std::vector<std::wstring> const &argv);

    void parse(rsc::TLVList const &tlvList, size_t parse_depth = 0) const;
    void parse_atr(scb::Bytes const &atr) const;
    void parse_atr_yield_interface_bytes(unsigned char byte, unsigned i) const;

    std::unique_ptr<rsc::Context> rscContext_ = nullptr;
    std::unique_ptr<rsc::Readers> rscReaders_ = nullptr;
    std::unique_ptr<rsc::Card> rscCard_ = nullptr;

    rsc::rAPDU last_rapdu_;

    static const std::unordered_map<std::wstring, void (CardShell::*)(std::vector<std::wstring> const &)> command_map_;
    static const std::unordered_map<std::wstring, std::wstring> help_map_;
};
