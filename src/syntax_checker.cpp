#include "smc/syntax_checker.hpp"

#include "cpp_generator_internal.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace smc {
namespace {

bool is_error(const Diagnostic& diagnostic) {
    return diagnostic.severity == DiagnosticSeverity::error;
}

bool is_default_target(std::string_view state) {
    const auto separator = state.rfind("::");
    if (separator != std::string_view::npos) state.remove_prefix(separator + 2);
    std::string normalized(state);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return normalized == "default";
}

bool is_default_state(std::string_view state) {
    return state == "Default";
}

} // namespace

SyntaxChecker::SyntaxChecker(Diagnostics& diagnostics) : diagnostics_(diagnostics) {}

void SyntaxChecker::report(const SourceLocation& location, std::string message) {
    diagnostics_.push_back(Diagnostic{DiagnosticSeverity::error, location, std::move(message)});
}

bool SyntaxChecker::check(const Fsm& fsm, std::string_view target_language) {
    std::unordered_map<std::string_view, std::unordered_set<std::string_view>> state_index;
    std::unordered_map<std::string, std::vector<std::string>> signatures;
    for (const auto& map : fsm.maps) {
        auto [entry, inserted] = state_index.try_emplace(map.name);
        if (inserted)
            for (const auto& state : map.states)
                entry->second.insert(state.name);
    }
    const auto state_exists = [&](std::string_view qualified_state,
                                  std::string_view current_map) {
        if (qualified_state == "nil")
            return true;
        const auto separator = qualified_state.find("::");
        const auto map_name = separator == std::string_view::npos
            ? current_map : qualified_state.substr(0, separator);
        const auto state_name = separator == std::string_view::npos
            ? qualified_state : qualified_state.substr(separator + 2);
        const auto map = state_index.find(map_name);
        return map != state_index.end() && map->second.contains(state_name);
    };
    if (fsm.start_state.empty()) {
        report(fsm.location, "\"%start\" missing.");
    }
    if (fsm.context_class.empty()) {
        report(fsm.location, "\"%class\" missing.");
    }
    if (target_language == "c++" && fsm.header_file.empty()) {
        report(fsm.location, "\"%header\" missing for C++ generation.");
    }
    if (fsm.maps.empty()) {
        report(fsm.location, "No state maps were defined.");
    }

    const auto separator = fsm.start_state.find("::");
    if (separator == std::string::npos || separator == 0 ||
        separator + 2 >= fsm.start_state.size() ||
        fsm.start_state.find("::", separator + 2) != std::string::npos) {
        report(fsm.location, "Start state must be of the form \"map::state\".");
    } else if (!state_exists(fsm.start_state, {})) {
        report(fsm.location, "Start state \"" + fsm.start_state + "\" does not exist.");
    } else if (fsm.start_state.substr(separator + 2) == "Default") {
        report(fsm.location, "Default state cannot be the start state.");
    }

    std::unordered_set<std::string> map_names;
    for (const auto& map : fsm.maps) {
        if (!map_names.insert(map.name).second) {
            report(map.location, "Duplicate map name \"" + map.name + "\".");
        }

        std::unordered_set<std::string> state_names;
        for (const auto& state : map.states) {
            if (!state_names.insert(state.name).second) {
                report(state.location, "Duplicate state name \"" + state.name + "\".");
            }
            if (is_default_state(state.name) &&
                (state.entry_actions.has_value() || state.exit_actions.has_value())) {
                report(state.location, "Default state may not have Entry or Exit actions.");
            }

            std::unordered_map<std::string, std::unordered_set<std::string>> guards_by_transition;
            std::unordered_map<std::string, std::vector<std::string>> parameter_names;
            std::unordered_set<std::string> unconditional;
            for (const auto& transition : state.transitions) {
                if (target_language == "c++") {
                    try {
                        std::vector<std::string> types;
                        std::vector<std::string> names;
                        for (const auto& parameter : cpp_detail::parameters(transition.parameters)) {
                            types.push_back(parameter.type);
                            names.push_back(parameter.name);
                            if (parameter.name == "context" || parameter.name == "ctxt" ||
                                parameter.name == "smc_end_state") {
                                report(transition.location, "Parameter name \"" + parameter.name +
                                                            "\" is reserved for C++ generation.");
                            }
                        }
                        const auto [names_it, names_inserted] = parameter_names.emplace(transition.name, names);
                        if (!names_inserted && names_it->second != names) {
                            report(transition.location, "Inconsistent parameter names for transition \"" +
                                                        transition.name + "\" in the same state.");
                        }
                        const auto [it, inserted] = signatures.emplace(transition.name, types);
                        if (!inserted && it->second != types) {
                            report(transition.location, "Conflicting parameter types for transition \"" +
                                                            transition.name + "\".");
                        }
                    } catch (const std::invalid_argument& error) {
                        report(transition.location, error.what());
                    }
                }
                for (const auto& guard : transition.guards) {
                    if (unconditional.contains(transition.name)) {
                        report(guard.location, "Guard follows an unconditional transition \"" +
                                                   transition.name + "\".");
                    }
                    if (guard.condition.empty()) unconditional.insert(transition.name);
                    if (!guards_by_transition[transition.name].insert(guard.condition).second) {
                        report(guard.location, "Duplicate guard for transition \"" +
                                                   transition.name + "\".");
                    }

                    if (guard.transition_type != "pop" &&
                        is_default_target(guard.end_state)) {
                        report(guard.location, "May not transition to the default state.");
                    } else if (guard.transition_type != "pop" &&
                               !state_exists(guard.end_state, map.name)) {
                        report(guard.location, "No such state as \"" + guard.end_state + "\".");
                    }

                    if (guard.transition_type == "push") {
                        if (guard.push_state == "nil") {
                            report(guard.location, "May not push to nil state.");
                        } else if (is_default_target(guard.push_state)) {
                            report(guard.location, "May not push to the default state.");
                        } else if (!state_exists(guard.push_state, map.name)) {
                            report(guard.location,
                                   "No such state as \"" + guard.push_state + "\".");
                        }
                    }
                }
            }
        }
    }

    if (target_language == "c++") {
        for (const auto& map : fsm.maps) {
            for (const auto& state : map.states) {
                for (const auto& transition : state.transitions) {
                    for (const auto& guard : transition.guards) {
                        if (guard.transition_type == "pop" && !guard.end_state.empty() &&
                            guard.end_state != "nil" &&
                            (guard.end_state == "Default" || !signatures.contains(guard.end_state))) {
                            report(guard.location, "Unknown pop transition \"" + guard.end_state + "\".");
                        }
                    }
                }
            }
        }
    }

    return std::none_of(diagnostics_.begin(), diagnostics_.end(), is_error);
}

} // namespace smc
