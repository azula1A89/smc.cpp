#include "smc/generator.hpp"
#include "smc/lexer.hpp"
#include "smc/parser.hpp"
#include "smc/syntax_checker.hpp"

#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

const char* valid_source = R"SMC(%{ raw source { and nested } %}
%start Main::Idle
%class Context
%fsmclass ContextFSM
%header context.h
%include <vector>
%map Main
%%
Idle
Entry { initialize(); }
{
    start(value: int) [value > 0] Active { set(value); }
                            [value <= 0] nil { }
}
Active
{
    stop nil { }
    reset Main::Idle { }
}
%%
)SMC";

void test_lexer_preserves_source_and_comments() {
    smc::Diagnostics diagnostics;
    smc::Lexer lexer("%{ raw { body } %} /* outer /* nested */ comment */ %%", "lexer.sm",
                     diagnostics);
    const auto source = lexer.next();
    const auto delimiter = lexer.next();
    const auto end = lexer.next();

    assert(source.kind == smc::TokenKind::source_block);
    assert(source.text == " raw { body } ");
    assert(delimiter.kind == smc::TokenKind::double_percent);
    assert(end.kind == smc::TokenKind::end);
    assert(diagnostics.empty());
}

void test_token_and_ast_end_locations() {
    smc::Diagnostics token_diagnostics;
    smc::Lexer lexer("word{}", "positions.sm", token_diagnostics);
    const auto word = lexer.next();
    const auto open = lexer.next();
    const auto close = lexer.next();
    assert(word.location.column == 1 && word.end_location.column == 5);
    assert(open.location.column == 5 && open.end_location.column == 6);
    assert(close.location.column == 6 && close.end_location.column == 7);
    assert(token_diagnostics.empty());

    const std::string source = R"SMC(%start Main::state_x
%class Context
%map Main
%%
state_x
{
    transition_xy state_y{}
}

state_y
{
}
%%
)SMC";
    smc::Diagnostics diagnostics;
    smc::Parser parser(source, "positions.sm", diagnostics);
    const auto fsm = parser.parse();
    assert(fsm.has_value());

    const auto& map = fsm->maps.front();
    const auto& state = map.states.front();
    const auto& transition = state.transitions.front();
    const auto& guard = transition.guards.front();
    assert(state.location.line == 5 && state.end_location.line == 8 &&
           state.end_location.column == 2);
    assert(transition.location.line == 7 &&
           transition.end_location.line == 7 &&
           transition.end_location.column ==
               std::string("    transition_xy state_y{}").size() + 1);
    assert(guard.location.line == 7 &&
           guard.end_location.line == transition.end_location.line &&
           guard.end_location.column == transition.end_location.column);
    assert(map.end_location.line == 13 && map.end_location.column == 3);
    assert(diagnostics.empty());
}

void test_parser_and_semantic_checker() {
    smc::Diagnostics diagnostics;
    smc::Parser parser(valid_source, "valid.sm", diagnostics);
    const auto fsm = parser.parse();

    assert(fsm.has_value());
    assert(fsm->maps.size() == 1);
    assert(fsm->maps.front().states.size() == 3);
    assert(fsm->maps.front().states.front().transitions.front().guards.size() == 2);
    assert(fsm->maps.front().states.front().entry_actions.has_value());

    smc::SyntaxChecker checker(diagnostics);
    assert(checker.check(*fsm));
    assert(diagnostics.empty());
}

void test_checker_reports_missing_destination() {
    const std::string source = R"SMC(%start Main::Ready
%class Context
%header context.h
%map Main
%%
Ready { open Missing {} }
%%
)SMC";
    smc::Diagnostics diagnostics;
    smc::Parser parser(source, "invalid.sm", diagnostics);
    const auto fsm = parser.parse();
    assert(fsm.has_value());

    smc::SyntaxChecker checker(diagnostics);
    assert(!checker.check(*fsm));
    assert(!diagnostics.empty());
}

void test_default_state_entry_is_rejected() {
    const std::string source = R"SMC(%start Main::Ready
%class Context
%header context.h
%map Main
%%
Ready { begin nil {} }
Default Entry {} { }
%%
)SMC";
    smc::Diagnostics diagnostics;
    smc::Parser parser(source, "default.sm", diagnostics);
    const auto fsm = parser.parse();
    assert(fsm.has_value());

    smc::SyntaxChecker checker(diagnostics);
    assert(!checker.check(*fsm));
}

