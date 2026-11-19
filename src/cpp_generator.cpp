#include "smc/generator.hpp"

#include "cpp_generator_internal.hpp"
#include "smc/syntax_checker.hpp"

#include <algorithm>
#include <cctype>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace smc {
namespace {

using cpp_detail::Parameter;

struct TransitionGroup {
    const Transition* first;
    std::vector<const Guard*> guards;
};

std::vector<TransitionGroup> groups(const State& state) {
    std::vector<TransitionGroup> result;
    for (const auto& transition : state.transitions) {
        auto it = std::find_if(result.begin(), result.end(), [&](const auto& group) {
            return group.first->name == transition.name;
        });
        if (it == result.end()) {
            result.push_back({&transition, {}});
            it = result.end() - 1;
        }
        for (const auto& guard : transition.guards) it->guards.push_back(&guard);
    }
    return result;
}

std::vector<const Transition*> transitions(const Fsm& fsm) {
    std::vector<const Transition*> result;
    for (const auto& map : fsm.maps) {
        for (const auto& state : map.states) {
            for (const auto& transition : state.transitions) {
                if (transition.name != "Default" &&
                    std::none_of(result.begin(), result.end(), [&](const auto* existing) {
                        return existing->name == transition.name;
                    })) {
                    result.push_back(&transition);
                }
            }
        }
    }
    return result;
}

bool cpp_identifier(const std::string& value) {
    static const std::unordered_set<std::string> keywords{
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
        "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
        "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
        "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
        "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit",
        "export", "extern", "false", "float", "for", "friend", "goto", "if", "inline",
        "int", "long", "mutable", "namespace", "new", "noexcept", "not", "not_eq",
        "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
        "register", "reinterpret_cast", "requires", "return", "short", "signed",
        "sizeof", "static", "static_assert", "static_cast", "struct", "switch",
        "template", "this", "thread_local", "throw", "true", "try", "typedef",
        "typeid", "typename", "union", "unsigned", "using", "virtual", "void",
        "volatile", "wchar_t", "while", "xor", "xor_eq"
    };
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value.front())) ||
                           value.front() == '_') || keywords.contains(value)) return false;
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == '_';
    });
}

void require_identifier(const std::string& value) {
    if (!cpp_identifier(value)) throw std::invalid_argument("Invalid C++ identifier: " + value);
}

void validate(const Fsm& fsm, const std::string& header_name) {
    if (header_name.empty() || header_name.find_first_of("\"\\\r\n") != std::string::npos) {
        throw std::invalid_argument("Invalid generated header name");
    }
    Diagnostics diagnostics;
    SyntaxChecker checker(diagnostics);
    if (!checker.check(fsm, "c++")) {
        const auto& diagnostic = *std::find_if(diagnostics.begin(), diagnostics.end(),
            [](const Diagnostic& d) { return d.severity == DiagnosticSeverity::error; });
        throw std::invalid_argument(diagnostic.location.file + ":" +
                                    std::to_string(diagnostic.location.line) + ": " +
                                    diagnostic.message);
    }
    require_identifier(fsm.context_class);
    const auto name = cpp_detail::fsm_name(fsm);
    require_identifier(name);
    std::unordered_set<std::string> classes{fsm.context_class};
    if (!classes.insert(name).second || !classes.insert(fsm.context_class + "State").second) {
        throw std::invalid_argument("Conflicting generated C++ class names");
    }
    std::size_t begin = 0;
    while (begin < fsm.package_name.size()) {
        const auto end = fsm.package_name.find("::", begin);
        require_identifier(fsm.package_name.substr(begin, end - begin));
        if (end == std::string::npos) break;
        begin = end + 2;
        if (begin == fsm.package_name.size()) throw std::invalid_argument("Invalid C++ namespace");
    }
    for (const auto& map : fsm.maps) {
        require_identifier(map.name);
        if (!classes.insert(map.name).second || !classes.insert(map.name + "_Default").second) {
            throw std::invalid_argument("Conflicting generated C++ class name: " + map.name);
        }
        for (const auto& state : map.states) {
            require_identifier(state.name);
            if (state.name == "nil") throw std::invalid_argument("State name \"nil\" is reserved");
            if (state.name != "Default" &&
                (!classes.insert(map.name + "_" + state.name).second || state.name == map.name)) {
                throw std::invalid_argument("Conflicting generated C++ state name: " + state.name);
            }
            for (const auto& transition : state.transitions) {
                require_identifier(transition.name);
                if (transition.name == "Entry" || transition.name == "Exit" ||
                    transition.name == "getState" || transition.name == "getOwner" ||
                    transition.name == "enterStartState" || transition.name == name ||
                    transition.name == "clearState" || transition.name == "setState" ||
                    transition.name == "pushState" || transition.name == "popState" ||
                    transition.name == "emptyStateStack" || transition.name == "getPreviousState" ||
                    transition.name == "getStateStackDepth" || transition.name == "setTransition" ||
                    transition.name == "getTransition" || transition.name == "setDebugFlag" ||
                    transition.name == "getDebugFlag" || transition.name == "setDebugStream" ||
                    transition.name == "getDebugStream" || transition.name == map.name + "_" + state.name ||
                    transition.name == fsm.context_class + "State") {
                    throw std::invalid_argument("Reserved generated transition name: " + transition.name);
                }
                std::unordered_set<std::string> parameter_names;
                for (const auto& parameter : cpp_detail::parameters(transition.parameters)) {
                    require_identifier(parameter.name);
                    if (!parameter_names.insert(parameter.name).second) {
                        throw std::invalid_argument("Duplicate transition parameter: " + parameter.name);
                    }
                }
                for (const auto& guard : transition.guards) (void)cpp_detail::actions(guard.actions);
            }
            if (state.entry_actions) (void)cpp_detail::actions(*state.entry_actions);
            if (state.exit_actions) (void)cpp_detail::actions(*state.exit_actions);
        }
    }
}

