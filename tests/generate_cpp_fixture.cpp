#include "smc/generator.hpp"
#include "smc/parser.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    if (argc != 4) return 2;
    std::ifstream input(argv[1]);
    if (!input) return 2;
    const std::string source((std::istreambuf_iterator<char>(input)), {});
    smc::Diagnostics diagnostics;
    smc::Parser parser(source, argv[1], diagnostics);
    auto fsm = parser.parse();
    if (!fsm) {
        for (const auto& diagnostic : diagnostics) std::cerr << diagnostic.message << '\n';
        return 1;
    }
    std::ofstream header(argv[2]), implementation(argv[3]);
    if (!header || !implementation) return 2;
    try {
        smc::CppHeaderGenerator header_generator(header, "cpp_fixture_generated.hpp");
        smc::CppSourceGenerator source_generator(implementation, "cpp_fixture_generated.hpp");
        fsm->accept(header_generator);
        fsm->accept(source_generator);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return header && implementation ? 0 : 1;
}
