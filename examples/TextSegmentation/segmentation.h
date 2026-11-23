#pragma once

#include "TextSegmentation_v2.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class TextSegmentation {
public:
    using TokenCount = std::function<std::size_t(std::string_view)>;

    TextSegmentation(TokenCount token_count, std::size_t limit,
                     std::size_t fixed_tokens = 0);
    std::vector<std::string> segment(std::string_view text);

    bool is_opening_quote() const;
    bool is_closing_quote() const;
    bool is_mismatched_closer() const;
    bool closes_outer_quote() const;
    bool is_delimiter() const;
    bool candidate_fits() const;
    void begin_quote();
    void push_quote();
    void pop_quote();
    void end_quote();
    void fallback_quote();
    void end_atom();
    void finish_text();
    void append_atom();
    void split_atom();
    void flush();

private:
    struct Character {
        std::uint32_t value;
        std::size_t begin;
        std::size_t end;
    };

    bool is_apostrophe() const;
    bool fits(std::string_view text) const;
    void emit_atom(std::size_t begin, std::size_t end);
    void fallback_range(std::size_t begin, std::size_t end);

    TextSegmentationContext fsm_;
    TokenCount token_count_;
    std::size_t limit_;
    std::size_t fixed_tokens_;
    std::string_view text_;
    std::vector<Character> chars_;
    std::size_t index_ = 0;
    std::size_t atom_begin_ = 0;
    std::size_t quote_begin_ = 0;
    std::vector<std::uint32_t> quotes_;
    std::vector<std::string_view> atoms_;
    std::string_view atom_;
    std::string current_;
    std::vector<std::string> output_;
};