std::string guard_name(const std::string& header_name) {
    std::string result = "SMC_GENERATED_";
    for (unsigned char ch : header_name) {
        result += std::isalnum(ch) ? static_cast<char>(std::toupper(ch)) : '_';
    }
    return result + "_INCLUDED";
}

std::string include_name(std::string value) {
    value = cpp_detail::trim(value);
    if (value.empty()) throw std::invalid_argument("Empty include name");
    if (value.front() == '<' || value.front() == '"') return value;
    return "\"" + value + "\"";
}

void open_namespace(std::ostream& out, const Fsm& fsm) {
    if (!fsm.package_name.empty()) out << "namespace " << fsm.package_name << " {\n\n";
}

void close_namespace(std::ostream& out, const Fsm& fsm) {
    if (!fsm.package_name.empty()) out << "\n} // namespace " << fsm.package_name << '\n';
}

void parameter_declarations(std::ostream& out, const std::vector<Parameter>& params,
                            bool leading_comma) {
    for (const auto& param : params) {
        if (leading_comma) out << ", ";
        out << "std::type_identity_t<" << param.type << "> " << param.name;
        leading_comma = true;
    }
}

void parameter_names(std::ostream& out, const std::vector<Parameter>& params) {
    for (const auto& param : params) out << ", " << param.name;
}

void declare_transition(std::ostream& out, const Transition& transition,
                        const std::string& fsm_class) {
    out << "    void " << transition.name << '(' << fsm_class << "& context";
    parameter_declarations(out, cpp_detail::parameters(transition.parameters), true);
    out << ") override;\n";
}

void emit_actions(std::ostream& out, std::string_view text, std::string_view indent) {
    for (const auto& action : cpp_detail::actions(text)) {
        out << indent;
        if (action.name == "emptyStateStack") out << "context.";
        else if (action.name.find("::") == std::string::npos) out << "ctxt.";
        out << action.name << action.suffix << '\n';
    }
}

void emit_completion(std::ostream& out, const Guard& guard, const StateMap& map,
                     std::string_view indent, bool loopback, bool has_actions) {
    const auto destination = loopback ? "smc_end_state" : cpp_detail::state_name(guard.end_state, map.name);
    if (guard.transition_type == "set" || guard.transition_type == "jump") {
        if (!loopback || has_actions) out << indent << "context.setState(" << destination << ");\n";
    } else if (guard.transition_type == "push") {
        if (!loopback || has_actions) out << indent << "context.setState(" << destination << ");\n";
        if (!loopback) out << indent << "context.getState().Entry(context);\n";
        out << indent << "context.pushState(" << cpp_detail::state_name(guard.push_state, map.name) << ");\n";
    } else if (guard.transition_type == "pop") {
        out << indent << "context.popState();\n";
    } else {
        throw std::invalid_argument("Unknown transition type: " + guard.transition_type);
    }
}

void emit_guard(std::ostream& out, const Guard& guard, const StateMap& map) {
    const bool loopback = guard.end_state == "nil" &&
        (guard.transition_type == "set" || guard.transition_type == "push" ||
         guard.transition_type == "jump");
    const bool has_actions = !cpp_detail::actions(guard.actions).empty();
    const std::string indent = "        ";
    if (loopback && has_actions) out << indent << "auto& smc_end_state = context.getState();\n";
    if (guard.transition_type == "pop" || !loopback) out << indent << "context.getState().Exit(context);\n";
    if (has_actions) {
        out << indent << "context.clearState();\n" << indent << "try {\n";
        emit_actions(out, guard.actions, "            ");
        emit_completion(out, guard, map, "            ", loopback, true);
        out << indent << "} catch (...) {\n";
        emit_completion(out, guard, map, "            ", loopback, true);
        out << "            throw;\n" << indent << "}\n";
    } else {
        emit_completion(out, guard, map, indent, loopback, false);
    }
    if (((guard.transition_type == "set" || guard.transition_type == "jump") && !loopback) ||
        guard.transition_type == "push") {
        out << indent << "context.getState().Entry(context);\n";
    }
    if (guard.transition_type == "pop" && !guard.end_state.empty() && guard.end_state != "nil") {
        out << indent << "context." << guard.end_state << '(' << guard.pop_arguments << ");\n";
    }
}

