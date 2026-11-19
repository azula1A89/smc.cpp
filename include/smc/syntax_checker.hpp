#pragma once

#include "smc/ast.hpp"

#include <string_view>

namespace smc {

class SyntaxChecker {
public:
    explicit SyntaxChecker(Diagnostics& diagnostics);

    bool check(const Fsm& fsm, std::string_view target_language = "c++");

private:
    void report(const SourceLocation& location, std::string message);
    Diagnostics& diagnostics_;
};

} // namespace smc
