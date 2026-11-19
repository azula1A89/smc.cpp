#pragma once

#include "smc/diagnostic.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace smc {

enum class TokenKind {
    end,
    invalid,
    identifier,
    directive,
    source_block,
    double_percent,
    scope,
    left_brace,
    right_brace,
    left_bracket,
    right_bracket,
    left_paren,
    right_paren,
    comma,
    semicolon,
    equal,
    dollar,
    colon,
    slash,
    asterisk
};

struct Token {
    TokenKind kind{TokenKind::end};
    std::string text;
    SourceLocation location;
    SourceLocation end_location;
};

class Lexer {
public:
    Lexer(std::string_view input, std::string filename, Diagnostics& diagnostics);

    Token next();
    std::string read_raw_line();
    std::string read_balanced_block(char open, char close, SourceLocation location);
    SourceLocation current_location() const;

private:
    char current() const;
    char peek(std::size_t offset = 1) const;
    char consume();
    bool at_end() const;
    void skip_trivia();
    Token token(TokenKind kind, std::string text, SourceLocation location) const;
    void report(SourceLocation location, std::string message);

    std::string input_;
    std::string filename_;
    Diagnostics& diagnostics_;
    std::size_t offset_{0};
    std::size_t line_{1};
    std::size_t column_{1};
    bool previous_was_cr_{false};
};

} // namespace smc
