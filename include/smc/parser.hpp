#pragma once

#include "smc/ast.hpp"
#include "smc/lexer.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace smc {

class Parser {
public:
    Parser(std::string_view input, std::string filename, Diagnostics& diagnostics);

    std::optional<Fsm> parse();

private:
    struct ParseFailure {};

    const Token& peek();
    Token consume();
    bool accept(TokenKind kind);
    bool accept_word(std::string_view text);
    Token expect(TokenKind kind, std::string_view expected);
    Token expect_identifier(std::string_view expected);
    void fail(const Token& token, std::string message);
    std::string directive_line_value(const Token& directive);
    std::string read_parenthesized(const Token& opening);
    std::string read_braced(const Token& opening);
    std::string read_bracketed(const Token& opening);
    void parse_directive(Fsm& fsm, const Token& directive);
    StateMap parse_map(const Token& directive);
    State parse_state(const StateMap& map);
    Transition parse_transition();
    Guard parse_guard();
    bool at_word(std::string_view text);
    Lexer lexer_;
    Diagnostics& diagnostics_;
    std::optional<Token> lookahead_;
    std::string filename_;
};

} // namespace smc
