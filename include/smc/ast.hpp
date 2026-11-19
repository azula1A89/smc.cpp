#pragma once

#include "smc/diagnostic.hpp"

#include <optional>
#include <string>
#include <vector>

namespace smc {

class Generator;

struct SourceRange {
    SourceLocation begin;
    SourceLocation end;
};

struct Guard {
    std::string condition;
    std::string end_state;
    std::string push_state;
    std::string pop_arguments;
    std::string transition_type{"set"};
    std::string actions;
    SourceLocation location;
    SourceLocation end_location;
    std::optional<SourceRange> end_state_range;
    std::optional<SourceRange> push_state_range;

    void accept(Generator& generator) const;
};

struct Transition {
    std::string name;
    std::string parameters;
    std::vector<Guard> guards;
    SourceLocation location;
    SourceLocation end_location;
    std::optional<SourceRange> name_range;

    void accept(Generator& generator) const;
};

struct State {
    std::string name;
    std::optional<std::string> entry_actions;
    std::optional<std::string> exit_actions;
    std::vector<Transition> transitions;
    SourceLocation location;
    SourceLocation end_location;
    std::optional<SourceRange> name_range;

    void accept(Generator& generator) const;
};

struct StateMap {
    std::string name;
    std::vector<State> states;
    SourceLocation location;
    SourceLocation end_location;
    std::optional<SourceRange> name_range;

    void accept(Generator& generator) const;
};

struct Fsm {
    std::string source_name;
    std::string start_state;
    std::string context_class;
    std::string fsm_class;
    std::string fsm_file;
    std::string header_file;
    std::string package_name;
    std::string access_level;
    std::string source;
    std::vector<std::string> includes;
    std::vector<std::string> imports;
    std::vector<std::string> declarations;
    std::vector<StateMap> maps;
    SourceLocation location;
    SourceLocation end_location;
    std::optional<SourceRange> start_state_range;

    void accept(Generator& generator) const;
};

} // namespace smc
