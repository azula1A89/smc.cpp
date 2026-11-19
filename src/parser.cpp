#include "smc/parser.hpp"

#include "cpp_generator_internal.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace smc {
namespace {

std::string trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
    const auto begin = std::find_if(value.begin(), value.end(), not_space);
    const auto end = std::find_if(value.rbegin(), value.rend(), not_space).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

SourceLocation advance_location(SourceLocation location, std::string_view text) {
    bool previous_cr = false;
    for (const char ch : text) {
        if (ch == '\r') {
            ++location.line;
            location.column = 1;
            previous_cr = true;
        } else if (ch == '\n') {
            if (!previous_cr)
                ++location.line;
            location.column = 1;
            previous_cr = false;
        } else {
            ++location.column;
            previous_cr = false;
        }
    }
    return location;
}

std::optional<SourceRange> trimmed_range(
    const Token& opening, std::string_view body) {
    const auto first = body.find_first_not_of(" \t\r\n\f\v");
    if (first == std::string_view::npos)
        return std::nullopt;
    const auto last = body.find_last_not_of(" \t\r\n\f\v") + 1;
    auto begin = advance_location(opening.end_location, body.substr(0, first));
    auto end = advance_location(begin, body.substr(first, last - first));
    return SourceRange{std::move(begin), std::move(end)};
}

bool is_identifier_start(char ch) {
    const auto value = static_cast<unsigned char>(ch);
    return std::isalpha(value) != 0 || ch == '_';
}

bool is_identifier_part(char ch) {
    const auto value = static_cast<unsigned char>(ch);
    return std::isalnum(value) != 0 || ch == '_';
}

bool valid_cpp_parameters(std::string_view parameters) {
    try {
        (void)cpp_detail::parameters(parameters);
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

std::optional<std::string> validate_action_body(std::string_view body) {
    std::size_t index = 0;
    const auto skip_trivia = [&]() {
        while (index < body.size()) {
            if (std::isspace(static_cast<unsigned char>(body[index])) != 0) {
                ++index;
            } else if (index + 1 < body.size() && body[index] == '/' &&
                       body[index + 1] == '/') {
                index += 2;
                while (index < body.size() && body[index] != '\r' && body[index] != '\n') {
                    ++index;
                }
            } else if (index + 1 < body.size() && body[index] == '/' &&
                       body[index + 1] == '*') {
                index += 2;
                std::size_t depth = 1;
                while (index < body.size() && depth != 0) {
                    if (index + 1 < body.size() && body[index] == '/' &&
                        body[index + 1] == '*') {
                        index += 2;
                        ++depth;
                    } else if (index + 1 < body.size() && body[index] == '*' &&
                               body[index + 1] == '/') {
                        index += 2;
                        --depth;
                    } else {
                        ++index;
                    }
                }
                if (depth != 0) {
                    return false;
                }
            } else {
                break;
            }
        }
        return true;
    };

    while (true) {
        if (!skip_trivia()) {
            return "Unterminated comment in action block.";
        }
        if (index == body.size()) {
            return std::nullopt;
        }
        if (!is_identifier_start(body[index])) {
            return "Expected an action name or end of action block.";
        }
        ++index;
        while (index < body.size()) {
            if (is_identifier_part(body[index]) || body[index] == '.') {
                ++index;
            } else if (body[index] == ':' && index + 1 < body.size() &&
                       body[index + 1] == ':') {
                index += 2;
            } else {
                break;
            }
        }
        if (!skip_trivia()) {
            return "Unterminated comment after action name.";
        }
        if (index < body.size() && body[index] == '(') {
            std::size_t depth = 1;
            ++index;
            char quote = '\0';
            bool escaped = false;
            while (index < body.size() && depth != 0) {
                const char ch = body[index];
                if (quote != '\0') {
                    ++index;
                    if (escaped) {
                        escaped = false;
                    } else if (ch == '\\') {
                        escaped = true;
                    } else if (ch == quote) {
                        quote = '\0';
                    }
                    continue;
                }
                if (ch == '"' || ch == '\'') {
                    quote = ch;
                    ++index;
                } else if (index + 1 < body.size() && ch == '/' &&
                           body[index + 1] == '/') {
                    index += 2;
                    while (index < body.size() && body[index] != '\r' &&
                           body[index] != '\n') {
                        ++index;
                    }
                } else if (index + 1 < body.size() && ch == '/' &&
                           body[index + 1] == '*') {
                    index += 2;
                    std::size_t comment_depth = 1;
                    while (index < body.size() && comment_depth != 0) {
                        if (index + 1 < body.size() && body[index] == '/' &&
                            body[index + 1] == '*') {
                            index += 2;
                            ++comment_depth;
                        } else if (index + 1 < body.size() && body[index] == '*' &&
                                   body[index + 1] == '/') {
                            index += 2;
                            --comment_depth;
                        } else {
                            ++index;
                        }
                    }
                    if (comment_depth != 0) {
                        return "Unterminated comment in action arguments.";
                    }
                } else if (ch == '(') {
                    ++depth;
                    ++index;
                } else if (ch == ')') {
                    --depth;
                    ++index;
                } else {
                    ++index;
                }
            }
            if (depth != 0 || quote != '\0') {
                return "Unterminated action argument list.";
            }
        } else if (index < body.size() && body[index] == '=') {
            ++index;
            if (!skip_trivia()) {
                return "Unterminated comment in property assignment.";
            }
            const std::size_t value_start = index;
            std::size_t paren_depth = 0;
            std::size_t bracket_depth = 0;
            std::size_t brace_depth = 0;
            char quote = '\0';
            bool escaped = false;
            while (index < body.size()) {
                const char ch = body[index];
                const char next = index + 1 < body.size() ? body[index + 1] : '\0';
                if (quote != '\0') {
                    ++index;
                    if (escaped) {
                        escaped = false;
                    } else if (ch == '\\') {
                        escaped = true;
                    } else if (ch == quote) {
                        quote = '\0';
                    }
                    continue;
                }
                if (ch == '"' || ch == '\'') {
                    quote = ch;
                    ++index;
                } else if (ch == '/' && next == '/') {
                    index += 2;
                    while (index < body.size() && body[index] != '\r' &&
                           body[index] != '\n') {
                        ++index;
                    }
                } else if (ch == '/' && next == '*') {
                    index += 2;
                    std::size_t comment_depth = 1;
                    while (index < body.size() && comment_depth != 0) {
                        if (index + 1 < body.size() && body[index] == '/' &&
                            body[index + 1] == '*') {
                            index += 2;
                            ++comment_depth;
                        } else if (index + 1 < body.size() && body[index] == '*' &&
                                   body[index + 1] == '/') {
                            index += 2;
                            --comment_depth;
                        } else {
                            ++index;
                        }
                    }
                    if (comment_depth != 0) {
                        return "Unterminated comment in property assignment.";
                    }
                } else if (ch == '(') {
                    ++paren_depth;
                    ++index;
                } else if (ch == ')' && paren_depth != 0) {
                    --paren_depth;
                    ++index;
                } else if (ch == '[') {
                    ++bracket_depth;
                    ++index;
                } else if (ch == ']' && bracket_depth != 0) {
                    --bracket_depth;
                    ++index;
                } else if (ch == '{') {
                    ++brace_depth;
                    ++index;
                } else if (ch == '}' && brace_depth != 0) {
                    --brace_depth;
                    ++index;
                } else if (ch == ';' && paren_depth == 0 && bracket_depth == 0 &&
                           brace_depth == 0) {
                    break;
                } else {
                    ++index;
                }
            }
            if (index == body.size() ||
                trim(std::string(body.substr(value_start, index - value_start))).empty()) {
                return "Property assignment requires a value and trailing ';'.";
            }
        } else {
            return "Expected '(' or '=' after action name.";
        }
        if (!skip_trivia()) {
            return "Unterminated comment after action.";
        }
        if (index == body.size() || body[index] != ';') {
            return "Expected ';' after action.";
        }
        ++index;
    }
}

} // namespace

Parser::Parser(std::string_view input, std::string filename, Diagnostics& diagnostics)
    : lexer_(input, filename, diagnostics), diagnostics_(diagnostics),
      filename_(std::move(filename)) {}

const Token& Parser::peek() {
    if (!lookahead_) {
        lookahead_ = lexer_.next();
    }
    return *lookahead_;
}

Token Parser::consume() {
    Token result = peek();
    lookahead_.reset();
    return result;
}

bool Parser::accept(TokenKind kind) {
    if (peek().kind != kind) {
        return false;
    }
    consume();
    return true;
}

bool Parser::accept_word(std::string_view text) {
    if (peek().kind != TokenKind::identifier || peek().text != text) {
        return false;
    }
    consume();
    return true;
}

Token Parser::expect(TokenKind kind, std::string_view expected) {
    if (peek().kind != kind) {
        fail(peek(), "Expected " + std::string(expected) + ".");
    }
    return consume();
}

Token Parser::expect_identifier(std::string_view expected) {
    if (peek().kind != TokenKind::identifier) {
        fail(peek(), "Expected " + std::string(expected) + ".");
    }
    return consume();
}

void Parser::fail(const Token& token, std::string message) {
    diagnostics_.push_back(Diagnostic{DiagnosticSeverity::error, token.location,
                                     std::move(message)});
    throw ParseFailure{};
}

std::string Parser::directive_line_value(const Token& directive) {
    lookahead_.reset();
    const auto value = trim(lexer_.read_raw_line());
    if (value.empty()) {
        fail(directive, "Expected a value after " + directive.text + ".");
    }
    return value;
}

std::string Parser::read_parenthesized(const Token& opening) {
    lookahead_.reset();
    return lexer_.read_balanced_block('(', ')', opening.location);
}

std::string Parser::read_braced(const Token& opening) {
    lookahead_.reset();
    return lexer_.read_balanced_block('{', '}', opening.location);
}

std::string Parser::read_bracketed(const Token& opening) {
    lookahead_.reset();
    return lexer_.read_balanced_block('[', ']', opening.location);
}

void Parser::parse_directive(Fsm& fsm, const Token& directive) {
    if (directive.text == "%start") {
        const auto name = expect_identifier("a start state in map::state form");
        fsm.start_state = name.text;
        fsm.start_state_range = SourceRange{name.location, name.end_location};
    } else if (directive.text == "%class") {
        fsm.context_class = expect_identifier("a context class name").text;
    } else if (directive.text == "%fsmclass") {
        fsm.fsm_class = expect_identifier("an FSM class name").text;
    } else if (directive.text == "%fsmfile") {
        fsm.fsm_file = expect_identifier("an FSM file name").text;
    } else if (directive.text == "%package") {
        fsm.package_name = expect_identifier("a package name").text;
    } else if (directive.text == "%header") {
        fsm.header_file = directive_line_value(directive);
    } else if (directive.text == "%include") {
        fsm.includes.push_back(directive_line_value(directive));
    } else if (directive.text == "%import") {
        fsm.imports.push_back(directive_line_value(directive));
    } else if (directive.text == "%declare") {
        fsm.declarations.push_back(directive_line_value(directive));
    } else if (directive.text == "%access") {
        fsm.access_level = directive_line_value(directive);
    } else {
        fail(directive, "Unsupported directive '" + directive.text + "'.");
    }
}

StateMap Parser::parse_map(const Token& directive) {
    StateMap map;
    map.location = directive.location;
    const auto name = expect_identifier("a map name");
    map.name = name.text;
    map.name_range = SourceRange{name.location, name.end_location};
    expect(TokenKind::double_percent, "'%%' after the map name");

    while (peek().kind != TokenKind::double_percent && peek().kind != TokenKind::end) {
        if (peek().kind != TokenKind::identifier) {
            fail(peek(), "Expected a state definition or '%%' to end the map.");
        }
        map.states.push_back(parse_state(map));
    }
    const Token end = expect(TokenKind::double_percent, "'%%' to end the map");
    map.end_location = end.end_location;
    const bool has_default = std::any_of(map.states.begin(), map.states.end(),
                                         [](const State& state) {
                                             return state.name == "Default";
                                         });
    if (!has_default) {
        State default_state;
        default_state.name = "Default";
        default_state.location = map.location;
        default_state.end_location = map.location;
        map.states.push_back(std::move(default_state));
    }
    return map;
}

State Parser::parse_state(const StateMap&) {
    State state;
    const Token name = expect_identifier("a state name");
    state.name = name.text;
    state.location = name.location;
    state.name_range = SourceRange{name.location, name.end_location};

    while (at_word("Entry") || at_word("Exit")) {
        const auto action_kind = consume();
        const auto opening = expect(TokenKind::left_brace, "'{' after Entry or Exit");
        auto body = read_braced(opening);
        if (const auto error = validate_action_body(body)) {
            fail(opening, *error);
        }
        if (action_kind.text == "Entry") {
            if (state.entry_actions.has_value()) {
                diagnostics_.push_back(Diagnostic{DiagnosticSeverity::warning,
                                                  action_kind.location,
                                                  "Entry action previously specified; duplicate ignored."});
                continue;
            }
            state.entry_actions.emplace(std::move(body));
        } else {
            if (state.exit_actions.has_value()) {
                diagnostics_.push_back(Diagnostic{DiagnosticSeverity::warning,
                                                  action_kind.location,
                                                  "Exit action previously specified; duplicate ignored."});
                continue;
            }
            state.exit_actions.emplace(std::move(body));
        }
    }

    expect(TokenKind::left_brace, "'{' before state transitions");
    while (peek().kind != TokenKind::right_brace && peek().kind != TokenKind::end) {
        if (peek().kind != TokenKind::identifier) {
            fail(peek(), "Expected a transition name or '}' to end the state.");
        }
        state.transitions.push_back(parse_transition());
    }
    const Token end = expect(TokenKind::right_brace, "'}' to end the state");
    state.end_location = end.end_location;
    return state;
}

Transition Parser::parse_transition() {
    Transition transition;
    const Token name = expect_identifier("a transition name");
    transition.name = name.text;
    transition.location = name.location;
    transition.name_range = SourceRange{name.location, name.end_location};

    if (peek().kind == TokenKind::left_paren) {
        const Token opening = consume();
        transition.parameters = read_parenthesized(opening);
        if (!valid_cpp_parameters(transition.parameters)) {
            fail(opening, "Invalid C++ parameter syntax; expected name: type.");
        }
        if (transition.name == "Default" && !trim(transition.parameters).empty()) {
            fail(name, "Default transitions may not have parameters.");
        }
    }

    do {
        transition.guards.push_back(parse_guard());
    } while (peek().kind == TokenKind::left_bracket);
    transition.end_location = transition.guards.back().end_location;
    return transition;
}

Guard Parser::parse_guard() {
    Guard guard;
    guard.location = peek().location;
    if (peek().kind == TokenKind::left_bracket) {
        const Token opening = consume();
        guard.condition = trim(read_bracketed(opening));
    }

    if (accept_word("push")) {
        guard.transition_type = "push";
        const Token opening = expect(TokenKind::left_paren, "'(' after push");
        const auto body = read_parenthesized(opening);
        guard.push_state = trim(body);
        guard.push_state_range = trimmed_range(opening, body);
        if (guard.push_state.empty()) {
            fail(opening, "A push transition requires a destination state.");
        }
        guard.end_state = "nil";
    } else if (accept_word("jump")) {
        guard.transition_type = "jump";
        const Token opening = expect(TokenKind::left_paren, "'(' after jump");
        const auto body = read_parenthesized(opening);
        guard.end_state = trim(body);
        guard.end_state_range = trimmed_range(opening, body);
        if (guard.end_state.empty()) {
            fail(opening, "A jump transition requires a destination state.");
        }
    } else if (accept_word("pop")) {
        guard.transition_type = "pop";
        if (peek().kind == TokenKind::left_paren) {
            const Token opening = consume();
            const auto body = read_parenthesized(opening);
            const auto comma = body.find(',');
            guard.end_state = trim(body.substr(0, comma));
            guard.end_state_range = trimmed_range(opening, std::string_view(body).substr(0, comma));
            if (comma != std::string::npos) {
                guard.pop_arguments = trim(body.substr(comma + 1));
            }
            if (!guard.end_state.empty() &&
                (!is_identifier_start(guard.end_state.front()) ||
                 !std::all_of(guard.end_state.begin() + 1, guard.end_state.end(),
                              is_identifier_part))) {
                fail(opening, "A pop transition requires a transition name before its arguments.");
            }
            if (comma != std::string::npos &&
                (guard.end_state.empty() || guard.pop_arguments.empty())) {
                fail(opening, "A pop transition requires a transition name and arguments.");
            }
        }
    } else {
        const Token destination = expect_identifier("an end state, push, pop or jump");
        guard.end_state = destination.text;
        guard.end_state_range = SourceRange{destination.location, destination.end_location};
        if (accept(TokenKind::slash)) {
            if (!accept_word("push")) {
                fail(peek(), "'push' must follow '/' in a push transition.");
            }
            guard.transition_type = "push";
            const Token opening = expect(TokenKind::left_paren, "'(' after /push");
            const auto body = read_parenthesized(opening);
            guard.push_state = trim(body);
            guard.push_state_range = trimmed_range(opening, body);
            if (guard.push_state.empty()) {
                fail(opening, "A push transition requires a destination state.");
            }
        }
    }

    const Token opening = expect(TokenKind::left_brace, "'{' before transition actions");
    guard.actions = read_braced(opening);
    if (const auto error = validate_action_body(guard.actions)) {
        fail(opening, *error);
    }
    guard.end_location = lexer_.current_location();
    return guard;
}

bool Parser::at_word(std::string_view text) {
    return peek().kind == TokenKind::identifier && peek().text == text;
}

std::optional<Fsm> Parser::parse() {
    Fsm fsm;
    fsm.source_name = filename_;
    fsm.location = SourceLocation{filename_, 1, 1};
    bool saw_map = false;
    bool saw_source = false;

    try {
        while (peek().kind != TokenKind::end) {
            const Token current = consume();
            if (current.kind == TokenKind::source_block) {
                if (saw_source || saw_map) {
                    fail(current, "A source block must precede map definitions and may appear once.");
                }
                fsm.source = current.text;
                saw_source = true;
            } else if (current.kind == TokenKind::directive) {
                if (current.text == "%map") {
                    if (fsm.start_state.empty() || fsm.context_class.empty()) {
                        fail(current, "Each map must be preceded by %start and %class declarations.");
                    }
                    fsm.maps.push_back(parse_map(current));
                    saw_map = true;
                } else {
                    if (saw_map) {
                        fail(current, "SMC declarations must precede map definitions.");
                    }
                    parse_directive(fsm, current);
                }
            } else {
                fail(current, "Expected an SMC directive or source block.");
            }
        }
    } catch (const ParseFailure&) {
        return std::nullopt;
    }

    fsm.end_location = peek().end_location;
    if (std::any_of(diagnostics_.begin(), diagnostics_.end(),
                    [](const Diagnostic& diagnostic) {
                        return diagnostic.severity == DiagnosticSeverity::error;
                    })) {
        return std::nullopt;
    }
    return fsm;
}

} // namespace smc
