#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace smc {

struct SourceLocation {
    std::string file;
    std::size_t line{1};
    std::size_t column{1};
};

enum class DiagnosticSeverity { warning, error };

struct Diagnostic {
    DiagnosticSeverity severity{DiagnosticSeverity::error};
    SourceLocation location;
    std::string message;
};

using Diagnostics = std::vector<Diagnostic>;

} // namespace smc
