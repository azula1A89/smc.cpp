#include "smc/generator.hpp"
#include "smc/parser.hpp"
#include "smc/syntax_checker.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_usage(std::ostream& output) {
    output << "Usage: smc_cpp -c++ [-d OUTPUT_DIR] [--check] INPUT.sm\n"
              "  --check  lex, parse, and validate without invoking code generation\n";
}

void print_diagnostics(const smc::Diagnostics& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        const char* severity = diagnostic.severity == smc::DiagnosticSeverity::error
                                   ? "error"
                                   : "warning";
        std::cerr << diagnostic.location.file << ':' << diagnostic.location.line << ':'
                  << diagnostic.location.column << ": " << severity << ": "
                  << diagnostic.message << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string input_file;
    std::string output_directory;
    bool check_only = false;
    bool cpp_target = false;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "-c++") {
            cpp_target = true;
        } else if (argument == "--check") {
            check_only = true;
        } else if (argument == "-d") {
            if (index + 1 >= argc) {
                std::cerr << "smc_cpp: -d requires an output directory\n";
                return 2;
            }
            output_directory = argv[++index];
        } else if (!argument.empty() && argument.front() == '-') {
            std::cerr << "smc_cpp: unknown option '" << argument << "'\n";
            print_usage(std::cerr);
            return 2;
        } else if (input_file.empty()) {
            input_file = argument;
        } else {
            std::cerr << "smc_cpp: only one input .sm file is supported\n";
            return 2;
        }
    }

    if (!cpp_target || input_file.empty()) {
        print_usage(std::cerr);
        return 2;
    }

    std::ifstream input(input_file, std::ios::binary);
    if (!input) {
        std::cerr << "smc_cpp: cannot open input file '" << input_file << "'\n";
        return 2;
    }
    const std::string contents((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
    if (input.bad()) {
        std::cerr << "smc_cpp: failed while reading input file '" << input_file << "'\n";
        return 2;
    }

    smc::Diagnostics diagnostics;
    smc::Parser parser(contents, input_file, diagnostics);
    const auto fsm = parser.parse();
    if (!fsm) {
        print_diagnostics(diagnostics);
        return 1;
    }

    smc::SyntaxChecker checker(diagnostics);
    if (!checker.check(*fsm, "c++")) {
        print_diagnostics(diagnostics);
        return 1;
    }

    print_diagnostics(diagnostics);
    if (check_only) {
        return 0;
    }

    const std::filesystem::path input_path(input_file);
    const auto output_path = output_directory.empty()
                                 ? input_path.parent_path()
                                 : std::filesystem::path(output_directory);
    const auto stem = input_path.stem().string();
    const auto header_path = output_path / (stem + ".hpp");
    const auto source_path = output_path / (stem + ".cpp");
    const auto header_name = header_path.filename().string();

    std::ostringstream header_contents;
    std::ostringstream source_contents;
    try {
        smc::CppHeaderGenerator header_generator(header_contents, header_name);
        smc::CppSourceGenerator source_generator(source_contents, header_name);
        fsm->accept(header_generator);
        fsm->accept(source_generator);
    } catch (const std::exception& error) {
        std::cerr << "smc_cpp: C++ generation failed: " << error.what() << '\n';
        return 1;
    }

    const auto write_output = [](const std::filesystem::path& path,
                                 const std::string& contents) {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) {
            std::cerr << "smc_cpp: cannot open output file '" << path.string() << "'\n";
            return false;
        }
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        output.flush();
        if (!output) {
            std::cerr << "smc_cpp: failed while writing output file '" << path.string()
                      << "'\n";
            return false;
        }
        return true;
    };

    if (!write_output(header_path, header_contents.str()) ||
        !write_output(source_path, source_contents.str())) {
        return 2;
    }
    return 0;
}