void generate_header(std::ostream& out, const Fsm& fsm, const std::string& header_name) {
    const auto name = cpp_detail::fsm_name(fsm);
    const auto state_base = fsm.context_class + "State";
    const auto all_transitions = transitions(fsm);
    const auto guard = guard_name(header_name);
    out << "// Generated by smc_cpp. Do not edit.\n#ifndef " << guard << "\n#define " << guard
        << "\n\n#if defined(_H_STATEMAP) && !defined(SMC_USES_IOSTREAMS)\n"
           "#error \"statemap.h must be configured with SMC_USES_IOSTREAMS\"\n#endif\n"
           "#if defined(SMC_NO_EXCEPTIONS) || defined(SMC_FIXED_STACK)\n"
           "#error \"Generated FSM requires exceptions and the dynamic state stack\"\n#endif\n"
           "#ifndef SMC_USES_IOSTREAMS\n#define SMC_USES_IOSTREAMS\n#endif\n"
           "#include <type_traits>\n#include <statemap.h>\n\n";
    open_namespace(out, fsm);
    out << "class " << fsm.context_class << ";\nclass " << name << ";\n";
    for (const auto& map : fsm.maps) {
        out << "class " << map.name << ";\n";
        for (const auto& state : map.states) {
            if (state.name != "Default") out << "class " << map.name << '_' << state.name << ";\n";
        }
    }
    for (const auto& declaration : fsm.declarations) out << declaration << '\n';
    out << "\nclass " << state_base << " : public statemap::State {\npublic:\n"
        << "    " << state_base << "(const char* name, int id) : statemap::State(name, id) {}\n"
        << "    virtual void Entry(" << name << "&) {}\n"
        << "    virtual void Exit(" << name << "&) {}\n";
    for (const auto* transition : all_transitions) {
        out << "    virtual void " << transition->name << '(' << name << "& context";
        parameter_declarations(out, cpp_detail::parameters(transition->parameters), true);
        out << ");\n";
    }
    out << "protected:\n    virtual void Default(" << name << "& context);\n};\n\n";
    for (const auto& map : fsm.maps) {
        out << "class " << map.name << " {\npublic:\n";
        for (const auto& state : map.states) {
            if (state.name != "Default")
                out << "    static " << map.name << '_' << state.name << ' ' << state.name << ";\n";
        }
        out << "};\n\nclass " << map.name << "_Default : public " << state_base << " {\npublic:\n"
            << "    " << map.name << "_Default(const char* name, int id) : "
            << state_base << "(name, id) {}\n";
        for (const auto& state : map.states) {
            if (state.name == "Default")
                for (const auto& group : groups(state)) declare_transition(out, *group.first, name);
        }
        out << "};\n\n";
        for (const auto& state : map.states) {
            if (state.name == "Default") continue;
            out << "class " << map.name << '_' << state.name << " : public " << map.name
                << "_Default {\npublic:\n    " << map.name << '_' << state.name
                << "(const char* name, int id) : " << map.name << "_Default(name, id) {}\n";
            if (state.entry_actions) out << "    void Entry(" << name << "& context) override;\n";
            if (state.exit_actions) out << "    void Exit(" << name << "& context) override;\n";
            for (const auto& group : groups(state)) declare_transition(out, *group.first, name);
            out << "};\n\n";
        }
    }
    out << "class " << name << " : public statemap::FSMContext {\npublic:\n"
        << "    explicit " << name << '(' << fsm.context_class << "& owner) : FSMContext("
        << fsm.start_state << "), owner_(owner) {}\n"
        << "    " << name << '(' << fsm.context_class
        << "& owner, const statemap::State& state) : FSMContext(state), owner_(owner) {}\n"
        << "    void enterStartState() override { getState().Entry(*this); }\n"
        << "    " << fsm.context_class << "& getOwner() { return owner_; }\n"
        << "    " << state_base << "& getState() {\n"
        << "        if (!_state) throw statemap::StateUndefinedException();\n"
        << "        return static_cast<" << state_base << "&>(*_state);\n    }\n";
    for (const auto* transition : all_transitions) {
        const auto params = cpp_detail::parameters(transition->parameters);
        out << "    void " << transition->name << '(';
        parameter_declarations(out, params, false);
        out << ") { getState()." << transition->name << "(*this";
        parameter_names(out, params);
        out << "); }\n";
    }
    out << "private:\n    " << fsm.context_class << "& owner_;\n};\n";
    close_namespace(out, fsm);
    out << "\n#endif // " << guard << '\n';
}