void test_duplicate_unconditional_transition_is_rejected() {
    const std::string source = R"SMC(%start Main::Ready
%class Context
%header context.h
%map Main
%%
Ready
{
    open Running { }
    open nil { }
}
Running { }
%%
)SMC";
    smc::Diagnostics diagnostics;
    smc::Parser parser(source, "duplicate.sm", diagnostics);
    const auto fsm = parser.parse();
    assert(fsm.has_value());

    smc::SyntaxChecker checker(diagnostics);
    assert(!checker.check(*fsm));
}

void test_parser_rejects_malformed_typed_parameter_and_action() {
    const std::string malformed_parameter = R"SMC(%start Main::Ready
%class Context
%header context.h
%map Main
%%
Ready { open(value) nil {} }
%%
)SMC";
    smc::Diagnostics parameter_diagnostics;
    smc::Parser parameter_parser(malformed_parameter, "parameter.sm",
                                 parameter_diagnostics);
    assert(!parameter_parser.parse().has_value());
    assert(!parameter_diagnostics.empty());

    const std::string malformed_action = R"SMC(%start Main::Ready
%class Context
%header context.h
%map Main
%%
Ready { open nil { call() } }
%%
)SMC";
    smc::Diagnostics action_diagnostics;
    smc::Parser action_parser(malformed_action, "action.sm", action_diagnostics);
    assert(!action_parser.parse().has_value());
    assert(!action_diagnostics.empty());
}

void test_cross_map_destination_and_nested_parameter_type() {
    const std::string source = R"SMC(%start Root::Idle
%class Context
%header context.h
%map Root
%%
Idle { switch(value: std::pair<int, std::string>) Target::Ready { set([] { return "}"; }); } }
%%
%map Target
%%
Ready { reset Root::Idle {} }
%%
)SMC";
    smc::Diagnostics diagnostics;
    smc::Parser parser(source, "cross-map.sm", diagnostics);
    const auto fsm = parser.parse();
    assert(fsm.has_value());

    smc::SyntaxChecker checker(diagnostics);
    assert(checker.check(*fsm));
    assert(diagnostics.empty());
}

void test_pop_and_generator_diagnostics() {
    const std::string source = R"SMC(%start Main::Ready
%class Context
%header context.h
%map Main
%%
Ready { back pop(done, make(1, 2)) {} done(value: int) nil {} }
%%
)SMC";
    smc::Diagnostics diagnostics;
    smc::Parser parser(source, "machine.sm", diagnostics);
    auto fsm = parser.parse();
    assert(fsm);
    const auto& guard = fsm->maps.front().states.front().transitions.front().guards.front();
    assert(guard.end_state == "done");
    assert(guard.pop_arguments == "make(1, 2)");
    smc::SyntaxChecker checker(diagnostics);
    assert(checker.check(*fsm));

    fsm->maps.front().states.front().transitions.front().name = "switch";
    std::ostringstream output;
    smc::CppHeaderGenerator generator(output, "machine.hpp");
    try {
        fsm->accept(generator);
        assert(false);
    } catch (const std::invalid_argument&) {
        assert(output.str().empty());
    }

    fsm->maps.front().states.front().transitions.front().name = "back";
    output.setstate(std::ios::badbit);
    try {
        fsm->accept(generator);
        assert(false);
    } catch (const std::ios_base::failure&) {}
}

void test_conflicting_transition_parameters_are_rejected() {
    const std::string source = R"SMC(%start Main::Ready
%class Context
%header context.h
%map Main
%%
Ready { run(a: int) [a > 0] nil {} run(b: int) nil {} }
%%
)SMC";
    smc::Diagnostics diagnostics;
    smc::Parser parser(source, "conflict.sm", diagnostics);
    const auto fsm = parser.parse();
    assert(fsm);
    smc::SyntaxChecker checker(diagnostics);
    assert(!checker.check(*fsm));
}

} // namespace

int main() {
    test_lexer_preserves_source_and_comments();
    test_token_and_ast_end_locations();
    test_parser_and_semantic_checker();
    test_checker_reports_missing_destination();
    test_default_state_entry_is_rejected();
    test_duplicate_unconditional_transition_is_rejected();
    test_parser_rejects_malformed_typed_parameter_and_action();
    test_cross_map_destination_and_nested_parameter_type();
    test_pop_and_generator_diagnostics();
    test_conflicting_transition_parameters_are_rejected();
}
