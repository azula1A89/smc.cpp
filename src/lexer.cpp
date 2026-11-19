#include "smc/lexer.hpp"

#include <cctype>
#include <utility>

namespace smc {
namespace {

bool is_identifier_start(unsigned char ch) {
    return std::isalpha(ch) != 0 || ch == '_';
}

bool is_identifier_part(unsigned char ch) {
    return std::isalnum(ch) != 0 || ch == '_' || ch == '.' || ch == '<' || ch == '>';
}

} // namespace

Lexer::Lexer(std::string_view input, std::string filename, Diagnostics& diagnostics)
    : input_(input), filename_(std::move(filename)), diagnostics_(diagnostics) {}

char Lexer::current() const {
    return at_end() ? '\0' : input_[offset_];
}

char Lexer::peek(std::size_t offset) const {
    const auto index = offset_ + offset;
    return index >= input_.size() ? '\0' : input_[index];
}

char Lexer::consume() {
    if (at_end()) {
        return '\0';
    }

    const char ch = input_[offset_++];
    if (ch == '\r') {
        ++line_;
        column_ = 1;
        previous_was_cr_ = true;
    } else if (ch == '\n') {
        if (!previous_was_cr_) {
            ++line_;
        }
        column_ = 1;
        previous_was_cr_ = false;
    } else {
        ++column_;
        previous_was_cr_ = false;
    }
    return ch;
}

bool Lexer::at_end() const {
    return offset_ >= input_.size();
}

Token Lexer::token(TokenKind kind, std::string text, SourceLocation location) const {
    return Token{kind, std::move(text), std::move(location), current_location()};
}

SourceLocation Lexer::current_location() const {
    return SourceLocation{filename_, line_, column_};
}

void Lexer::report(SourceLocation location, std::string message) {
    diagnostics_.push_back(Diagnostic{DiagnosticSeverity::error, std::move(location),
                                     std::move(message)});
}

void Lexer::skip_trivia() {
    while (!at_end()) {
        if (std::isspace(static_cast<unsigned char>(current())) != 0) {
            consume();
            continue;
        }
        if (current() == '/' && peek() == '/') {
            while (!at_end() && current() != '\r' && current() != '\n') {
                consume();
            }
            continue;
        }
        if (current() == '/' && peek() == '*') {
            const SourceLocation start{filename_, line_, column_};
            consume();
            consume();
            std::size_t depth = 1;
            while (!at_end() && depth > 0) {
                if (current() == '/' && peek() == '*') {
                    consume();
                    consume();
                    ++depth;
                } else if (current() == '*' && peek() == '/') {
                    consume();
                    consume();
                    --depth;
                } else {
                    consume();
                }
            }
            if (depth != 0) {
                report(start, "Unterminated block comment.");
            }
            continue;
        }
        break;
    }
}

Token Lexer::next() {
    skip_trivia();
    const SourceLocation start{filename_, line_, column_};
    if (at_end()) {
        return token(TokenKind::end, {}, start);
    }

    if (current() == '%' && peek() == '{') {
        consume();
        consume();
        const std::size_t body_start = offset_;
        while (!at_end() && !(current() == '%' && peek() == '}')) {
            consume();
        }
        const std::string body = input_.substr(body_start, offset_ - body_start);
        if (at_end()) {
            report(start, "Unterminated source block; expected '%}'.");
            return token(TokenKind::invalid, body, start);
        }
        consume();
        consume();
        return token(TokenKind::source_block, body, start);
    }

    if (current() == '%' && peek() == '%') {
        consume();
        consume();
        return token(TokenKind::double_percent, "%%", start);
    }

    if (current() == '%') {
        consume();
        std::string directive{"%"};
        while (is_identifier_part(static_cast<unsigned char>(current()))) {
            directive.push_back(consume());
        }
        if (directive.size() == 1) {
            report(start, "Invalid '%' token.");
            return token(TokenKind::invalid, directive, start);
        }
        return token(TokenKind::directive, std::move(directive), start);
    }

    if (is_identifier_start(static_cast<unsigned char>(current()))) {
        std::string word;
        while (is_identifier_part(static_cast<unsigned char>(current()))) {
            word.push_back(consume());
        }
        if (current() == ':' && peek() == ':') {
            consume();
            consume();
            word += "::";
            while (is_identifier_part(static_cast<unsigned char>(current()))) {
                word.push_back(consume());
            }
        }
        return token(TokenKind::identifier, std::move(word), start);
    }

    const char ch = consume();
    switch (ch) {
    case '{': return token(TokenKind::left_brace, "{", start);
    case '}': return token(TokenKind::right_brace, "}", start);
    case '[': return token(TokenKind::left_bracket, "[", start);
    case ']': return token(TokenKind::right_bracket, "]", start);
    case '(': return token(TokenKind::left_paren, "(", start);
    case ')': return token(TokenKind::right_paren, ")", start);
    case ',': return token(TokenKind::comma, ",", start);
    case ';': return token(TokenKind::semicolon, ";", start);
    case '=': return token(TokenKind::equal, "=", start);
    case '$': return token(TokenKind::dollar, "$", start);
    case ':':
        if (current() == ':') {
            consume();
            return token(TokenKind::scope, "::", start);
        }
        return token(TokenKind::colon, ":", start);
    case '/': return token(TokenKind::slash, "/", start);
    case '*': return token(TokenKind::asterisk, "*", start);
    default:
        report(start, "Unknown character in SMC input.");
        return token(TokenKind::invalid, std::string(1, ch), start);
    }
}

std::string Lexer::read_raw_line() {
    while (current() == ' ' || current() == '\t') {
        consume();
    }
    std::string result;
    while (!at_end() && current() != '\r' && current() != '\n') {
        result.push_back(consume());
    }
    while (!at_end() && (current() == '\r' || current() == '\n')) {
        consume();
    }
    return result;
}

std::string Lexer::read_balanced_block(char open, char close, SourceLocation location) {
    std::string result;
    std::size_t depth = 1;
    char quote = '\0';
    bool escaped = false;
    bool line_comment = false;
    std::size_t block_comment_depth = 0;

    while (!at_end()) {
        const char ch = current();
        const char next_ch = peek();

        if (line_comment) {
            result.push_back(consume());
            if (ch == '\r' || ch == '\n') {
                line_comment = false;
            }
            continue;
        }
        if (block_comment_depth != 0) {
            if (ch == '/' && next_ch == '*') {
                result.push_back(consume());
                result.push_back(consume());
                ++block_comment_depth;
            } else if (ch == '*' && next_ch == '/') {
                result.push_back(consume());
                result.push_back(consume());
                --block_comment_depth;
            } else {
                result.push_back(consume());
            }
            continue;
        }
        if (quote != '\0') {
            result.push_back(consume());
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == quote) {
                quote = '\0';
            }
            continue;
        }
        if (ch == '/' && next_ch == '/') {
            result.push_back(consume());
            result.push_back(consume());
            line_comment = true;
            continue;
        }
        if (ch == '/' && next_ch == '*') {
            result.push_back(consume());
            result.push_back(consume());
            block_comment_depth = 1;
            continue;
        }
        if (ch == '"' || ch == '\'') {
            quote = ch;
            result.push_back(consume());
            continue;
        }
        if (ch == open) {
            ++depth;
            result.push_back(consume());
            continue;
        }
        if (ch == close) {
            consume();
            if (--depth == 0) {
                return result;
            }
            result.push_back(ch);
            continue;
        }
        result.push_back(consume());
    }

    report(std::move(location), "Unterminated balanced source block.");
    return result;
}

} // namespace smc