void generate_source(std::ostream& out, const Fsm& fsm, const std::string& header_name) {
    const auto name = cpp_detail::fsm_name(fsm);
    const auto state_base = fsm.context_class + "State";
    out << "// Generated by smc_cpp. Do not edit.\n" << fsm.source << '\n';
    for (const auto& include : fsm.includes) out << "#include " << include_name(include) << '\n';
    out << "#include " << include_name(fsm.header_file) << '\n'
        << "#include \"" << header_name << "\"\n\n";
    for (const auto& import : fsm.imports) out << "using namespace " << import << ";\n";
    open_namespace(out, fsm);
    int id = 0;
    for (const auto& map : fsm.maps) {
        for (const auto& state : map.states) {
            if (state.name != "Default")
                out << map.name << '_' << state.name << ' ' << map.name << "::" << state.name
                    << "(\"" << map.name << "::" << state.name << "\", " << id++ << ");\n";
        }
    }
    for (const auto* transition : transitions(fsm)) {
        out << "\nvoid " << state_base << "::" << transition->name << '(' << name << "& context";
        parameter_declarations(out, cpp_detail::parameters(transition->parameters), true);
        out << ") { Default(context); }\n";
    }
    out << "\nvoid " << state_base << "::Default(" << name << "& context) {\n"
        << "    throw statemap::TransitionUndefinedException(context.getState().getName(), "
           "context.getTransition());\n}\n";
    for (const auto& map : fsm.maps) {
        for (const auto& state : map.states) {
            const auto cls = map.name + "_" + state.name;
            const auto base = state.name == "Default" ? state_base : map.name + "_Default";
            const auto state_action = [&](std::string_view method, const std::string& actions) {
                out << "\nvoid " << cls << "::" << method << '(' << name << "& context) {\n"
                    << "    [[maybe_unused]] auto& ctxt = context.getOwner();\n";
                emit_actions(out, actions, "    ");
                out << "}\n";
            };
            if (state.entry_actions) state_action("Entry", *state.entry_actions);
            if (state.exit_actions) state_action("Exit", *state.exit_actions);
            for (const auto& group : groups(state)) {
                const auto params = cpp_detail::parameters(group.first->parameters);
                out << "\nvoid " << cls << "::" << group.first->name << '(' << name << "& context";
                parameter_declarations(out, params, true);
                out << ") {\n    [[maybe_unused]] auto& ctxt = context.getOwner();\n";
                bool unconditional = false;
                for (const auto* guard : group.guards) {
                    if (guard->condition.empty()) {
                        out << "    {\n";
                        unconditional = true;
                    } else {
                        out << "    if (" << guard->condition << ") {\n";
                    }
                    emit_guard(out, *guard, map);
                    out << "        return;\n    }\n";
                }
                if (!unconditional) {
                    out << "    " << base << "::" << group.first->name << "(context";
                    parameter_names(out, params);
                    out << ");\n";
                }
                out << "}\n";
            }
        }
    }
    close_namespace(out, fsm);
}

void require_fsm() {
    throw std::logic_error("C++ generators must be invoked on a complete Fsm");
}

} // namespace

CppHeaderGenerator::CppHeaderGenerator(std::ostream& output, std::string header_name)
    : output_(output), header_name_(std::move(header_name)) {}
void CppHeaderGenerator::visit(const Fsm& fsm) {
    validate(fsm, header_name_);
    generate_header(output_, fsm, header_name_);
    if (!output_) throw std::ios_base::failure("Failed to write C++ header");
}
void CppHeaderGenerator::visit(const StateMap&) { require_fsm(); }
void CppHeaderGenerator::visit(const State&) { require_fsm(); }
void CppHeaderGenerator::visit(const Transition&) { require_fsm(); }
void CppHeaderGenerator::visit(const Guard&) { require_fsm(); }

CppSourceGenerator::CppSourceGenerator(std::ostream& output, std::string header_name)
    : output_(output), header_name_(std::move(header_name)) {}
void CppSourceGenerator::visit(const Fsm& fsm) {
    validate(fsm, header_name_);
    generate_source(output_, fsm, header_name_);
    if (!output_) throw std::ios_base::failure("Failed to write C++ source");
}
void CppSourceGenerator::visit(const StateMap&) { require_fsm(); }
void CppSourceGenerator::visit(const State&) { require_fsm(); }
void CppSourceGenerator::visit(const Transition&) { require_fsm(); }
void CppSourceGenerator::visit(const Guard&) { require_fsm(); }

} // namespace smc
