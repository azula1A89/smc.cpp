#pragma once

#include "smc/ast.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace smc::cpp_detail {

struct Parameter {
    std::string name;
    std::string type;
};

struct Action {
    std::string name;
    std::string suffix;
};

std::string trim(std::string_view text);
std::vector<Parameter> parameters(std::string_view text);
std::vector<Action> actions(std::string_view text);
std::string fsm_name(const Fsm& fsm);
std::string state_name(std::string_view name, std::string_view map);

} // namespace smc::cpp_detail
